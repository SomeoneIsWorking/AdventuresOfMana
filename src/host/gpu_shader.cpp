#include "host/gpu_shader.h"

#include <format>
#include <stdexcept>

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

ShaderCode SelectSolid(SDL_GPUShaderFormat format, bool vertex) {
  using namespace embedded;
  if (format == SDL_GPU_SHADERFORMAT_SPIRV)
    return vertex ? ShaderCode{solid_vert_spv, sizeof(solid_vert_spv), format,
                               "main"}
                  : ShaderCode{solid_frag_spv, sizeof(solid_frag_spv), format,
                               "main"};
  if (format == SDL_GPU_SHADERFORMAT_DXIL)
    return vertex ? ShaderCode{solid_vert_dxil, sizeof(solid_vert_dxil), format,
                               "main"}
                  : ShaderCode{solid_frag_dxil, sizeof(solid_frag_dxil), format,
                               "main"};
  return vertex ? ShaderCode{solid_vert_msl, sizeof(solid_vert_msl), format,
                             "main0"}
                : ShaderCode{solid_frag_msl, sizeof(solid_frag_msl), format,
                             "main0"};
}

ShaderCode SelectTextured(SDL_GPUShaderFormat format, bool vertex) {
  using namespace embedded;
  if (format == SDL_GPU_SHADERFORMAT_SPIRV)
    return vertex ? ShaderCode{textured_vert_spv, sizeof(textured_vert_spv),
                               format, "main"}
                  : ShaderCode{textured_frag_spv, sizeof(textured_frag_spv),
                               format, "main"};
  if (format == SDL_GPU_SHADERFORMAT_DXIL)
    return vertex ? ShaderCode{textured_vert_dxil, sizeof(textured_vert_dxil),
                               format, "main"}
                  : ShaderCode{textured_frag_dxil, sizeof(textured_frag_dxil),
                               format, "main"};
  return vertex ? ShaderCode{textured_vert_msl, sizeof(textured_vert_msl),
                             format, "main0"}
                : ShaderCode{textured_frag_msl, sizeof(textured_frag_msl),
                             format, "main0"};
}

ShaderCode SelectSkinned(SDL_GPUShaderFormat format) {
  using namespace embedded;
  if (format == SDL_GPU_SHADERFORMAT_SPIRV)
    return {skinned_vert_spv, sizeof(skinned_vert_spv), format, "main"};
  if (format == SDL_GPU_SHADERFORMAT_DXIL)
    return {skinned_vert_dxil, sizeof(skinned_vert_dxil), format, "main"};
  return {skinned_vert_msl, sizeof(skinned_vert_msl), format, "main0"};
}

SDL_GPUShaderFormat PreferredFormat(SDL_GPUShaderFormat supported) {
  if (supported & SDL_GPU_SHADERFORMAT_SPIRV)
    return SDL_GPU_SHADERFORMAT_SPIRV;
  if (supported & SDL_GPU_SHADERFORMAT_DXIL)
    return SDL_GPU_SHADERFORMAT_DXIL;
  if (supported & SDL_GPU_SHADERFORMAT_MSL)
    return SDL_GPU_SHADERFORMAT_MSL;
  throw std::runtime_error(
      std::format("shader pack has no artifact for formats 0x{:x}", supported));
}

} // namespace

SDL_GPUShader *CreateShader(Device &device, std::string_view program,
                            SDL_GPUShaderStage stage,
                            ShaderResources resources) {
  const SDL_GPUShaderFormat format = PreferredFormat(device.shader_formats());
  const bool vertex = stage == SDL_GPU_SHADERSTAGE_VERTEX;
  ShaderCode code{};
  if (program == "solid")
    code = SelectSolid(format, vertex);
  else if (program == "textured")
    code = SelectTextured(format, vertex);
  else if (program == "skinned" && vertex)
    code = SelectSkinned(format);
  else if (program == "skinned")
    throw std::invalid_argument("skinned shader program has no fragment stage");
  else
    throw std::invalid_argument(
        std::format("unknown shader program '{}'", program));
  const SDL_GPUShaderCreateInfo info{
      .code_size = code.size,
      .code = code.data,
      .entrypoint = code.entrypoint,
      .format = code.format,
      .stage = stage,
      .num_samplers = resources.samplers,
      .num_uniform_buffers = resources.uniform_buffers,
  };
  SDL_GPUShader *shader = SDL_CreateGPUShader(device.native_handle(), &info);
  if (!shader)
    throw std::runtime_error(
        std::format("SDL_CreateGPUShader({}): {}", program, SDL_GetError()));
  return shader;
}

} // namespace mana::gpu
