#include "host/image_compare.h"

#include <cstring>
#include <stdexcept>

namespace mana {

std::uint32_t PixelDifference(std::span<const std::uint8_t> left,
                              std::span<const std::uint8_t> right) {
  if (left.size() != right.size() || left.size() % 4 != 0)
    throw std::invalid_argument("pixel comparison dimensions differ");
  std::uint32_t different = 0;
  for (std::size_t offset = 0; offset < left.size(); offset += 4) {
    if (std::memcmp(left.data() + offset, right.data() + offset, 4) != 0)
      ++different;
  }
  return different;
}

} // namespace mana
