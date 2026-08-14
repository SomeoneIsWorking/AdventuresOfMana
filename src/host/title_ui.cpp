#include "host/title_ui.h"

#include <cmath>

#include "engine/mode.h"
#include "mcf/mcf.h"

namespace mana {
namespace {

std::string Text(const mcf::StringTable &strings, const char *id) {
  const std::string *value = strings.Find(id);
  return value ? *value : std::string(id);
}

void Add(std::vector<UiText> &commands, std::string text, float x, float y,
         float scale, bool centered, std::array<float, 4> color) {
  commands.push_back({.text = std::move(text),
                      .x = x,
                      .y = y,
                      .scale = scale,
                      .centered = centered,
                      .color = color});
}

} // namespace

UiFrame BuildTitleUi(const mcf::Font &font, const mcf::StringTable &strings,
                     std::uint32_t width, std::uint32_t height,
                     const TitleUiState &state) {
  constexpr float kLayoutScale = 2.f;
  constexpr float kDesignLineHeight = 17.f;
  const float center = float(width) * .5f;
  const float glyph = kLayoutScale * kDesignLineHeight /
                      float(font.line_height());
  const float large = glyph * 1.4f;
  std::vector<UiText> commands;
  if (state.screen != TitleUiScreen::kCrawl &&
      state.screen != TitleUiScreen::kNames)
    Add(commands, Text(strings, mcf::TitleMenu::kCopyrightId), center,
        float(height) - 44.f, glyph, true, {1.f, 1.f, 1.f, .75f});

  if (state.screen == TitleUiScreen::kCrawl) {
    const float app_scale = float(height) / 544.f;
    for (std::size_t index = 0; index < state.crawl_lines.size(); ++index) {
      const float app_y = state.crawl_scroll + mcf::OpeningCrawl::kFirstY +
                          mcf::OpeningCrawl::kLineStep * float(index);
      const float alpha = mcf::OpeningCrawl::Alpha(app_y);
      if (alpha <= 0.f)
        continue;
      const float y = app_y * app_scale;
      Add(commands, state.crawl_lines[index], center + 2.f, y + 2.f, large,
          true, {.25f, .25f, .25f, alpha});
      Add(commands, state.crawl_lines[index], center, y, large, true,
          {.94f, .94f, .94f, alpha});
    }
    Add(commands, Text(strings, mcf::OpeningCrawl::kSkipId) + "  [Shift]",
        center, float(height) - 40.f, glyph, true, {1.f, 1.f, 1.f, .7f});
  } else if (state.screen == TitleUiScreen::kNames) {
    Add(commands, Text(strings, "SYS_NAMEENTRY_INFO_1"), center,
        float(height) * .30f, glyph, true, {1.f, 1.f, 1.f, 1.f});
    Add(commands, Text(strings, "SYS_NAMEENTRY_INFO_2"), center,
        float(height) * .35f, glyph, true, {1.f, 1.f, 1.f, .8f});
    constexpr const char *labels[]{"SYS_NAMEENTRY_HERO_NAME_TITLE",
                                   "SYS_NAMEENTRY_GIRL_NAME_TITLE"};
    float y = float(height) * .50f;
    for (int field = 0; field < 2; ++field) {
      const bool selected = field == state.name_field;
      Add(commands, Text(strings, labels[field]) + ":", center - 240.f, y,
          large, false, {1.f, 1.f, selected ? .55f : 1.f, 1.f});
      std::string shown = state.names[field];
      if (selected && (state.frames / 20) % 2)
        shown += '_';
      Add(commands, std::move(shown), center + 20.f, y, large, false,
          {1.f, 1.f, 1.f, 1.f});
      y += 52.f;
    }
    if (const char *error = mcf::NameEntry::ErrorId(state.name_error))
      Add(commands, Text(strings, error), center, float(height) * .68f, glyph,
          true, {1.f, .45f, .4f, 1.f});
    Add(commands, Text(strings, "SYS_NAMEENTRY_BUTTON_DECIDE") +
                      "  [Enter]   Tab: switch",
        center, float(height) * .78f, glyph, true, {1.f, 1.f, 1.f, .7f});
  } else if (state.screen == TitleUiScreen::kAttract) {
    const float phase = float(state.frames % 60) / 60.f;
    const float alpha = .35f + .65f * std::fabs(1.f - 2.f * phase);
    Add(commands, Text(strings, mcf::TitleMenu::kStartId), center,
        float(height) * .74f, large, true, {1.f, 1.f, 1.f, alpha});
  } else {
    float y = float(height) * .64f;
    for (int item = 0; item < mcf::TitleMenu::kItemCount; ++item) {
      const bool enabled = item == 0;
      const bool selected = item == state.cursor;
      const float value = enabled ? 1.f : .4f;
      Add(commands, Text(strings, mcf::TitleMenu::kItemId[item]), center, y,
          large, true,
          {value, value, selected ? .55f * value : value,
           enabled ? 1.f : .7f});
      if (selected)
        Add(commands, ">", center - 180.f, y, large, false,
            {1.f, .85f, .35f, 1.f});
      y += 40.f;
    }
  }
  return BuildTextUi(font, width, height, commands);
}

} // namespace mana
