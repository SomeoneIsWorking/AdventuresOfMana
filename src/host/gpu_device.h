#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace mana::gpu {

// Owns the SDL3 GPU device independently of window and audio initialization.
// Scene presentation may claim a window later; verification renders directly
// into textures so batch runs remain genuinely windowless.
class Device {
public:
  Device();
  ~Device();

  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;

  const char *driver() const;
  SDL_GPUShaderFormat shader_formats() const;
  SDL_GPUTextureFormat depth_format() const;
  SDL_GPUDevice *native_handle() const { return device_; }
  std::vector<std::uint8_t>
  ClearAndReadback(std::uint32_t width, std::uint32_t height, SDL_FColor color);
  std::vector<std::uint8_t> RenderAndReadback(
      std::uint32_t width, std::uint32_t height, SDL_FColor clear_color,
      const std::function<void(SDL_GPUCommandBuffer *, SDL_GPURenderPass *)>
          &draw,
      bool depth = false);

private:
  SDL_GPUDevice *device_ = nullptr;
  bool owns_video_ = false;
};

int RunDeviceSelfTest(bool negative_control = false);

} // namespace mana::gpu
