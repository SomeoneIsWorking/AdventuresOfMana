#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <vector>

#include "host/render_ui.h"

namespace mcf {
class Font;
}

namespace mana::gpu {

class Device;

// SDL3 GPU consumer for backend-independent UI batches. Font and vertex
// uploads happen before a render pass; Draw only submits prepared state.
class UiRenderer {
public:
  UiRenderer(Device &device, const mcf::Font &font);
  ~UiRenderer();

  UiRenderer(const UiRenderer &) = delete;
  UiRenderer &operator=(const UiRenderer &) = delete;

  void Prepare(const UiFrame &frame);
  void Draw(SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass);
  std::vector<std::uint8_t> DrawAndReadback(std::uint32_t width,
                                            std::uint32_t height,
                                            const UiFrame &frame);

  std::size_t prepared_batches() const { return batches_.size(); }
  std::uint32_t prepared_vertices() const { return vertex_count_; }

private:
  Device &device_;
  SDL_GPUGraphicsPipeline *pipeline_ = nullptr;
  SDL_GPUTexture *atlas_ = nullptr;
  SDL_GPUSampler *sampler_ = nullptr;
  SDL_GPUBuffer *vertices_ = nullptr;
  std::vector<UiBatch> batches_;
  std::uint32_t vertex_count_ = 0;
};

} // namespace mana::gpu
