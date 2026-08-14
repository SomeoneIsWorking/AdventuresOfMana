#include "host/gpu_asset.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <stdexcept>

#include "host/gpu_device.h"
#include "host/gpu_upload.h"

namespace mana::gpu {
namespace {

[[noreturn]] void Fail(std::string_view operation) {
  throw std::runtime_error(std::format("{}: {}", operation, SDL_GetError()));
}

} // namespace

Asset::Asset(Device &device, const mcf::RenderAsset &source) : device_(device) {
  if (source.model.index_size != 2 && source.model.index_size != 4)
    throw std::invalid_argument("model index elements must be 16 or 32 bit");
  const std::size_t normal_float_count =
      std::size_t(source.model.vertex_count) * 3;
  if (source.normals.values.size() != normal_float_count)
    throw std::invalid_argument(std::format(
        "model has {} generated normal floats for {} vertices; expected {}",
        source.normals.values.size(), source.model.vertex_count,
        normal_float_count));
  normal_offset_ = source.model.vertex_stride;
  vertex_stride_ = normal_offset_ + sizeof(float) * 3;
  std::vector<std::uint8_t> expanded_vertices(
      std::size_t(source.model.vertex_count) * vertex_stride_);
  const auto source_vertices = source.model.vertices();
  for (std::uint32_t vertex = 0; vertex < source.model.vertex_count; ++vertex) {
    std::uint8_t *destination =
        expanded_vertices.data() + std::size_t(vertex) * vertex_stride_;
    std::memcpy(destination,
                source_vertices.data() +
                    std::size_t(vertex) * source.model.vertex_stride,
                source.model.vertex_stride);
    std::memcpy(destination + normal_offset_,
                source.normals.values.data() + std::size_t(vertex) * 3,
                sizeof(float) * 3);
  }
  vertices_ =
      UploadBuffer(device_, expanded_vertices, SDL_GPU_BUFFERUSAGE_VERTEX);
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
    white_ = UploadTexture(device_, 1, 1,
                           SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 4, white);
    textures_.resize(source.textures.size());
    for (std::size_t i = 0; i < source.textures.size(); ++i) {
      const mcf::Texture *texture = source.TextureAt(i);
      if (texture)
        textures_[i] = UploadTexture(
            device_, texture->width, texture->height,
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 4, texture->pixels);
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
  skinned_ = source.skinned();
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
