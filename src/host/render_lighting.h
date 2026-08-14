#pragma once

#include <array>

namespace mana {

// Port-enhanced surface light. Shipping assets have no normals and the
// original mLight path only applied positional colour attenuation, so this is
// deliberately separate from the vanilla configuration.
struct DirectionalLight {
  std::array<float, 3> direction_to_light{-0.45f, 0.80f, 0.40f};
  std::array<float, 3> color{1.f, 1.f, 1.f};
  float ambient = 0.55f;
  float diffuse = 0.45f;

  static constexpr DirectionalLight Vanilla() {
    return {.direction_to_light = {0.f, 1.f, 0.f},
            .color = {1.f, 1.f, 1.f},
            .ambient = 1.f,
            .diffuse = 0.f};
  }

  static constexpr DirectionalLight AmbientOnly(float level) {
    return {.direction_to_light = {0.f, 1.f, 0.f},
            .color = {1.f, 1.f, 1.f},
            .ambient = level,
            .diffuse = 0.f};
  }

  DirectionalLight ForModelYaw(float yaw) const;
};

} // namespace mana
