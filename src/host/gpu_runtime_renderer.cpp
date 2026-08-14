#include "host/gpu_runtime_renderer.h"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <utility>

#include "host/gpu_device.h"
#include "host/gpu_frame_renderer.h"
#include "host/gpu_presentation.h"
#include "host/gpu_sprite.h"
#include "host/render_overlay.h"
#include "host/render_snapshot.h"
#include "host/render_ui.h"

namespace mana::gpu {

RuntimeRenderer::RuntimeRenderer(const mcf::Font &font,
                                 std::optional<SpriteImage> maker,
                                 std::optional<SpriteImage> title,
                                 bool windowed, bool unpaced)
    : device_(std::make_unique<Device>()),
      frame_(std::make_unique<FrameRenderer>(*device_, font)) {
  maker_.image = std::move(maker);
  title_.image = std::move(title);
  if (windowed)
    presentation_ = std::make_unique<Presentation>(
        *device_, "Adventures of Mana", 720, 720, unpaced);
}

RuntimeRenderer::~RuntimeRenderer() = default;

SDL_Window *RuntimeRenderer::window() const {
  return presentation_ ? presentation_->window() : nullptr;
}

const char *RuntimeRenderer::driver() const { return device_->driver(); }

int RuntimeRenderer::WindowCount() {
  int count = 0;
  SDL_Window **windows = SDL_GetWindows(&count);
  SDL_free(windows);
  return count;
}

SpriteRenderer *RuntimeRenderer::RequireSprite(BootSprite sprite,
                                               std::uint32_t width,
                                               std::uint32_t height) {
  SpriteSlot *slot = nullptr;
  if (sprite == BootSprite::kMaker)
    slot = &maker_;
  else if (sprite == BootSprite::kTitle)
    slot = &title_;
  if (!slot || !slot->image)
    return nullptr;
  if (!slot->renderer || slot->width != width || slot->height != height) {
    slot->renderer =
        std::make_unique<SpriteRenderer>(*device_, *slot->image, width, height);
    slot->width = width;
    slot->height = height;
  }
  return slot->renderer.get();
}

void RuntimeRenderer::DrawTitle(BootSprite sprite, const UiFrame &ui,
                                std::uint32_t width, std::uint32_t height,
                                SDL_GPUCommandBuffer *command,
                                SDL_GPURenderPass *pass) {
  if (auto *renderer = RequireSprite(sprite, width, height))
    renderer->Draw(command, pass);
  frame_->DrawUi(ui, command, pass);
}

bool RuntimeRenderer::PresentTitle(BootSprite sprite, const UiFrame &ui) {
  if (!presentation_)
    throw std::logic_error("title presentation requested without a window");
  return presentation_->Present(
      SDL_FColor{0.f, 0.f, 0.f, 1.f},
      [&](std::uint32_t width, std::uint32_t height,
          SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        DrawTitle(sprite, ui, width, height, command, pass);
      });
}

std::vector<std::uint8_t>
RuntimeRenderer::CaptureTitle(BootSprite sprite, const UiFrame &ui,
                              std::uint32_t width, std::uint32_t height) {
  return device_->RenderAndReadback(
      width, height, SDL_FColor{0.f, 0.f, 0.f, 1.f},
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        DrawTitle(sprite, ui, width, height, command, pass);
      },
      true);
}

bool RuntimeRenderer::PresentGame(const RenderSnapshot &snapshot,
                                  const UiFrame &ui,
                                  const FadeOverlay &overlay) {
  if (!presentation_)
    throw std::logic_error("game presentation requested without a window");
  return presentation_->Present(
      SDL_FColor{.1f, .11f, .14f, 1.f},
      [&](std::uint32_t width, std::uint32_t height,
          SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        frame_->Draw(snapshot, width, height, ui, overlay, command, pass);
      });
}

std::vector<std::uint8_t> RuntimeRenderer::CaptureGame(
    const RenderSnapshot &snapshot, const UiFrame &ui,
    const FadeOverlay &overlay, std::uint32_t width, std::uint32_t height) {
  return frame_->DrawAndReadback(snapshot, width, height, ui, overlay);
}

} // namespace mana::gpu
