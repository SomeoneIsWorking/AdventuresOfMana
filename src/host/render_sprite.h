#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "host/render_ui.h"

namespace mana {

struct SpriteImage {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint8_t> rgba;
};

SpriteImage DecodeSprite(std::span<const std::uint8_t> png);
std::array<UiVertex, 6> BuildAspectFitSprite(const SpriteImage &image,
                                             std::uint32_t viewport_width,
                                             std::uint32_t viewport_height);

} // namespace mana
