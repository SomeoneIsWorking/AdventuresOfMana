#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <functional>
#include <string_view>

struct SDL_Window;

namespace mana::gpu {

class Device;

// Owns the shipping window, its SDL3 GPU swapchain, and the intermediate
// RGBA/depth targets consumed by the backend-independent renderer stack.
// Automated render verification does not construct this type.
class Presentation {
public:
  using Draw = std::function<void(std::uint32_t, std::uint32_t,
                                  SDL_GPUCommandBuffer *, SDL_GPURenderPass *)>;

  Presentation(Device &device, std::string_view title, std::uint32_t width,
               std::uint32_t height, bool unpaced);
  ~Presentation();

  Presentation(const Presentation &) = delete;
  Presentation &operator=(const Presentation &) = delete;

  bool Present(SDL_FColor clear_color, const Draw &draw);
  SDL_Window *window() const { return window_; }
  SDL_GPUTextureFormat swapchain_format() const { return swapchain_format_; }
  SDL_GPUPresentMode present_mode() const { return present_mode_; }

private:
  void EnsureTargets(std::uint32_t width, std::uint32_t height);
  void ReleaseTargets();

  Device &device_;
  SDL_Window *window_ = nullptr;
  SDL_GPUTexture *color_ = nullptr;
  SDL_GPUTexture *depth_ = nullptr;
  std::uint32_t width_ = 0;
  std::uint32_t height_ = 0;
  SDL_GPUTextureFormat swapchain_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
  SDL_GPUPresentMode present_mode_ = SDL_GPU_PRESENTMODE_VSYNC;
  bool claimed_ = false;
};

SDL_GPUPresentMode SelectPresentMode(bool unpaced, bool immediate_supported);
int RunPresentationPolicySelfTest();
int RunPresentationSmokeTest();

} // namespace mana::gpu
