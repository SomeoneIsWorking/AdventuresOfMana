#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace mana {

// Backend-independent full-frame colour overlay. Fade timing remains engine
// state; renderers receive only the resolved colour and coverage for one frame.
struct FadeOverlay {
  std::array<float, 4> color{};

  static FadeOverlay FromEngineColor(std::span<const std::uint8_t, 3> rgb,
                                     float coverage);
  bool visible() const { return color[3] > 0.f; }
};

} // namespace mana
