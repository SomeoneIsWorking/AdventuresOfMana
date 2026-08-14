#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <span>

namespace mana::gpu {

class Device;

SDL_GPUBuffer *UploadBuffer(Device &device,
                            std::span<const std::uint8_t> data,
                            SDL_GPUBufferUsageFlags usage);
SDL_GPUTexture *UploadTexture(Device &device, std::uint32_t width,
                              std::uint32_t height,
                              SDL_GPUTextureFormat format,
                              std::uint32_t bytes_per_pixel,
                              std::span<const std::uint8_t> pixels);

} // namespace mana::gpu
