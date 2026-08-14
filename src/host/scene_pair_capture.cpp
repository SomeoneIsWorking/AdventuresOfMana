#include "host/scene_pair_capture.h"

#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include <lucent/log.h>

#include "host/gpu_device.h"
#include "host/gpu_snapshot_renderer.h"
#include "host/image_compare.h"
#include "host/image_write.h"
#include "host/render_snapshot.h"

namespace mana {

class ScenePairCapture::Impl {
public:
  explicit Impl(std::string output_prefix)
      : prefix(std::move(output_prefix)), renderer(device) {
    if (prefix.empty())
      throw std::invalid_argument("scene-pair output prefix is empty");
  }

  std::string prefix;
  gpu::Device device;
  gpu::SnapshotRenderer renderer;
};

ScenePairCapture::ScenePairCapture(std::string output_prefix)
    : impl_(std::make_unique<Impl>(std::move(output_prefix))) {}

ScenePairCapture::~ScenePairCapture() = default;

const char *ScenePairCapture::driver() const { return impl_->device.driver(); }

void ScenePairCapture::Write(
    const RenderSnapshot &snapshot, std::uint32_t width, std::uint32_t height,
    std::span<const std::uint8_t> gles_bottom_up_rgba) {
  const std::size_t expected = std::size_t(width) * height * 4;
  if (gles_bottom_up_rgba.size() != expected)
    throw std::invalid_argument("GLES scene capture dimensions differ");

  std::vector<std::uint8_t> gles_top_down(expected);
  for (std::uint32_t y = 0; y < height; ++y)
    std::memcpy(gles_top_down.data() + std::size_t(y) * width * 4,
                gles_bottom_up_rgba.data() +
                    std::size_t(height - 1 - y) * width * 4,
                std::size_t(width) * 4);
  const auto gpu_pixels =
      impl_->renderer.DrawAndReadback(snapshot, width, height);
  const std::string gl_path = impl_->prefix + "-gles.png";
  const std::string gpu_path = impl_->prefix + "-sdl3.png";
  WritePng(gl_path, width, height, gles_top_down);
  WritePng(gpu_path, width, height, gpu_pixels);
  const auto different = PixelDifference(gles_top_down, gpu_pixels);
  lucent::info(
      "gpu",
      "live snapshot pair: {} instances ({} skinned), {} cached assets; {} "
      "of {} pixels differ; wrote {} and {}",
      snapshot.instances.size(), snapshot.skinned_count(),
      impl_->renderer.cached_asset_count(), different, width * height, gl_path,
      gpu_path);
}

} // namespace mana
