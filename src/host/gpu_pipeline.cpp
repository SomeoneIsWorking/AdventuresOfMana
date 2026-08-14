#include "host/gpu_pipeline.h"

#include <array>
#include <cstring>
#include <format>
#include <stdexcept>

#include <lucent/log.h>

#include "host/gpu_device.h"
#include "shader_pack.inc"

namespace mana::gpu {
namespace {

struct ShaderCode {
  const unsigned char *data;
  std::size_t size;
  SDL_GPUShaderFormat format;
  const char *entrypoint;
};

ShaderCode SelectShader(SDL_GPUShaderFormat supported, bool vertex) {
  using namespace embedded;
  if (supported & SDL_GPU_SHADERFORMAT_SPIRV)
    return vertex ? ShaderCode{solid_vert_spv, sizeof(solid_vert_spv),
                               SDL_GPU_SHADERFORMAT_SPIRV, "main"}
                  : ShaderCode{solid_frag_spv, sizeof(solid_frag_spv),
                               SDL_GPU_SHADERFORMAT_SPIRV, "main"};
  if (supported & SDL_GPU_SHADERFORMAT_DXIL)
    return vertex ? ShaderCode{solid_vert_dxil, sizeof(solid_vert_dxil),
                               SDL_GPU_SHADERFORMAT_DXIL, "main"}
                  : ShaderCode{solid_frag_dxil, sizeof(solid_frag_dxil),
                               SDL_GPU_SHADERFORMAT_DXIL, "main"};
  if (supported & SDL_GPU_SHADERFORMAT_MSL)
    return vertex ? ShaderCode{solid_vert_msl, sizeof(solid_vert_msl),
                               SDL_GPU_SHADERFORMAT_MSL, "main0"}
                  : ShaderCode{solid_frag_msl, sizeof(solid_frag_msl),
                               SDL_GPU_SHADERFORMAT_MSL, "main0"};
  throw std::runtime_error(
      std::format("shader pack has no artifact for formats 0x{:x}", supported));
}

SDL_GPUShader *CreateShader(Device &device, bool vertex) {
  const ShaderCode code = SelectShader(device.shader_formats(), vertex);
  const SDL_GPUShaderCreateInfo info{
      .code_size = code.size,
      .code = code.data,
      .entrypoint = code.entrypoint,
      .format = code.format,
      .stage =
          vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT,
  };
  SDL_GPUShader *shader = SDL_CreateGPUShader(device.native_handle(), &info);
  if (!shader)
    throw std::runtime_error(
        std::format("SDL_CreateGPUShader: {}", SDL_GetError()));
  return shader;
}

} // namespace

SolidPipeline::SolidPipeline(Device &device) : device_(device) {
  SDL_GPUShader *vertex = CreateShader(device_, true);
  SDL_GPUShader *fragment = nullptr;
  try {
    fragment = CreateShader(device_, false);
    const SDL_GPUColorTargetDescription color_target{
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
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
        .target_info = {.color_target_descriptions = &color_target,
                        .num_color_targets = 1},
    };
    pipeline_ = SDL_CreateGPUGraphicsPipeline(device_.native_handle(), &info);
    if (!pipeline_)
      throw std::runtime_error(
          std::format("SDL_CreateGPUGraphicsPipeline: {}", SDL_GetError()));
  } catch (...) {
    if (fragment)
      SDL_ReleaseGPUShader(device_.native_handle(), fragment);
    SDL_ReleaseGPUShader(device_.native_handle(), vertex);
    throw;
  }
  SDL_ReleaseGPUShader(device_.native_handle(), fragment);
  SDL_ReleaseGPUShader(device_.native_handle(), vertex);
}

SolidPipeline::~SolidPipeline() {
  if (pipeline_)
    SDL_ReleaseGPUGraphicsPipeline(device_.native_handle(), pipeline_);
}

std::vector<std::uint8_t> SolidPipeline::DrawAndReadback(std::uint32_t width,
                                                         std::uint32_t height) {
  return device_.RenderAndReadback(
      width, height, SDL_FColor{0.f, 0.f, 0.f, 1.f},
      [&](SDL_GPUCommandBuffer *, SDL_GPURenderPass *pass) {
        SDL_BindGPUGraphicsPipeline(pass, pipeline_);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
      });
}

int RunPipelineSelfTest(bool negative_control) {
  Device device;
  SolidPipeline pipeline(device);
  constexpr std::uint32_t width = 8;
  constexpr std::uint32_t height = 6;
  const auto pixels = pipeline.DrawAndReadback(width, height);
  const std::array<std::uint8_t, 4> expected =
      negative_control ? std::array<std::uint8_t, 4>{0, 0, 0, 255}
                       : std::array<std::uint8_t, 4>{255, 0, 255, 255};
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
    const auto *got = pixels.data() + first * 4;
    lucent::error("gpu",
                  "PIPELINE SELFTEST FAIL: scanned {} pixels, {} "
                  "mismatched; first {} was [{},{},{},{}]",
                  width * height, mismatched, first, got[0], got[1], got[2],
                  got[3]);
    return 1;
  }
  lucent::info("gpu",
               "PIPELINE SELFTEST: drew 1 triangle; scanned {} pixels, "
               "0 mismatched",
               width * height);
  return 0;
}

} // namespace mana::gpu
