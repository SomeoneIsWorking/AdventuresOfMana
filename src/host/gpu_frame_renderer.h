#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace mcf {
class Font;
}

namespace mana {
struct FadeOverlay;
struct RenderSnapshot;
struct UiFrame;
}

namespace mana::gpu {

class Device;
class OverlayRenderer;
class SnapshotRenderer;
class UiRenderer;

// Composes one immutable running-game frame in shipping order: world scene,
// game UI, then authored fade. It owns GPU render resources, not game state,
// command submission, readback, or presentation.
class FrameRenderer {
public:
  FrameRenderer(Device &device, const mcf::Font &font);
  ~FrameRenderer();

  FrameRenderer(const FrameRenderer &) = delete;
  FrameRenderer &operator=(const FrameRenderer &) = delete;

  void Draw(const RenderSnapshot &snapshot, std::uint32_t width,
            std::uint32_t height, const UiFrame &ui,
            const FadeOverlay &overlay, SDL_GPUCommandBuffer *command,
            SDL_GPURenderPass *pass);
  std::vector<std::uint8_t>
  DrawAndReadback(const RenderSnapshot &snapshot, std::uint32_t width,
                  std::uint32_t height, const UiFrame &ui,
                  const FadeOverlay &overlay);
  std::size_t cached_asset_count() const;

private:
  Device &device_;
  std::unique_ptr<SnapshotRenderer> scene_;
  std::unique_ptr<UiRenderer> ui_;
  std::unique_ptr<OverlayRenderer> overlay_;
};

} // namespace mana::gpu
