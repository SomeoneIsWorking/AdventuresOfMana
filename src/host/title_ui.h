#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "host/render_ui.h"

namespace mcf {
class Font;
class StringTable;
}

namespace mana {

enum class TitleUiScreen { kAttract, kMenu, kCrawl, kNames };

struct TitleUiState {
  TitleUiScreen screen = TitleUiScreen::kAttract;
  int frames = 0;
  int cursor = 0;
  int name_field = 0;
  int name_error = 0;
  float crawl_scroll = 0.f;
  std::array<std::string, 2> names;
  std::vector<std::string> crawl_lines;
};

UiFrame BuildTitleUi(const mcf::Font &font, const mcf::StringTable &strings,
                     std::uint32_t width, std::uint32_t height,
                     const TitleUiState &state);

} // namespace mana
