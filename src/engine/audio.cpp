#include "engine/audio.h"

#include <SDL3/SDL.h>
#include <vorbis/vorbisfile.h>

#include <cstring>
#include <format>

#include <lucent/log.h>

namespace mcf {
namespace {
constexpr SDL_AudioSpec kOut{SDL_AUDIO_S16LE, 2, 48000};
}

Audio::~Audio() {
    if (dev_) SDL_CloseAudioDevice(dev_);
}

bool Audio::Init() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        lucent::warn("audio", "SDL_InitSubSystem: {}", SDL_GetError());
        return false;
    }
    dev_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &kOut);
    if (!dev_) {
        // Not fatal: headless runs have no audio device, and the port should
        // still run. Say so rather than failing silently.
        lucent::warn("audio", "no playback device ({}); audio disabled", SDL_GetError());
        return false;
    }
    lucent::info("audio", "device open: {} Hz, {} ch, S16", kOut.freq, kOut.channels);
    return true;
}

bool Audio::PlaySe(int id, std::span<const uint8_t> wav, bool loop) {
    if (!dev_) return false;
    SDL_AudioSpec spec{};
    uint8_t* buf = nullptr;
    uint32_t len = 0;
    SDL_IOStream* io = SDL_IOFromConstMem(wav.data(), wav.size());
    if (!io || !SDL_LoadWAV_IO(io, true, &spec, &buf, &len)) {
        lucent::warn("audio", "SE{:04d}: {}", id, SDL_GetError());
        return false;
    }
    StopSe(id);
    Voice v;
    v.pcm.assign(buf, buf + len);
    v.loop = loop;
    SDL_free(buf);
    v.stream = SDL_CreateAudioStream(&spec, &kOut);
    if (!v.stream) return false;
    SDL_BindAudioStream(dev_, v.stream);
    SDL_PutAudioStreamData(v.stream, v.pcm.data(), int(v.pcm.size()));
    ++stat.decoded_sounds;
    stat.decoded_frames += len / (SDL_AUDIO_BYTESIZE(spec.format) * spec.channels);
    se_[id] = std::move(v);
    return true;
}

void Audio::StopSe(int id) {
    auto it = se_.find(id);
    if (it == se_.end()) return;
    if (it->second.stream) {
        SDL_UnbindAudioStream(it->second.stream);
        SDL_DestroyAudioStream(it->second.stream);
    }
    se_.erase(it);
}

void Audio::StopAllSe() {
    while (!se_.empty()) StopSe(se_.begin()->first);
}

bool Audio::PlayBgm(int id, const std::string& path, bool loop) {
    if (!dev_) return false;
    if (bgm_id_ == id) return true;

    OggVorbis_File vf;
    if (ov_fopen(path.c_str(), &vf) != 0) {
        lucent::warn("audio", "bgm {}: cannot open {}", id, path);
        return false;
    }
    vorbis_info* vi = ov_info(&vf, -1);
    SDL_AudioSpec spec{SDL_AUDIO_S16LE, vi->channels, int(vi->rate)};

    std::vector<uint8_t> pcm;
    char tmp[8192];
    int bs = 0;
    for (;;) {
        long n = ov_read(&vf, tmp, sizeof tmp, 0, 2, 1, &bs);
        if (n <= 0) break;
        pcm.insert(pcm.end(), tmp, tmp + n);
    }
    ov_clear(&vf);
    if (pcm.empty()) {
        lucent::warn("audio", "bgm {}: decoded 0 bytes from {}", id, path);
        return false;
    }

    if (bgm_.stream) {
        SDL_UnbindAudioStream(bgm_.stream);
        SDL_DestroyAudioStream(bgm_.stream);
    }
    bgm_.pcm = std::move(pcm);
    bgm_.loop = loop;
    bgm_.stream = SDL_CreateAudioStream(&spec, &kOut);
    if (!bgm_.stream) return false;
    SDL_BindAudioStream(dev_, bgm_.stream);
    SDL_PutAudioStreamData(bgm_.stream, bgm_.pcm.data(), int(bgm_.pcm.size()));
    bgm_id_ = id;
    ++stat.decoded_sounds;
    uint64_t frames = bgm_.pcm.size() / (2ull * uint64_t(spec.channels));
    stat.decoded_frames += frames;
    lucent::info("audio", "bgm {}: {} Hz, {} ch, {:.1f} s", id, spec.freq,
                 spec.channels, double(frames) / spec.freq);
    return true;
}

void Audio::Update() {
    if (!dev_) return;
    if (bgm_.stream && bgm_.loop &&
        SDL_GetAudioStreamAvailable(bgm_.stream) < int(bgm_.pcm.size() / 8))
        SDL_PutAudioStreamData(bgm_.stream, bgm_.pcm.data(), int(bgm_.pcm.size()));
    for (auto& [id, v] : se_)
        if (v.stream && v.loop && SDL_GetAudioStreamAvailable(v.stream) == 0)
            SDL_PutAudioStreamData(v.stream, v.pcm.data(), int(v.pcm.size()));
}

}  // namespace mcf
