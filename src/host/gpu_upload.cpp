#include "host/gpu_upload.h"

#include <cstring>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>

#include "host/gpu_device.h"

namespace mana::gpu {
namespace {

[[noreturn]] void Fail(std::string_view operation) {
  throw std::runtime_error(std::format("{}: {}", operation, SDL_GetError()));
}

void SubmitBufferUpload(Device &device, SDL_GPUTransferBuffer *transfer,
                        SDL_GPUBuffer *buffer, std::uint32_t bytes) {
  SDL_GPUCommandBuffer *command =
      SDL_AcquireGPUCommandBuffer(device.native_handle());
  if (!command)
    Fail("SDL_AcquireGPUCommandBuffer");
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
  if (!copy) {
    SDL_CancelGPUCommandBuffer(command);
    Fail("SDL_BeginGPUCopyPass");
  }
  const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer};
  const SDL_GPUBufferRegion destination{.buffer = buffer, .size = bytes};
  SDL_UploadToGPUBuffer(copy, &source, &destination, false);
  SDL_EndGPUCopyPass(copy);
  if (!SDL_SubmitGPUCommandBuffer(command))
    Fail("SDL_SubmitGPUCommandBuffer");
}

} // namespace

SDL_GPUBuffer *UploadBuffer(Device &device,
                            std::span<const std::uint8_t> data,
                            SDL_GPUBufferUsageFlags usage) {
  if (data.empty() || data.size() > std::numeric_limits<std::uint32_t>::max())
    throw std::invalid_argument("GPU buffer upload size is invalid");
  const auto size = static_cast<std::uint32_t>(data.size());
  const SDL_GPUBufferCreateInfo buffer_info{.usage = usage, .size = size};
  SDL_GPUBuffer *buffer =
      SDL_CreateGPUBuffer(device.native_handle(), &buffer_info);
  if (!buffer)
    Fail("SDL_CreateGPUBuffer");
  const SDL_GPUTransferBufferCreateInfo transfer_info{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size};
  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device.native_handle(), &transfer_info);
  if (!transfer) {
    SDL_ReleaseGPUBuffer(device.native_handle(), buffer);
    Fail("SDL_CreateGPUTransferBuffer");
  }
  void *mapped =
      SDL_MapGPUTransferBuffer(device.native_handle(), transfer, false);
  if (!mapped) {
    SDL_ReleaseGPUTransferBuffer(device.native_handle(), transfer);
    SDL_ReleaseGPUBuffer(device.native_handle(), buffer);
    Fail("SDL_MapGPUTransferBuffer");
  }
  std::memcpy(mapped, data.data(), data.size());
  SDL_UnmapGPUTransferBuffer(device.native_handle(), transfer);
  try {
    SubmitBufferUpload(device, transfer, buffer, size);
  } catch (...) {
    SDL_ReleaseGPUTransferBuffer(device.native_handle(), transfer);
    SDL_ReleaseGPUBuffer(device.native_handle(), buffer);
    throw;
  }
  SDL_ReleaseGPUTransferBuffer(device.native_handle(), transfer);
  return buffer;
}

SDL_GPUTexture *UploadTexture(Device &device, std::uint32_t width,
                              std::uint32_t height,
                              SDL_GPUTextureFormat format,
                              std::uint32_t bytes_per_pixel,
                              std::span<const std::uint8_t> pixels) {
  const std::uint64_t byte_count =
      std::uint64_t(width) * height * bytes_per_pixel;
  if (!width || !height || !bytes_per_pixel || byte_count > pixels.size() ||
      byte_count > std::numeric_limits<std::uint32_t>::max())
    throw std::invalid_argument("GPU texture base level is incomplete");
  const auto size = static_cast<std::uint32_t>(byte_count);
  const SDL_GPUTextureCreateInfo texture_info{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = format,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = width,
      .height = height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
  };
  SDL_GPUTexture *texture =
      SDL_CreateGPUTexture(device.native_handle(), &texture_info);
  if (!texture)
    Fail("SDL_CreateGPUTexture");
  const SDL_GPUTransferBufferCreateInfo transfer_info{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size};
  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device.native_handle(), &transfer_info);
  if (!transfer) {
    SDL_ReleaseGPUTexture(device.native_handle(), texture);
    Fail("SDL_CreateGPUTransferBuffer");
  }
  void *mapped =
      SDL_MapGPUTransferBuffer(device.native_handle(), transfer, false);
  if (!mapped) {
    SDL_ReleaseGPUTransferBuffer(device.native_handle(), transfer);
    SDL_ReleaseGPUTexture(device.native_handle(), texture);
    Fail("SDL_MapGPUTransferBuffer");
  }
  std::memcpy(mapped, pixels.data(), size);
  SDL_UnmapGPUTransferBuffer(device.native_handle(), transfer);

  try {
    SDL_GPUCommandBuffer *command =
        SDL_AcquireGPUCommandBuffer(device.native_handle());
    if (!command)
      Fail("SDL_AcquireGPUCommandBuffer");
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
      SDL_CancelGPUCommandBuffer(command);
      Fail("SDL_BeginGPUCopyPass");
    }
    const SDL_GPUTextureTransferInfo source{
        .transfer_buffer = transfer,
        .pixels_per_row = width,
        .rows_per_layer = height,
    };
    const SDL_GPUTextureRegion destination{
        .texture = texture, .w = width, .h = height, .d = 1};
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(command))
      Fail("SDL_SubmitGPUCommandBuffer");
  } catch (...) {
    SDL_ReleaseGPUTransferBuffer(device.native_handle(), transfer);
    SDL_ReleaseGPUTexture(device.native_handle(), texture);
    throw;
  }
  SDL_ReleaseGPUTransferBuffer(device.native_handle(), transfer);
  return texture;
}

} // namespace mana::gpu
