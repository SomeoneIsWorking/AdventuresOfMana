#include "tools/gpu_asset_selftest.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <lucent/log.h>

#include "host/gpu_asset.h"
#include "host/gpu_asset_pipeline.h"
#include "host/gpu_device.h"
#include "host/gpu_scene.h"
#include "host/gpu_snapshot_renderer.h"
#include "host/image_compare.h"
#include "host/image_write.h"
#include "host/render_asset.h"
#include "host/render_pose.h"
#include "host/render_snapshot.h"
#include "mcf/mcf.h"

namespace mana::gpu {
namespace {

std::uint32_t ChangedFromClear(std::span<const std::uint8_t> pixels) {
  constexpr std::array<std::uint8_t, 4> clear{0, 255, 255, 255};
  std::uint32_t changed = 0;
  for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
    if (std::memcmp(pixels.data() + offset, clear.data(), clear.size()) != 0)
      ++changed;
  }
  return changed;
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

int RunAssetPipelineSelfTest(const char *archive_path, bool negative_control,
                             const char *capture_path) {
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

    constexpr std::string_view skinned_name = "C0000_00";
    mcf::RenderAsset skinned_source;
    if (!mcf::LoadRenderAsset(archive, std::string(skinned_name),
                              &skinned_source)) {
      lucent::error("gpu",
                    "ASSET SELFTEST FAIL: scanned archive for {}, matched 0",
                    skinned_name);
      return 1;
    }
    Asset skinned_asset(device, skinned_source);
    if (!skinned_source.skinned() || !skinned_asset.skinned()) {
      lucent::error(
          "gpu",
          "ASSET SELFTEST FAIL: scanned {} vertices in {}; expected shipping "
          "skinning attributes, source={} GPU={}",
          skinned_source.model.vertex_count,
          skinned_name, skinned_source.skinned(), skinned_asset.skinned());
      return 1;
    }
    std::vector<float> bind_joints;
    mcf::BuildJointPalette(skinned_source.model, nullptr, 0.f, &bind_joints);
    AssetPipeline skinned_pipeline(device, skinned_asset);
    const auto transform = skinned_pipeline.TopDownTransform();
    const auto bind_pixels = skinned_pipeline.DrawAndReadback(
        width, height, transform, true, bind_joints);
    auto shifted_joints = bind_joints;
    const float shift = (skinned_source.hi[0] - skinned_source.lo[0]) * .2f;
    for (std::size_t bone = 0; bone < 80; ++bone)
      shifted_joints[bone * 12 + 3] += shift;
    const auto shifted_pixels = skinned_pipeline.DrawAndReadback(
        width, height, transform, true, shifted_joints);
    const std::uint32_t skinned_changed = ChangedFromClear(bind_pixels);
    const std::uint32_t shifted_differences =
        PixelDifference(bind_pixels, shifted_pixels);
    bool missing_palette_failed = false;
    std::string missing_palette_message;
    try {
      (void)skinned_pipeline.DrawAndReadback(width, height, transform, true);
    } catch (const std::invalid_argument &error) {
      missing_palette_message = error.what();
      missing_palette_failed =
          missing_palette_message ==
          "skinned asset requires 960 joint floats, received 0";
    }
    if (skinned_changed == 0 || shifted_differences == 0 ||
        !missing_palette_failed) {
      lucent::error(
          "gpu",
          "ASSET SELFTEST FAIL: skinning scanned {} pixels and {} vertices in "
          "{}; {} changed from clear, {} differ after shifting all 80 joints; "
          "missing-palette discriminator={} ({})",
          width * height,
          skinned_source.model.vertex_count,
          skinned_name, skinned_changed, shifted_differences,
          missing_palette_failed, missing_palette_message);
      return 1;
    }
    lucent::info(
        "gpu",
        "ASSET SELFTEST: skinning scanned {} pixels and {} vertices in {}; {} "
        "changed from clear, {} differ after shifting all 80 joints; missing "
        "960-float palette rejected",
        width * height,
        skinned_source.model.vertex_count,
        skinned_name, skinned_changed, shifted_differences);

    SceneRenderer scene(device);
    constexpr std::uint32_t scene_width = 320;
    constexpr std::uint32_t scene_height = 240;
    AssetPipeline blend_order_pipeline(
        device, blend_asset,
        PipelineFeatures{.depth_test = false, .material_blending = true});
    AssetPipeline static_order_pipeline(
        device, asset,
        PipelineFeatures{.depth_test = false, .material_blending = true});
    SceneRenderer order_scene(device, false);
    const auto blend_transform = blend_order_pipeline.TopDownTransform();
    const auto static_transform = static_order_pipeline.TopDownTransform();
    const std::array ordered_material_draws{
        SceneDraw{.pipeline = &blend_order_pipeline,
                  .transform = blend_transform},
        SceneDraw{.pipeline = &static_order_pipeline,
                  .transform = static_transform}};
    const auto ordered_material_pixels =
        order_scene.DrawAndReadback(width, height, ordered_material_draws);
    constexpr SDL_FColor scene_clear{0.f, 1.f, 1.f, 1.f};
    const auto per_asset_order_pixels = device.RenderAndReadback(
        width, height, scene_clear,
        [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
          blend_order_pipeline.Draw(command, pass, blend_transform);
          static_order_pipeline.Draw(command, pass, static_transform);
        },
        false);
    const std::uint32_t material_order_differences =
        PixelDifference(ordered_material_pixels, per_asset_order_pixels);
    if (material_order_differences == 0) {
      lucent::error(
          "gpu",
          "ASSET SELFTEST FAIL: scene material-order discriminator scanned {} "
          "pixels across {}+{} draws; 0 differ from per-asset ordering",
          width * height, blend_asset.draws().size(), asset.draws().size());
      return 1;
    }
    const float actor_x = (source.lo[0] + source.hi[0]) * .5f;
    const float actor_y = -skinned_source.lo[1];
    const float actor_z = (source.lo[2] + source.hi[2]) * .5f;
    const std::array<float, 3> target{actor_x, 20.f, actor_z};
    constexpr float camera_distance = 450.f;
    constexpr float camera_pitch = 20.f * std::numbers::pi_v<float> / 180.f;
    const std::array<float, 3> eye{
        actor_x, target[1] + std::sin(camera_pitch) * camera_distance,
        actor_z + std::cos(camera_pitch) * camera_distance};
    CameraFrame snapshot_camera{.eye = eye,
                                .target = target,
                                .vertical_fov_radians =
                                    40.f * std::numbers::pi_v<float> / 180.f,
                                .near_plane = 40.f,
                                .far_plane = 5000.f};
    const auto room_transform = MultiplyTransform(
        PerspectiveTransform(40.f * std::numbers::pi_v<float> / 180.f,
                             float(scene_width) / float(scene_height), 40.f,
                             5000.f),
        LookAtTransform(eye, target, {0.f, 1.f, 0.f}));
    const auto actor_transform = MultiplyTransform(
        room_transform, TranslationTransform(actor_x, actor_y, actor_z));
    const std::array room_draws{
        SceneDraw{.pipeline = &pipeline, .transform = room_transform}};
    const std::array combined_draws{
        SceneDraw{.pipeline = &pipeline, .transform = room_transform},
        SceneDraw{.pipeline = &skinned_pipeline,
                  .transform = actor_transform,
                  .joints = bind_joints}};
    const auto outside_transform = MultiplyTransform(
        room_transform,
        TranslationTransform(actor_x + (source.hi[0] - source.lo[0]) * 2.f,
                             actor_y, actor_z));
    const std::array outside_draws{
        SceneDraw{.pipeline = &pipeline, .transform = room_transform},
        SceneDraw{.pipeline = &skinned_pipeline,
                  .transform = outside_transform,
                  .joints = bind_joints}};
    constexpr SDL_FColor live_scene_clear{.1f, .11f, .14f, 1.f};
    const auto room_pixels =
        scene.DrawAndReadback(scene_width, scene_height, room_draws,
                              live_scene_clear);
    const auto combined_pixels =
        scene.DrawAndReadback(scene_width, scene_height, combined_draws,
                              live_scene_clear);
    RenderSnapshot snapshot(snapshot_camera);
    snapshot.Add(source, {0.f, 0.f, 0.f});
    snapshot.Add(skinned_source, {actor_x, actor_y, actor_z});
    SnapshotRenderer snapshot_renderer(device);
    const auto snapshot_pixels = snapshot_renderer.DrawAndReadback(
        snapshot, scene_width, scene_height);
    const std::uint32_t snapshot_differences =
        PixelDifference(combined_pixels, snapshot_pixels);
    if (capture_path) {
      mana::WritePng(capture_path, scene_width, scene_height, combined_pixels);
      lucent::info("gpu", "ASSET SELFTEST: wrote scene capture {}",
                   capture_path);
    }
    const auto outside_pixels =
        scene.DrawAndReadback(scene_width, scene_height, outside_draws,
                              live_scene_clear);
    const std::uint32_t actor_differences =
        PixelDifference(room_pixels, combined_pixels);
    const std::uint32_t outside_differences =
        PixelDifference(room_pixels, outside_pixels);
    bool unnamed_asset_failed = false;
    std::string unnamed_asset_message;
    try {
      mcf::RenderAsset unnamed;
      RenderSnapshot invalid_snapshot(snapshot_camera);
      invalid_snapshot.Add(unnamed, {0.f, 0.f, 0.f});
      (void)snapshot_renderer.DrawAndReadback(invalid_snapshot, scene_width,
                                              scene_height);
    } catch (const std::invalid_argument &error) {
      unnamed_asset_message = error.what();
      unnamed_asset_failed =
          unnamed_asset_message == "render snapshot asset has no shipping name";
    }
    if (actor_differences == 0 || outside_differences != 0 ||
        snapshot_differences != 0 ||
        snapshot_renderer.cached_asset_count() != 2 || !unnamed_asset_failed) {
      lucent::error(
          "gpu",
          "ASSET SELFTEST FAIL: scene scanned {} pixels, {} room draws and {} "
          "actor draws; centered actor changed {}, offscreen actor changed {}; "
          "snapshot differs in {}, cache holds {} assets, unnamed-asset "
          "negative={} ({}); expected nonzero, zero, zero, 2, true",
          scene_width * scene_height, asset.draws().size(),
          skinned_asset.draws().size(),
          actor_differences, outside_differences, snapshot_differences,
          snapshot_renderer.cached_asset_count(), unnamed_asset_failed,
          unnamed_asset_message);
      return 1;
    }
    lucent::info(
        "gpu",
        "ASSET SELFTEST: scene scanned {} pixels, {} room draws and {} actor "
        "draws; centered actor changed {}, offscreen actor changed 0; {} "
        "pixels discriminate scene-wide material ordering; snapshot adapter "
        "matches 0 pixels different with 2 cached assets; unnamed asset "
        "rejected",
        scene_width * scene_height, asset.draws().size(),
        skinned_asset.draws().size(),
        actor_differences, material_order_differences);
  }
  return 0;
}

} // namespace mana::gpu
