#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mcf {
class Font;
}

namespace mana {

struct UiVertex {
  std::array<float, 2> position;
  std::array<float, 2> uv;
};

struct UiBatch {
  std::uint32_t first_vertex = 0;
  std::uint32_t vertex_count = 0;
  std::array<float, 4> color{};
  bool textured = false;
};

struct UiText {
  std::string text;
  float x = 0.f;
  float y = 0.f;
  float scale = 1.f;
  bool centered = false;
  std::array<float, 4> color{1.f, 1.f, 1.f, 1.f};
};

// Backend-independent UI command stream. Coordinates are converted to clip
// space here so every graphics backend consumes identical geometry and order.
struct UiFrame {
  std::vector<UiVertex> vertices;
  std::vector<UiBatch> batches;
  std::uint32_t solid_quads = 0;
  std::uint32_t glyph_quads = 0;
  std::uint32_t missing_glyphs = 0;
  std::uint32_t first_missing_codepoint = 0;
};

// The engine-facing host formats authored strings and live values once; this
// layout owner turns that content into panels and glyphs without knowing game
// state or a graphics API.
struct GameUiContent {
  bool show_hud = false;
  bool low_hp = false;
  bool level_up_open = false;
  std::vector<std::string> hud_rows;
  std::vector<std::string> level_up_rows;
  std::string message;
};

UiFrame BuildGameUi(const mcf::Font &font, std::uint32_t width,
                    std::uint32_t height, const GameUiContent &content);
UiFrame BuildTextUi(const mcf::Font &font, std::uint32_t width,
                    std::uint32_t height,
                    const std::vector<UiText> &commands);

} // namespace mana
