#include "host/render_lighting.h"

#include <cmath>

namespace mana {

DirectionalLight DirectionalLight::ForModelYaw(float yaw) const {
  const float cosine = std::cos(yaw);
  const float sine = std::sin(yaw);
  auto result = *this;
  result.direction_to_light = {
      cosine * direction_to_light[0] - sine * direction_to_light[2],
      direction_to_light[1],
      sine * direction_to_light[0] + cosine * direction_to_light[2]};
  return result;
}

} // namespace mana
