#pragma once

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include "host/render_asset.h"
#include "host/render_camera.h"

namespace mana {

struct RenderInstance {
  const mcf::RenderAsset *asset = nullptr;
  const mcf::Motion *motion = nullptr;
  std::array<float, 3> position{};
  float yaw = 0.f;
  float motion_time = 0.f;
};

// Immutable-for-consumers frame input shared by rendering backends. It owns no
// assets; the room cache retains those for at least the duration of the frame.
class RenderSnapshot {
public:
  explicit RenderSnapshot(CameraFrame camera_frame)
      : camera(std::move(camera_frame)) {}

  void Add(const mcf::RenderAsset &asset, std::array<float, 3> position,
           float yaw = 0.f, const mcf::Motion *motion = nullptr,
           float motion_time = 0.f);
  std::size_t skinned_count() const;

  CameraFrame camera;
  std::vector<RenderInstance> instances;
};

int RunRenderSnapshotSelfTest();

} // namespace mana
