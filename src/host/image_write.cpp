#include "host/image_write.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <format>
#include <stdexcept>
#include <vector>

namespace mana {
namespace {

std::uint32_t Crc32(std::span<const std::uint8_t> data) {
  static const auto table = [] {
    std::array<std::uint32_t, 256> result{};
    for (std::uint32_t i = 0; i < result.size(); ++i) {
      std::uint32_t value = i;
      for (int bit = 0; bit < 8; ++bit)
        value = (value & 1) ? 0xedb88320u ^ (value >> 1) : value >> 1;
      result[i] = value;
    }
    return result;
  }();
  std::uint32_t crc = 0xffffffffu;
  for (const auto byte : data)
    crc = table[(crc ^ byte) & 0xff] ^ (crc >> 8);
  return ~crc;
}

void Put32(std::vector<std::uint8_t> &output, std::uint32_t value) {
  output.push_back(std::uint8_t(value >> 24));
  output.push_back(std::uint8_t(value >> 16));
  output.push_back(std::uint8_t(value >> 8));
  output.push_back(std::uint8_t(value));
}

void AppendChunk(std::vector<std::uint8_t> &output, const char tag[4],
                 std::span<const std::uint8_t> data) {
  Put32(output, std::uint32_t(data.size()));
  const std::size_t body_offset = output.size();
  output.insert(output.end(), tag, tag + 4);
  output.insert(output.end(), data.begin(), data.end());
  Put32(output, Crc32(std::span(output).subspan(body_offset)));
}

} // namespace

void WritePng(const std::string &path, std::uint32_t width,
              std::uint32_t height, std::span<const std::uint8_t> rgba) {
  const std::size_t expected = std::size_t(width) * height * 4;
  if (width == 0 || height == 0 || rgba.size() != expected)
    throw std::invalid_argument(std::format(
        "PNG {}x{} requires {} RGBA bytes, received {}", width, height,
        expected, rgba.size()));

  std::vector<std::uint8_t> raw;
  raw.reserve(std::size_t(height) * (std::size_t(width) * 4 + 1));
  for (std::uint32_t y = 0; y < height; ++y) {
    raw.push_back(0);
    const auto row = rgba.subspan(std::size_t(y) * width * 4,
                                  std::size_t(width) * 4);
    raw.insert(raw.end(), row.begin(), row.end());
  }

  std::vector<std::uint8_t> compressed{0x78, 0x01};
  std::uint32_t adler_a = 1;
  std::uint32_t adler_b = 0;
  for (const auto byte : raw) {
    adler_a = (adler_a + byte) % 65521;
    adler_b = (adler_b + adler_a) % 65521;
  }
  for (std::size_t offset = 0; offset < raw.size(); offset += 65535) {
    const auto count = std::uint16_t(
        std::min<std::size_t>(65535, raw.size() - offset));
    compressed.push_back(offset + count >= raw.size() ? 1 : 0);
    compressed.push_back(std::uint8_t(count));
    compressed.push_back(std::uint8_t(count >> 8));
    compressed.push_back(std::uint8_t(~count));
    compressed.push_back(std::uint8_t(~count >> 8));
    compressed.insert(compressed.end(), raw.begin() + long(offset),
                      raw.begin() + long(offset + count));
  }
  Put32(compressed, (adler_b << 16) | adler_a);

  std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::vector<std::uint8_t> header;
  Put32(header, width);
  Put32(header, height);
  header.insert(header.end(), {8, 6, 0, 0, 0});
  AppendChunk(png, "IHDR", header);
  AppendChunk(png, "IDAT", compressed);
  AppendChunk(png, "IEND", {});

  FILE *file = std::fopen(path.c_str(), "wb");
  if (!file)
    throw std::runtime_error(
        std::format("cannot open PNG '{}': {}", path, std::strerror(errno)));
  const std::size_t written = std::fwrite(png.data(), 1, png.size(), file);
  const int close_result = std::fclose(file);
  if (written != png.size() || close_result != 0)
    throw std::runtime_error(std::format(
        "incomplete PNG '{}': wrote {} of {} bytes", path, written,
        png.size()));
}

} // namespace mana
