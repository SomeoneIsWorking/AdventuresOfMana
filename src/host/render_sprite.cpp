#include "host/render_sprite.h"

#include <stdexcept>

#include "mcf/mcf.h"

namespace mana {

SpriteImage DecodeSprite(std::span<const std::uint8_t> png) {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;
  if (!mcf::DecodePng(std::vector<std::uint8_t>(png.begin(), png.end()),
                      &width, &height, &rgba) ||
      width <= 0 || height <= 0)
    throw std::invalid_argument("sprite PNG did not decode");
  return {.width = static_cast<std::uint32_t>(width),
          .height = static_cast<std::uint32_t>(height),
          .rgba = std::move(rgba)};
}

std::array<UiVertex, 6> BuildAspectFitSprite(const SpriteImage &image,
                                             std::uint32_t viewport_width,
                                             std::uint32_t viewport_height) {
  if (!image.width || !image.height ||
      image.rgba.size() != std::size_t(image.width) * image.height * 4)
    throw std::invalid_argument("aspect-fit sprite requires complete RGBA pixels");
  if (!viewport_width || !viewport_height)
    throw std::invalid_argument("aspect-fit sprite viewport is empty");
  const float sprite_aspect = float(image.width) / float(image.height);
  const float viewport_aspect = float(viewport_width) / float(viewport_height);
  const float extent_x =
      sprite_aspect > viewport_aspect ? 1.f : sprite_aspect / viewport_aspect;
  const float extent_y =
      sprite_aspect > viewport_aspect ? viewport_aspect / sprite_aspect : 1.f;
  return {{{{-extent_x, -extent_y}, {0.f, 1.f}},
           {{extent_x, -extent_y}, {1.f, 1.f}},
           {{extent_x, extent_y}, {1.f, 0.f}},
           {{-extent_x, -extent_y}, {0.f, 1.f}},
           {{extent_x, extent_y}, {1.f, 0.f}},
           {{-extent_x, extent_y}, {0.f, 0.f}}}};
}

} // namespace mana
