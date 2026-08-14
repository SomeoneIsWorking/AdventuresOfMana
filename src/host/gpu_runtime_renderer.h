#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "host/render_sprite.h"

struct SDL_Window;

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
class FrameRenderer;
class Presentation;
class SpriteRenderer;

enum class BootSprite { kNone, kMaker, kTitle };

// Shipping SDL3 GPU runtime graphics owner. It composes immutable frame inputs
// and chooses either a real swapchain or synchronized texture readback; it
// never reaches into mutable engine or Lua state.
class RuntimeRenderer {
public:
  RuntimeRenderer(const mcf::Font &font, std::optional<SpriteImage> maker,
                  std::optional<SpriteImage> title, bool windowed,
                  bool unpaced);
  ~RuntimeRenderer();

  RuntimeRenderer(const RuntimeRenderer &) = delete;
  RuntimeRenderer &operator=(const RuntimeRenderer &) = delete;

  SDL_Window *window() const;
  const char *driver() const;
  static int WindowCount();

  bool PresentTitle(BootSprite sprite, const UiFrame &ui);
  std::vector<std::uint8_t> CaptureTitle(BootSprite sprite, const UiFrame &ui,
                                         std::uint32_t width,
                                         std::uint32_t height);
  bool PresentGame(const RenderSnapshot &snapshot, const UiFrame &ui,
                   const FadeOverlay &overlay);
  std::vector<std::uint8_t>
  CaptureGame(const RenderSnapshot &snapshot, const UiFrame &ui,
              const FadeOverlay &overlay, std::uint32_t width,
              std::uint32_t height);

private:
  struct SpriteSlot {
    std::optional<SpriteImage> image;
    std::unique_ptr<SpriteRenderer> renderer;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
  };

  SpriteRenderer *RequireSprite(BootSprite sprite, std::uint32_t width,
                                std::uint32_t height);
  void DrawTitle(BootSprite sprite, const UiFrame &ui, std::uint32_t width,
                 std::uint32_t height, SDL_GPUCommandBuffer *command,
                 SDL_GPURenderPass *pass);

  std::unique_ptr<Device> device_;
  std::unique_ptr<Presentation> presentation_;
  std::unique_ptr<FrameRenderer> frame_;
  SpriteSlot maker_;
  SpriteSlot title_;
};

} // namespace mana::gpu
