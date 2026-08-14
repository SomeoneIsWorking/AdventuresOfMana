#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <vector>

#include "host/render_overlay.h"

namespace mana::gpu {

class Device;

// SDL3 GPU owner for the full-frame fade composition pass. Text/HUD and
// presentation deliberately remain separate owners.
class OverlayRenderer {
public:
  explicit OverlayRenderer(Device &device);
  ~OverlayRenderer();

  OverlayRenderer(const OverlayRenderer &) = delete;
  OverlayRenderer &operator=(const OverlayRenderer &) = delete;

  void Draw(SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass,
            const FadeOverlay &overlay);
  std::vector<std::uint8_t>
  DrawAndReadback(std::uint32_t width, std::uint32_t height,
                  const FadeOverlay &overlay);

private:
  Device &device_;
  SDL_GPUGraphicsPipeline *pipeline_ = nullptr;
};

int RunOverlaySelfTest(bool negative_control = false);

} // namespace mana::gpu
