#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace mana {

// Writes one unfiltered RGBA8 image as a portable PNG and refuses incomplete
// input or output failures instead of leaving a plausible truncated capture.
void WritePng(const std::string &path, std::uint32_t width,
              std::uint32_t height, std::span<const std::uint8_t> rgba);

} // namespace mana
