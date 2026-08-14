#include "tools/gpu_asset_selftest.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <lucent/log.h>

#include "host/gpu_asset.h"
#include "host/gpu_asset_pipeline.h"
#include "host/gpu_device.h"
#include "host/render_asset.h"
#include "mcf/mcf.h"

namespace mana::gpu {
namespace {

std::uint32_t PixelDifference(std::span<const std::uint8_t> left,
                              std::span<const std::uint8_t> right) {
  if (left.size() != right.size() || left.size() % 4 != 0)
    throw std::invalid_argument("pixel comparison dimensions differ");
  std::uint32_t different = 0;
  for (std::size_t offset = 0; offset < left.size(); offset += 4) {
    if (std::memcmp(left.data() + offset, right.data() + offset, 4) != 0)
      ++different;
  }
  return different;
}

std::vector<std::uint8_t>
DrawDepthLayers(Device &device, AssetPipeline &pipeline, std::uint32_t width,
                std::uint32_t height, const std::array<float, 16> &base,
                bool depth) {
  auto near_transform = base;
  auto far_transform = base;
  near_transform[14] -= .05f;
  far_transform[14] += .05f;
  constexpr SDL_FColor clear{0.f, 1.f, 1.f, 1.f};
  return device.RenderAndReadback(
      width, height, clear,
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        pipeline.Draw(command, pass, near_transform, true);
        pipeline.Draw(command, pass, far_transform, false);
      },
      depth);
}

} // namespace

int RunAssetPipelineSelfTest(const char *archive_path, bool negative_control) {
  mcf::Archive archive(archive_path);
  mcf::RenderAsset source;
  constexpr std::string_view name = "M0001_00_00";
  if (!mcf::LoadRenderAsset(archive, std::string(name), &source)) {
    lucent::error(
        "gpu", "ASSET SELFTEST FAIL: scanned archive for {}, matched 0", name);
    return 1;
  }
  Device device;
  Asset asset(device, source);
  AssetPipeline pipeline(device, asset);
  constexpr std::uint32_t width = 96;
  constexpr std::uint32_t height = 72;
  const auto pixels =
      pipeline.DrawAndReadback(width, height, !negative_control);
  const auto white_pixels =
      negative_control ? std::vector<std::uint8_t>{}
                       : pipeline.DrawAndReadback(width, height, true, false);
  constexpr std::array<std::uint8_t, 4> clear{0, 255, 255, 255};
  std::uint32_t changed = 0;
  std::array<bool, 256> red_values{};
  std::uint32_t distinct_red = 0;
  std::uint32_t texture_changed = 0;
  for (std::uint32_t i = 0; i < width * height; ++i) {
    const auto *pixel = pixels.data() + i * 4;
    if (!negative_control &&
        std::memcmp(pixel, white_pixels.data() + i * 4, 4) != 0)
      ++texture_changed;
    if (std::memcmp(pixel, clear.data(), 4) != 0) {
      ++changed;
      if (!red_values[pixel[0]]) {
        red_values[pixel[0]] = true;
        ++distinct_red;
      }
    }
  }
  const bool pass = negative_control ? changed == 0
                                     : changed > 0 && distinct_red >= 2 &&
                                           texture_changed > 0;
  if (!pass) {
    lucent::error(
        "gpu",
        "ASSET SELFTEST FAIL: scanned {} pixels from {} draws; {} "
        "changed from clear, {} distinct changed red values, {} differ from "
        "forced-white; expected {} changed class, at least {} red classes, "
        "and {} texture differences",
        width * height, asset.draws().size(), changed, distinct_red,
        texture_changed, negative_control ? "zero" : "nonzero",
        negative_control ? 0 : 2, negative_control ? 0 : 1);
    return 1;
  }

  std::uint32_t depth_control_differences = 0;
  if (!negative_control) {
    if (asset.blended_draw_count() != 0) {
      lucent::error(
          "gpu",
          "ASSET SELFTEST FAIL: depth discriminator scanned {} draws in {}; "
          "expected 0 blended, matched {}",
          asset.draws().size(), name, asset.blended_draw_count());
      return 1;
    }
    const auto base = pipeline.TopDownTransform();
    auto near_transform = base;
    near_transform[14] -= .05f;
    const auto near_pixels =
        pipeline.DrawAndReadback(width, height, near_transform, true);
    const auto depth_pixels =
        DrawDepthLayers(device, pipeline, width, height, base, true);
    const std::uint32_t depth_differences =
        PixelDifference(near_pixels, depth_pixels);
    AssetPipeline no_depth_pipeline(
        device, asset,
        PipelineFeatures{.depth_test = false, .material_blending = true});
    const auto no_depth_pixels =
        DrawDepthLayers(device, no_depth_pipeline, width, height, base, false);
    depth_control_differences = PixelDifference(near_pixels, no_depth_pixels);
    if (depth_differences != 0 || depth_control_differences == 0) {
      lucent::error(
          "gpu",
          "ASSET SELFTEST FAIL: depth scanned {} pixels; far redraw changed "
          "{} with depth and {} with depth disabled; expected 0 and nonzero",
          width * height, depth_differences, depth_control_differences);
      return 1;
    }
  }
  lucent::info(
      "gpu",
      "ASSET SELFTEST: loaded {}; uploaded {} draws; scanned {} pixels; "
      "{} changed from clear; {} distinct changed red values; {} differ from "
      "forced-white; depth control differs in {}",
      name, asset.draws().size(), width * height, changed, distinct_red,
      texture_changed, depth_control_differences);

  if (!negative_control) {
    constexpr std::string_view blend_name = "M0000_00_03";
    mcf::RenderAsset blend_source;
    if (!mcf::LoadRenderAsset(archive, std::string(blend_name),
                              &blend_source)) {
      lucent::error("gpu",
                    "ASSET SELFTEST FAIL: scanned archive for {}, matched 0",
                    blend_name);
      return 1;
    }
    Asset blend_asset(device, blend_source);
    if (blend_asset.blended_draw_count() == 0) {
      lucent::error(
          "gpu",
          "ASSET SELFTEST FAIL: scanned {} draws in {}, matched 0 blended",
          blend_asset.draws().size(), blend_name);
      return 1;
    }
    AssetPipeline blend_pipeline(device, blend_asset);
    AssetPipeline opaque_control(
        device, blend_asset,
        PipelineFeatures{.depth_test = true, .material_blending = false});
    const auto blend_pixels =
        blend_pipeline.DrawAndReadback(width, height, true, true);
    const auto opaque_pixels =
        opaque_control.DrawAndReadback(width, height, true, true);
    const std::uint32_t blend_differences =
        PixelDifference(blend_pixels, opaque_pixels);
    if (blend_differences == 0) {
      lucent::error(
          "gpu",
          "ASSET SELFTEST FAIL: blend scanned {} pixels and {} blended draws "
          "in {}; 0 differ from opaque control",
          width * height, blend_asset.blended_draw_count(), blend_name);
      return 1;
    }
    lucent::info(
        "gpu",
        "ASSET SELFTEST: blend scanned {} pixels and {} blended draws in {}; "
        "{} differ from opaque control",
        width * height, blend_asset.blended_draw_count(), blend_name,
        blend_differences);
  }
  return 0;
}

} // namespace mana::gpu
