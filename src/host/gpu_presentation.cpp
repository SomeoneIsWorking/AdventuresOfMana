#include "host/gpu_presentation.h"

#include <format>
#include <stdexcept>
#include <string>

#include <SDL3/SDL_video.h>
#include <lucent/log.h>

#include "host/gpu_device.h"

namespace mana::gpu {
namespace {

[[noreturn]] void Fail(std::string_view operation) {
  throw std::runtime_error(std::format("{}: {}", operation, SDL_GetError()));
}

} // namespace

SDL_GPUPresentMode SelectPresentMode(bool unpaced,
                                     bool immediate_supported) {
  return unpaced && immediate_supported ? SDL_GPU_PRESENTMODE_IMMEDIATE
                                        : SDL_GPU_PRESENTMODE_VSYNC;
}

Presentation::Presentation(Device &device, std::string_view title,
                           std::uint32_t width, std::uint32_t height,
                           bool unpaced)
    : device_(device) {
  if (title.empty() || !width || !height)
    throw std::invalid_argument(
        "GPU presentation requires a title and nonzero dimensions");
  const std::string owned_title(title);
  window_ = SDL_CreateWindow(owned_title.c_str(), static_cast<int>(width),
                             static_cast<int>(height),
                             SDL_WINDOW_RESIZABLE);
  if (!window_)
    Fail("SDL_CreateWindow");
  if (!SDL_ClaimWindowForGPUDevice(device_.native_handle(), window_)) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
    Fail("SDL_ClaimWindowForGPUDevice");
  }
  claimed_ = true;
  try {
    const bool immediate_supported = SDL_WindowSupportsGPUPresentMode(
        device_.native_handle(), window_, SDL_GPU_PRESENTMODE_IMMEDIATE);
    present_mode_ = SelectPresentMode(unpaced, immediate_supported);
    if (!SDL_SetGPUSwapchainParameters(device_.native_handle(), window_,
                                       SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       present_mode_))
      Fail("SDL_SetGPUSwapchainParameters");
    swapchain_format_ =
        SDL_GetGPUSwapchainTextureFormat(device_.native_handle(), window_);
    if (swapchain_format_ == SDL_GPU_TEXTUREFORMAT_INVALID)
      Fail("SDL_GetGPUSwapchainTextureFormat");
  } catch (...) {
    SDL_ReleaseWindowFromGPUDevice(device_.native_handle(), window_);
    SDL_DestroyWindow(window_);
    window_ = nullptr;
    claimed_ = false;
    throw;
  }
  lucent::info("gpu", "presentation: {}x{}, swapchain format {}, {}",
               width, height, static_cast<int>(swapchain_format_),
               present_mode_ == SDL_GPU_PRESENTMODE_IMMEDIATE ? "unpaced"
                                                               : "vsync");
}

Presentation::~Presentation() {
  if (!window_)
    return;
  if (!SDL_WaitForGPUIdle(device_.native_handle()))
    lucent::error("gpu", "SDL_WaitForGPUIdle during presentation shutdown: {}",
                  SDL_GetError());
  ReleaseTargets();
  if (claimed_)
    SDL_ReleaseWindowFromGPUDevice(device_.native_handle(), window_);
  SDL_DestroyWindow(window_);
}

void Presentation::ReleaseTargets() {
  if (depth_)
    SDL_ReleaseGPUTexture(device_.native_handle(), depth_);
  if (color_)
    SDL_ReleaseGPUTexture(device_.native_handle(), color_);
  depth_ = nullptr;
  color_ = nullptr;
  width_ = 0;
  height_ = 0;
}

void Presentation::EnsureTargets(std::uint32_t width, std::uint32_t height) {
  if (color_ && width == width_ && height == height_)
    return;
  ReleaseTargets();
  const SDL_GPUTextureCreateInfo color_info{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
      .width = width,
      .height = height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
  };
  color_ = SDL_CreateGPUTexture(device_.native_handle(), &color_info);
  if (!color_)
    Fail("SDL_CreateGPUTexture(presentation color)");
  const SDL_GPUTextureCreateInfo depth_info{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = device_.depth_format(),
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
      .width = width,
      .height = height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
  };
  depth_ = SDL_CreateGPUTexture(device_.native_handle(), &depth_info);
  if (!depth_) {
    ReleaseTargets();
    Fail("SDL_CreateGPUTexture(presentation depth)");
  }
  width_ = width;
  height_ = height;
}

bool Presentation::Present(SDL_FColor clear_color, const Draw &draw) {
  SDL_GPUCommandBuffer *command =
      SDL_AcquireGPUCommandBuffer(device_.native_handle());
  if (!command)
    Fail("SDL_AcquireGPUCommandBuffer(presentation)");
  SDL_GPUTexture *swapchain = nullptr;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, window_, &swapchain,
                                             &width, &height)) {
    SDL_CancelGPUCommandBuffer(command);
    Fail("SDL_WaitAndAcquireGPUSwapchainTexture");
  }
  if (!swapchain || !width || !height) {
    SDL_CancelGPUCommandBuffer(command);
    return false;
  }
  try {
    EnsureTargets(width, height);
  } catch (...) {
    SDL_CancelGPUCommandBuffer(command);
    throw;
  }
  const SDL_GPUColorTargetInfo color_target{
      .texture = color_,
      .clear_color = clear_color,
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE,
  };
  const SDL_GPUDepthStencilTargetInfo depth_target{
      .texture = depth_,
      .clear_depth = 1.f,
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_DONT_CARE,
      .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
      .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
  };
  SDL_GPURenderPass *pass =
      SDL_BeginGPURenderPass(command, &color_target, 1, &depth_target);
  if (!pass) {
    SDL_CancelGPUCommandBuffer(command);
    Fail("SDL_BeginGPURenderPass(presentation)");
  }
  try {
    if (draw)
      draw(width, height, command, pass);
  } catch (...) {
    SDL_EndGPURenderPass(pass);
    SDL_CancelGPUCommandBuffer(command);
    throw;
  }
  SDL_EndGPURenderPass(pass);
  const SDL_GPUBlitInfo blit{
      .source = {.texture = color_, .w = width, .h = height},
      .destination = {.texture = swapchain, .w = width, .h = height},
      .load_op = SDL_GPU_LOADOP_DONT_CARE,
      .flip_mode = SDL_FLIP_NONE,
      .filter = SDL_GPU_FILTER_LINEAR,
  };
  SDL_BlitGPUTexture(command, &blit);
  if (!SDL_SubmitGPUCommandBuffer(command))
    Fail("SDL_SubmitGPUCommandBuffer(presentation)");
  return true;
}

int RunPresentationPolicySelfTest() {
  int bad = 0;
  bad += SelectPresentMode(false, false) != SDL_GPU_PRESENTMODE_VSYNC;
  bad += SelectPresentMode(false, true) != SDL_GPU_PRESENTMODE_VSYNC;
  bad += SelectPresentMode(true, false) != SDL_GPU_PRESENTMODE_VSYNC;
  bad += SelectPresentMode(true, true) != SDL_GPU_PRESENTMODE_IMMEDIATE;
  lucent::info("gpu", "PRESENTATION POLICY SELFTEST: 4 cases, {} failures",
               bad);
  return bad;
}

int RunPresentationSmokeTest() {
  Device device;
  Presentation presentation(device, "Adventures of Mana presentation smoke",
                            64, 48, true);
  const bool presented = presentation.Present(
      SDL_FColor{.25f, .5f, .75f, 1.f}, {});
  if (!presented) {
    lucent::error("gpu", "PRESENTATION SMOKE FAIL: swapchain was unavailable");
    return 1;
  }
  lucent::info("gpu", "PRESENTATION SMOKE: submitted one RGBA-to-swapchain frame");
  return 0;
}

} // namespace mana::gpu
