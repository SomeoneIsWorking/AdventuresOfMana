#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "host/render_asset.h"

namespace mana::gpu {

class Device;

// Owns the GPU copies of one backend-independent RenderAsset. The source asset
// is only needed during construction; all draw metadata is copied here.
class Asset {
public:
  Asset(Device &device, const mcf::RenderAsset &source);
  ~Asset();

  Asset(const Asset &) = delete;
  Asset &operator=(const Asset &) = delete;

  SDL_GPUBuffer *vertices() const { return vertices_; }
  SDL_GPUBuffer *indices() const { return indices_; }
  SDL_GPUSampler *sampler() const { return sampler_; }
  SDL_GPUTexture *TextureForDraw(std::size_t draw,
                                 bool use_material_texture = true) const;
  SDL_GPUIndexElementSize index_type() const { return index_type_; }
  std::uint32_t vertex_stride() const { return vertex_stride_; }
  const std::vector<mcf::VertexAttribute> &layout() const { return layout_; }
  const std::vector<mcf::DrawRange> &draws() const { return draws_; }
  bool DrawBlended(std::size_t draw) const;
  std::size_t blended_draw_count() const;
  const float *lo() const { return lo_; }
  const float *hi() const { return hi_; }

private:
  Device &device_;
  SDL_GPUBuffer *vertices_ = nullptr;
  SDL_GPUBuffer *indices_ = nullptr;
  SDL_GPUSampler *sampler_ = nullptr;
  SDL_GPUTexture *white_ = nullptr;
  std::vector<SDL_GPUTexture *> textures_;
  std::vector<std::optional<std::uint32_t>> draw_textures_;
  std::vector<mcf::VertexAttribute> layout_;
  std::vector<mcf::DrawRange> draws_;
  std::vector<bool> draw_blended_;
  SDL_GPUIndexElementSize index_type_ = SDL_GPU_INDEXELEMENTSIZE_16BIT;
  std::uint32_t vertex_stride_ = 0;
  float lo_[3]{}, hi_[3]{};
};

} // namespace mana::gpu
