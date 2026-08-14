#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <string_view>

namespace mana::gpu {

class Device;

struct ShaderResources {
  std::uint32_t samplers = 0;
  std::uint32_t uniform_buffers = 0;
};

SDL_GPUShader *CreateShader(Device &device, std::string_view program,
                            SDL_GPUShaderStage stage,
                            ShaderResources resources = {});

} // namespace mana::gpu
