#include "host/gpu_snapshot_renderer.h"

#include <array>
#include <cmath>
#include <stdexcept>

#include "host/gpu_asset.h"
#include "host/gpu_asset_pipeline.h"
#include "host/gpu_device.h"
#include "host/gpu_scene.h"
#include "host/render_pose.h"

namespace mana::gpu {
namespace {

std::array<float, 16> ModelTransform(const std::array<float, 3> &position,
                                     float yaw) {
  const float cosine = std::cos(yaw);
  const float sine = std::sin(yaw);
  return {cosine, 0.f, -sine, 0.f, 0.f, 1.f, 0.f, 0.f,
          sine,   0.f, cosine, 0.f, position[0], position[1], position[2], 1.f};
}

} // namespace

SnapshotRenderer::SnapshotRenderer(Device &device)
    : device_(device), scene_(std::make_unique<SceneRenderer>(device)) {}

SnapshotRenderer::~SnapshotRenderer() = default;

SnapshotRenderer::CachedAsset &
SnapshotRenderer::RequireAsset(const mcf::RenderAsset &source) {
  if (source.name.empty())
    throw std::invalid_argument("render snapshot asset has no shipping name");
  auto [iterator, inserted] = cache_.try_emplace(source.name);
  if (!inserted)
    return iterator->second;
  try {
    iterator->second.asset = std::make_unique<Asset>(device_, source);
    iterator->second.pipeline = std::make_unique<AssetPipeline>(
        device_, *iterator->second.asset);
  } catch (...) {
    cache_.erase(iterator);
    throw;
  }
  return iterator->second;
}

std::vector<std::uint8_t>
SnapshotRenderer::DrawAndReadback(const RenderSnapshot &snapshot,
                                  std::uint32_t width,
                                  std::uint32_t height) {
  constexpr SDL_FColor clear{.1f, .11f, .14f, 1.f};
  return device_.RenderAndReadback(
      width, height, clear,
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        Draw(snapshot, width, height, command, pass);
      },
      true);
}

void SnapshotRenderer::Draw(const RenderSnapshot &snapshot,
                            std::uint32_t width, std::uint32_t height,
                            SDL_GPUCommandBuffer *command,
                            SDL_GPURenderPass *pass) {
  if (snapshot.instances.empty())
    throw std::invalid_argument("render snapshot contains 0 instances");
  const auto projection = PerspectiveTransform(
      snapshot.camera.vertical_fov_radians, float(width) / float(height),
      snapshot.camera.near_plane, snapshot.camera.far_plane);
  const auto view = LookAtTransform(snapshot.camera.eye, snapshot.camera.target,
                                    {0.f, 1.f, 0.f});
  const auto view_projection = MultiplyTransform(projection, view);

  std::vector<std::vector<float>> palettes;
  palettes.reserve(snapshot.instances.size());
  std::vector<SceneDraw> draws;
  draws.reserve(snapshot.instances.size());
  for (const auto &instance : snapshot.instances) {
    if (!instance.asset)
      throw std::invalid_argument("render snapshot instance has no asset");
    auto &cached = RequireAsset(*instance.asset);
    const auto transform = MultiplyTransform(
        view_projection, ModelTransform(instance.position, instance.yaw));
    std::span<const float> joints;
    if (instance.asset->skinned()) {
      palettes.emplace_back();
      mcf::BuildJointPalette(instance.asset->model, instance.motion,
                             instance.motion_time, &palettes.back());
      joints = palettes.back();
    }
    draws.push_back({.pipeline = cached.pipeline.get(),
                     .transform = transform,
                     .joints = joints});
  }
  scene_->Draw(command, pass, draws);
}

} // namespace mana::gpu
