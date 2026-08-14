#include "host/gpu_overlay.h"

#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <stdexcept>

#include <lucent/log.h>

#include "host/gpu_device.h"
#include "host/gpu_shader.h"

namespace mana::gpu {

OverlayRenderer::OverlayRenderer(Device &device) : device_(device) {
  SDL_GPUShader *vertex = CreateShader(
      device_, "overlay", SDL_GPU_SHADERSTAGE_VERTEX,
      ShaderResources{.uniform_buffers = 1});
  SDL_GPUShader *fragment = nullptr;
  try {
    fragment =
        CreateShader(device_, "overlay", SDL_GPU_SHADERSTAGE_FRAGMENT);
    const SDL_GPUColorTargetDescription color_target{
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .blend_state =
            {
                .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                .dst_color_blendfactor =
                    SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .color_blend_op = SDL_GPU_BLENDOP_ADD,
                .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                .dst_alpha_blendfactor =
                    SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                .enable_blend = true,
            },
    };
    const SDL_GPUGraphicsPipelineCreateInfo info{
        .vertex_shader = vertex,
        .fragment_shader = fragment,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                             .cull_mode = SDL_GPU_CULLMODE_NONE,
                             .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
                             .enable_depth_clip = true},
        .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
        .depth_stencil_state = {.enable_depth_test = false,
                                .enable_depth_write = false},
        .target_info =
            {
                .color_target_descriptions = &color_target,
                .num_color_targets = 1,
                .depth_stencil_format = device_.depth_format(),
                .has_depth_stencil_target = true,
            },
    };
    pipeline_ = SDL_CreateGPUGraphicsPipeline(device_.native_handle(), &info);
    if (!pipeline_)
      throw std::runtime_error(std::format(
          "SDL_CreateGPUGraphicsPipeline(overlay): {}", SDL_GetError()));
  } catch (...) {
    if (fragment)
      SDL_ReleaseGPUShader(device_.native_handle(), fragment);
    SDL_ReleaseGPUShader(device_.native_handle(), vertex);
    throw;
  }
  SDL_ReleaseGPUShader(device_.native_handle(), fragment);
  SDL_ReleaseGPUShader(device_.native_handle(), vertex);
}

OverlayRenderer::~OverlayRenderer() {
  if (pipeline_)
    SDL_ReleaseGPUGraphicsPipeline(device_.native_handle(), pipeline_);
}

void OverlayRenderer::Draw(SDL_GPUCommandBuffer *command,
                           SDL_GPURenderPass *pass,
                           const FadeOverlay &overlay) {
  if (!command)
    throw std::invalid_argument("fade overlay has no command buffer");
  if (!pass)
    throw std::invalid_argument("fade overlay has no render pass");
  if (!overlay.visible())
    return;
  SDL_BindGPUGraphicsPipeline(pass, pipeline_);
  SDL_PushGPUVertexUniformData(command, 0, overlay.color.data(),
                               sizeof(overlay.color));
  SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
}

std::vector<std::uint8_t>
OverlayRenderer::DrawAndReadback(std::uint32_t width, std::uint32_t height,
                                 const FadeOverlay &overlay) {
  return device_.RenderAndReadback(
      width, height, SDL_FColor{.8f, .4f, .2f, 1.f},
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        Draw(command, pass, overlay);
      },
      true);
}

int RunOverlaySelfTest(bool negative_control) {
  Device device;
  OverlayRenderer renderer(device);
  constexpr std::array<std::uint8_t, 3> black{0, 0, 0};
  const auto overlay = FadeOverlay::FromEngineColor(
      black, negative_control ? 0.f : .5f);
  constexpr std::uint32_t width = 4;
  constexpr std::uint32_t height = 3;
  const auto pixels = renderer.DrawAndReadback(width, height, overlay);
  constexpr std::array<std::uint8_t, 4> expected{102, 51, 26, 191};
  std::uint32_t mismatched = 0;
  std::uint32_t first = 0;
  for (std::uint32_t i = 0; i < width * height; ++i) {
    const auto *pixel = pixels.data() + i * 4;
    bool match = true;
    for (int channel = 0; channel < 4; ++channel)
      match &= std::abs(int(pixel[channel]) - int(expected[channel])) <= 1;
    if (!match) {
      if (mismatched == 0)
        first = i;
      ++mismatched;
    }
  }
  if (mismatched) {
    const auto *got = pixels.data() + first * 4;
    lucent::error("gpu",
                  "OVERLAY SELFTEST FAIL: scanned {} pixels, {} mismatched; "
                  "first {} was [{},{},{},{}], expected [{},{},{},{}] +/-1",
                  width * height, mismatched, first, got[0], got[1], got[2],
                  got[3], expected[0], expected[1], expected[2], expected[3]);
    return 1;
  }
  lucent::info("gpu",
               "OVERLAY SELFTEST: half-black fade scanned {} pixels, 0 "
               "mismatched",
               width * height);
  return 0;
}

} // namespace mana::gpu
