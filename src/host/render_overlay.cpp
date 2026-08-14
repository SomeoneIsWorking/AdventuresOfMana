#include "host/render_overlay.h"

#include <cmath>
#include <stdexcept>

namespace mana {

FadeOverlay
FadeOverlay::FromEngineColor(std::span<const std::uint8_t, 3> rgb,
                             float coverage) {
  if (!std::isfinite(coverage) || coverage < 0.f || coverage > 1.f)
    throw std::invalid_argument("fade coverage must be within [0,1]");
  return {.color = {rgb[0] / 255.f, rgb[1] / 255.f, rgb[2] / 255.f,
                    coverage}};
}

} // namespace mana
