#include "host/scene_pair_capture.h"

#include <GLES2/gl2.h>

#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include <lucent/log.h>

#include "host/gpu_device.h"
#include "host/gpu_frame_renderer.h"
#include "host/image_compare.h"
#include "host/image_write.h"
#include "host/render_snapshot.h"
#include "host/render_overlay.h"
#include "host/render_ui.h"

namespace mana {

class ScenePairCapture::Impl {
public:
  Impl(std::string output_prefix, const mcf::Font &font)
      : prefix(std::move(output_prefix)), frame(device, font) {
    if (prefix.empty())
      throw std::invalid_argument("scene-pair output prefix is empty");
  }

  std::string prefix;
  gpu::Device device;
  gpu::FrameRenderer frame;
};

ScenePairCapture::ScenePairCapture(std::string output_prefix,
                                   const mcf::Font &font)
    : impl_(std::make_unique<Impl>(std::move(output_prefix), font)) {}

ScenePairCapture::~ScenePairCapture() = default;

const char *ScenePairCapture::driver() const { return impl_->device.driver(); }

void ScenePairCapture::WriteFromGles(const RenderSnapshot &snapshot,
                                     std::uint32_t width,
                                     std::uint32_t height,
                                     const UiFrame &ui_frame,
                                     const FadeOverlay &overlay) {
  const std::size_t expected = std::size_t(width) * height * 4;
  std::vector<std::uint8_t> gles_bottom_up(expected);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
               gles_bottom_up.data());

  std::vector<std::uint8_t> gles_top_down(expected);
  for (std::uint32_t y = 0; y < height; ++y)
    std::memcpy(gles_top_down.data() + std::size_t(y) * width * 4,
                gles_bottom_up.data() +
                    std::size_t(height - 1 - y) * width * 4,
                std::size_t(width) * 4);
  const auto gpu_pixels = impl_->frame.DrawAndReadback(
      snapshot, width, height, ui_frame, overlay);
  const std::string gl_path = impl_->prefix + "-gles.png";
  const std::string gpu_path = impl_->prefix + "-sdl3.png";
  WritePng(gl_path, width, height, gles_top_down);
  WritePng(gpu_path, width, height, gpu_pixels);
  const auto different = PixelDifference(gles_top_down, gpu_pixels);
  lucent::info(
      "gpu",
      "live snapshot pair: {} instances ({} skinned), {} cached assets, {} UI "
      "batches/{} glyph quads, fade coverage {:.3f}; {} of {} pixels differ; "
      "wrote {} and {}",
      snapshot.instances.size(), snapshot.skinned_count(),
      impl_->frame.cached_asset_count(), ui_frame.batches.size(),
      ui_frame.glyph_quads, overlay.color[3], different, width * height,
      gl_path, gpu_path);
}

} // namespace mana
