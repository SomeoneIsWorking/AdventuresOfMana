#include "host/gpu_asset.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <limits>
#include <span>
#include <stdexcept>

#include "host/gpu_device.h"

namespace mana::gpu {
namespace {

[[noreturn]] void Fail(std::string_view operation) {
  throw std::runtime_error(std::format("{}: {}", operation, SDL_GetError()));
}

void SubmitUpload(Device &device, SDL_GPUTransferBuffer *transfer,
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

SDL_GPUBuffer *UploadBuffer(Device &device, std::span<const std::uint8_t> data,
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
    SubmitUpload(device, transfer, buffer, size);
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
                              std::span<const std::uint8_t> pixels) {
  const std::uint64_t byte_count = std::uint64_t(width) * height * 4;
  if (!width || !height || byte_count > pixels.size() ||
      byte_count > std::numeric_limits<std::uint32_t>::max())
    throw std::invalid_argument("RGBA texture base level is incomplete");
  const auto size = static_cast<std::uint32_t>(byte_count);
  const SDL_GPUTextureCreateInfo texture_info{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
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

} // namespace

Asset::Asset(Device &device, const mcf::RenderAsset &source) : device_(device) {
  if (source.model.index_size != 2 && source.model.index_size != 4)
    throw std::invalid_argument("model index elements must be 16 or 32 bit");
  vertices_ = UploadBuffer(device_, source.model.vertices(),
                           SDL_GPU_BUFFERUSAGE_VERTEX);
  try {
    indices_ = UploadBuffer(device_, source.model.indices(),
                            SDL_GPU_BUFFERUSAGE_INDEX);
    const SDL_GPUSamplerCreateInfo sampler_info{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .max_lod = 0.f,
    };
    sampler_ = SDL_CreateGPUSampler(device_.native_handle(), &sampler_info);
    if (!sampler_)
      Fail("SDL_CreateGPUSampler");
    const std::uint8_t white[] = {255, 255, 255, 255};
    white_ = UploadTexture(device_, 1, 1, white);
    textures_.resize(source.textures.size());
    for (std::size_t i = 0; i < source.textures.size(); ++i) {
      const mcf::Texture *texture = source.TextureAt(i);
      if (texture)
        textures_[i] = UploadTexture(device_, texture->width, texture->height,
                                     texture->pixels);
    }
  } catch (...) {
    for (SDL_GPUTexture *texture : textures_)
      if (texture)
        SDL_ReleaseGPUTexture(device_.native_handle(), texture);
    if (white_)
      SDL_ReleaseGPUTexture(device_.native_handle(), white_);
    if (sampler_)
      SDL_ReleaseGPUSampler(device_.native_handle(), sampler_);
    if (indices_)
      SDL_ReleaseGPUBuffer(device_.native_handle(), indices_);
    if (vertices_)
      SDL_ReleaseGPUBuffer(device_.native_handle(), vertices_);
    vertices_ = nullptr;
    indices_ = nullptr;
    sampler_ = nullptr;
    white_ = nullptr;
    textures_.clear();
    throw;
  }
  draw_textures_ = source.draw_textures;
  layout_ = source.model.layout;
  draws_ = source.model.draws;
  draw_blended_.reserve(draws_.size());
  for (const auto &draw : draws_) {
    draw_blended_.push_back(draw.material < source.model.materials.size() &&
                            source.model.materials[draw.material].blend);
  }
  index_type_ = source.model.index_size == 2 ? SDL_GPU_INDEXELEMENTSIZE_16BIT
                                             : SDL_GPU_INDEXELEMENTSIZE_32BIT;
  vertex_stride_ = source.model.vertex_stride;
  std::memcpy(lo_, source.lo, sizeof(lo_));
  std::memcpy(hi_, source.hi, sizeof(hi_));
}

Asset::~Asset() {
  for (SDL_GPUTexture *texture : textures_)
    if (texture)
      SDL_ReleaseGPUTexture(device_.native_handle(), texture);
  if (white_)
    SDL_ReleaseGPUTexture(device_.native_handle(), white_);
  if (sampler_)
    SDL_ReleaseGPUSampler(device_.native_handle(), sampler_);
  if (indices_)
    SDL_ReleaseGPUBuffer(device_.native_handle(), indices_);
  if (vertices_)
    SDL_ReleaseGPUBuffer(device_.native_handle(), vertices_);
}

SDL_GPUTexture *Asset::TextureForDraw(std::size_t draw,
                                      bool use_material_texture) const {
  if (use_material_texture && draw < draw_textures_.size() &&
      draw_textures_[draw] && *draw_textures_[draw] < textures_.size() &&
      textures_[*draw_textures_[draw]])
    return textures_[*draw_textures_[draw]];
  return white_;
}

bool Asset::DrawBlended(std::size_t draw) const {
  return draw < draw_blended_.size() && draw_blended_[draw];
}

std::size_t Asset::blended_draw_count() const {
  return std::count(draw_blended_.begin(), draw_blended_.end(), true);
}

} // namespace mana::gpu
