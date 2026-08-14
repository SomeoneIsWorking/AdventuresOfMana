#include "host/gpu_frame_renderer.h"

#include "host/gpu_device.h"
#include "host/gpu_overlay.h"
#include "host/gpu_snapshot_renderer.h"
#include "host/gpu_ui.h"
#include "host/render_overlay.h"
#include "host/render_snapshot.h"
#include "host/render_ui.h"

namespace mana::gpu {

FrameRenderer::FrameRenderer(Device &device, const mcf::Font &font)
    : device_(device), scene_(std::make_unique<SnapshotRenderer>(device)),
      ui_(std::make_unique<UiRenderer>(device, font)),
      overlay_(std::make_unique<OverlayRenderer>(device)) {}

FrameRenderer::~FrameRenderer() = default;

void FrameRenderer::Draw(const RenderSnapshot &snapshot, std::uint32_t width,
                         std::uint32_t height, const UiFrame &ui,
                         const FadeOverlay &overlay,
                         SDL_GPUCommandBuffer *command,
                         SDL_GPURenderPass *pass) {
  scene_->Draw(snapshot, width, height, command, pass);
  DrawUi(ui, command, pass);
  overlay_->Draw(command, pass, overlay);
}

void FrameRenderer::DrawUi(const UiFrame &ui, SDL_GPUCommandBuffer *command,
                           SDL_GPURenderPass *pass) {
  ui_->Prepare(ui);
  ui_->Draw(command, pass);
}

std::vector<std::uint8_t> FrameRenderer::DrawAndReadback(
    const RenderSnapshot &snapshot, std::uint32_t width,
    std::uint32_t height, const UiFrame &ui, const FadeOverlay &overlay) {
  return device_.RenderAndReadback(
      width, height, SDL_FColor{.1f, .11f, .14f, 1.f},
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        Draw(snapshot, width, height, ui, overlay, command, pass);
      },
      true);
}

std::size_t FrameRenderer::cached_asset_count() const {
  return scene_->cached_asset_count();
}

} // namespace mana::gpu
