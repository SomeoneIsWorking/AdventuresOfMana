#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>

namespace mana {
struct SpriteImage;
}

namespace mana::gpu {

class Device;

class SpriteRenderer {
public:
  SpriteRenderer(Device &device, const SpriteImage &image,
                 std::uint32_t viewport_width, std::uint32_t viewport_height);
  ~SpriteRenderer();

  SpriteRenderer(const SpriteRenderer &) = delete;
  SpriteRenderer &operator=(const SpriteRenderer &) = delete;

  void Draw(SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass);

private:
  Device &device_;
  SDL_GPUTexture *texture_ = nullptr;
  SDL_GPUSampler *sampler_ = nullptr;
  SDL_GPUBuffer *vertices_ = nullptr;
  SDL_GPUGraphicsPipeline *pipeline_ = nullptr;
};

} // namespace mana::gpu
