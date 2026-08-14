#include "host/gpu_device.h"

#include <array>
#include <cstring>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <lucent/log.h>

namespace mana::gpu {
namespace {

[[noreturn]] void Fail(std::string_view operation) {
  throw std::runtime_error(std::format("{}: {}", operation, SDL_GetError()));
}

struct ReadbackResources {
  SDL_GPUDevice *device = nullptr;
  SDL_GPUTexture *texture = nullptr;
  SDL_GPUTransferBuffer *transfer = nullptr;

  ~ReadbackResources() {
    if (transfer)
      SDL_ReleaseGPUTransferBuffer(device, transfer);
    if (texture)
      SDL_ReleaseGPUTexture(device, texture);
  }
};

} // namespace

Device::Device() {
  if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
      Fail("SDL_InitSubSystem(SDL_INIT_VIDEO)");
    owns_video_ = true;
  }
  constexpr SDL_GPUShaderFormat kPortableShaderFormats =
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
      SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB;
  device_ = SDL_CreateGPUDevice(kPortableShaderFormats, false, nullptr);
  if (!device_) {
    if (owns_video_)
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    Fail("SDL_CreateGPUDevice");
  }
}

Device::~Device() {
  if (!device_)
    return;
  if (!SDL_WaitForGPUIdle(device_))
    lucent::error("gpu", "SDL_WaitForGPUIdle during shutdown: {}",
                  SDL_GetError());
  SDL_DestroyGPUDevice(device_);
  if (owns_video_)
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

const char *Device::driver() const {
  const char *name = SDL_GetGPUDeviceDriver(device_);
  return name ? name : "unknown";
}

SDL_GPUShaderFormat Device::shader_formats() const {
  return SDL_GetGPUShaderFormats(device_);
}

std::vector<std::uint8_t> Device::ClearAndReadback(std::uint32_t width,
                                                   std::uint32_t height,
                                                   SDL_FColor color) {
  if (width == 0 || height == 0)
    throw std::invalid_argument("GPU readback dimensions must be nonzero");
  const std::uint64_t byte_count = std::uint64_t(width) * height * 4;
  if (byte_count > std::numeric_limits<std::uint32_t>::max())
    throw std::invalid_argument(
        "GPU readback exceeds SDL transfer-buffer limits");
  const std::uint32_t bytes = static_cast<std::uint32_t>(byte_count);
  ReadbackResources resources{.device = device_};

  const SDL_GPUTextureCreateInfo texture_info{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
      .width = width,
      .height = height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
  };
  resources.texture = SDL_CreateGPUTexture(device_, &texture_info);
  if (!resources.texture)
    Fail("SDL_CreateGPUTexture");

  const SDL_GPUTransferBufferCreateInfo transfer_info{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
      .size = bytes,
  };
  resources.transfer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
  if (!resources.transfer)
    Fail("SDL_CreateGPUTransferBuffer");

  SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device_);
  if (!command)
    Fail("SDL_AcquireGPUCommandBuffer");
  const SDL_GPUColorTargetInfo target{
      .texture = resources.texture,
      .clear_color = color,
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE,
  };
  SDL_GPURenderPass *render =
      SDL_BeginGPURenderPass(command, &target, 1, nullptr);
  if (!render) {
    SDL_CancelGPUCommandBuffer(command);
    Fail("SDL_BeginGPURenderPass");
  }
  SDL_EndGPURenderPass(render);

  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
  if (!copy) {
    SDL_CancelGPUCommandBuffer(command);
    Fail("SDL_BeginGPUCopyPass");
  }
  const SDL_GPUTextureRegion region{
      .texture = resources.texture,
      .w = width,
      .h = height,
      .d = 1,
  };
  const SDL_GPUTextureTransferInfo destination{
      .transfer_buffer = resources.transfer,
      .pixels_per_row = width,
      .rows_per_layer = height,
  };
  SDL_DownloadFromGPUTexture(copy, &region, &destination);
  SDL_EndGPUCopyPass(copy);

  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
  if (!fence)
    Fail("SDL_SubmitGPUCommandBufferAndAcquireFence");
  if (!SDL_WaitForGPUFences(device_, true, &fence, 1)) {
    SDL_ReleaseGPUFence(device_, fence);
    Fail("SDL_WaitForGPUFences");
  }
  SDL_ReleaseGPUFence(device_, fence);

  void *mapped = SDL_MapGPUTransferBuffer(device_, resources.transfer, false);
  if (!mapped)
    Fail("SDL_MapGPUTransferBuffer");
  std::vector<std::uint8_t> pixels(bytes);
  std::memcpy(pixels.data(), mapped, pixels.size());
  SDL_UnmapGPUTransferBuffer(device_, resources.transfer);
  return pixels;
}

int RunDeviceSelfTest() {
  int bad = 0;
  const bool video_before = (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != 0;
  const bool audio_before = (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0;
  {
    Device device;
    lucent::info("gpu",
                 "SDL3 GPU driver: {}; shader formats: 0x{:x}; "
                 "video driver: {}",
                 device.driver(), device.shader_formats(),
                 SDL_GetCurrentVideoDriver());

    int window_count = -1;
    SDL_Window **windows = SDL_GetWindows(&window_count);
    SDL_free(windows);
    if (window_count != 0) {
      ++bad;
      lucent::error("gpu",
                    "SELFTEST FAIL: windowless device created or inherited "
                    "{} SDL window(s)",
                    window_count);
    } else {
      lucent::info("gpu", "  ok: windowless device has 0 SDL windows");
    }

    auto check_clear = [&](std::string_view name, SDL_FColor color,
                           std::array<std::uint8_t, 4> expected) {
      constexpr std::uint32_t width = 4;
      constexpr std::uint32_t height = 3;
      const auto pixels = device.ClearAndReadback(width, height, color);
      std::uint32_t mismatched = 0;
      std::uint32_t first = 0;
      for (std::uint32_t i = 0; i < width * height; ++i) {
        if (std::memcmp(pixels.data() + i * 4, expected.data(), 4) != 0) {
          if (mismatched == 0)
            first = i;
          ++mismatched;
        }
      }
      if (mismatched) {
        ++bad;
        const auto *got = pixels.data() + first * 4;
        lucent::error("gpu",
                      "SELFTEST FAIL: {} scanned {} pixels, {} mismatched; "
                      "first {} was [{},{},{},{}], expected [{},{},{},{}]",
                      name, width * height, mismatched, first, got[0], got[1],
                      got[2], got[3], expected[0], expected[1], expected[2],
                      expected[3]);
      } else {
        lucent::info("gpu", "  ok: {} scanned {} pixels, 0 mismatched", name,
                     width * height);
      }
    };
    check_clear("black clear", SDL_FColor{0.f, 0.f, 0.f, 1.f}, {0, 0, 0, 255});
    check_clear("magenta clear", SDL_FColor{1.f, 0.f, 1.f, 1.f},
                {255, 0, 255, 255});

    const bool audio_during =
        (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0;
    if (audio_before || audio_during) {
      ++bad;
      lucent::error("gpu",
                    "SELFTEST FAIL: offscreen GPU test initialized audio "
                    "{}->{}",
                    audio_before, audio_during);
    } else {
      lucent::info("gpu", "  ok: SDL audio remained uninitialized");
    }
  }

  const bool video_after = (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != 0;
  const bool audio_after = (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0;
  if (video_after != video_before || audio_after != audio_before) {
    ++bad;
    lucent::error("gpu",
                  "SELFTEST FAIL: subsystem ownership leaked; video "
                  "{}->{} audio {}->{}",
                  video_before, video_after, audio_before, audio_after);
  } else {
    lucent::info("gpu", "  ok: SDL subsystem state restored after shutdown");
  }
  lucent::info("gpu", "SELFTEST: 2 offscreen colors, 24 pixels, {} failures",
               bad);
  return bad;
}

} // namespace mana::gpu
