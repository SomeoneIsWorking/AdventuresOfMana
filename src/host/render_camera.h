#pragma once

#include <array>

#include "engine/world.h"

namespace mana {

struct CameraFrame {
  std::array<float, 3> eye{};
  std::array<float, 3> target{};
  float vertical_fov_radians = 0.f;
  float near_plane = 0.f;
  float far_plane = 0.f;
};

// Resolves script camera state into one backend-independent frame while
// retaining only the shipping per-frame eye interpolation state.
class CameraTracker {
public:
  CameraFrame Update(const mcf::World &world,
                     const std::array<float, 3> &player_world,
                     const std::array<float, 3> &room_origin,
                     float delta_seconds);
  void Reset() { initialized_ = false; }

private:
  std::array<float, 3> eye_{};
  bool initialized_ = false;
};

int RunCameraTrackerSelfTest();

} // namespace mana
