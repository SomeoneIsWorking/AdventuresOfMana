#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

struct SDL_AudioStream;   // global scope: SDL's type, not mcf's

namespace mcf {

// SDL3 audio. Sound effects are `sk1/SE%04d.wav` inside the MPK (ids 1..176,
// contiguous). Music is `assets/bgm%03d*.ogg` from the APK, NOT the MPK, in two
// banks: 1..30 and 101..130 (original and arranged soundtracks).
class Audio {
public:
    ~Audio();
    bool Init();
    bool ok() const { return dev_ != 0; }

    // `wav` is a whole RIFF file. Returns false and logs if it will not decode.
    bool PlaySe(int id, std::span<const uint8_t> wav, bool loop);
    void StopSe(int id);
    void StopAllSe();

    // Decodes the whole track; one BGM plays at a time so the memory is bounded.
    bool PlayBgm(int id, const std::string& ogg_path, bool loop);
    int  bgm_id() const { return bgm_id_; }

    // Refills looping streams. Call once per frame.
    void Update();

    struct Stat { int decoded_sounds = 0; uint64_t decoded_frames = 0; };
    Stat stat;

private:
    struct Voice {
        ::SDL_AudioStream* stream = nullptr;
        std::vector<uint8_t> pcm;
        bool loop = false;
    };
    uint32_t dev_ = 0;
    std::map<int, Voice> se_;
    Voice bgm_;
    int bgm_id_ = -1;
};

}  // namespace mcf
