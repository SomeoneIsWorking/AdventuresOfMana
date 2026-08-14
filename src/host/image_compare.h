#pragma once

#include <cstdint>
#include <span>

namespace mana {

std::uint32_t PixelDifference(std::span<const std::uint8_t> left,
                              std::span<const std::uint8_t> right);

} // namespace mana
