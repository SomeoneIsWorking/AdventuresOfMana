#include "host/render_ui.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "mcf/mcf.h"

namespace mana {
namespace {

constexpr std::array<float, 4> kPanel{0.05f, 0.07f, 0.18f, 0.85f};
constexpr std::array<float, 4> kBorder{0.55f, 0.65f, 0.95f, 0.90f};
constexpr std::array<float, 4> kText{1.f, 1.f, 1.f, 1.f};

class UiBuilder {
public:
  UiBuilder(const mcf::Font &font, std::uint32_t width, std::uint32_t height)
      : font_(font), width_(width), height_(height) {
    if (!width_ || !height_)
      throw std::invalid_argument("UI frame dimensions must be nonzero");
    if (!font_.width() || !font_.height() || !font_.line_height())
      throw std::invalid_argument("UI frame requires a loaded font atlas");
  }

  void Solid(float x0, float y0, float x1, float y1,
             std::array<float, 4> color) {
    Quad(x0, y0, x1, y1, 0.f, 0.f, 1.f, 1.f);
    Flush(color, false);
    ++frame_.solid_quads;
  }

  float Measure(const std::vector<std::uint32_t> &codepoints,
                float scale) const {
    float width = 0.f;
    for (const auto codepoint : codepoints)
      if (const auto *glyph = font_.Find(codepoint))
        width += float(glyph->Advance()) * scale;
    return width;
  }

  float Measure(const std::string &text, float scale) const {
    return Measure(mcf::Utf8Codepoints(text), scale);
  }

  void Text(const std::vector<std::vector<std::uint32_t>> &lines, float x,
            float y, float scale, float line_height) {
    const float atlas_width = float(font_.width());
    const float atlas_height = float(font_.height());
    for (const auto &line : lines) {
      float pen = x;
      for (const auto codepoint : line) {
        const auto *glyph = font_.Find(codepoint);
        if (!glyph) {
          if (!frame_.missing_glyphs)
            frame_.first_missing_codepoint = codepoint;
          ++frame_.missing_glyphs;
          continue;
        }
        if (glyph->w && glyph->h) {
          const float x0 = pen + float(glyph->left) * scale;
          const float y0 = y + float(glyph->top) * scale;
          Quad(x0, y0, x0 + float(glyph->w) * scale,
               y0 + float(glyph->h) * scale,
               float(glyph->x) / atlas_width,
               float(glyph->y) / atlas_height,
               float(glyph->x + glyph->w) / atlas_width,
               float(glyph->y + glyph->h) / atlas_height);
          ++frame_.glyph_quads;
        }
        pen += float(glyph->Advance()) * scale;
      }
      y += line_height;
    }
    Flush(kText, true);
  }

  void Text(const std::vector<std::string> &lines, float x, float y,
            float scale, float line_height) {
    std::vector<std::vector<std::uint32_t>> codepoints;
    codepoints.reserve(lines.size());
    for (const auto &line : lines)
      codepoints.push_back(mcf::Utf8Codepoints(line));
    Text(codepoints, x, y, scale, line_height);
  }

  std::vector<std::vector<std::uint32_t>> Wrap(const std::string &text,
                                                float scale,
                                                float available) const {
    std::vector<std::vector<std::uint32_t>> lines(1);
    for (const auto codepoint : mcf::Utf8Codepoints(text)) {
      if (codepoint == '\n') {
        lines.emplace_back();
        continue;
      }
      auto probe = lines.back();
      probe.push_back(codepoint);
      if (codepoint != ' ' && Measure(probe, scale) > available) {
        const auto space = std::find(lines.back().rbegin(),
                                     lines.back().rend(), std::uint32_t(' '));
        if (space != lines.back().rend()) {
          const auto split = space.base() - 1;
          std::vector<std::uint32_t> carry(split + 1, lines.back().end());
          lines.back().erase(split, lines.back().end());
          lines.push_back(std::move(carry));
          lines.back().push_back(codepoint);
        } else if (!lines.back().empty()) {
          lines.push_back({codepoint});
        } else {
          lines.back().push_back(codepoint);
        }
      } else {
        lines.back().push_back(codepoint);
      }
    }
    return lines;
  }

  UiFrame Finish() { return std::move(frame_); }

private:
  void Quad(float x0, float y0, float x1, float y1, float u0, float v0,
            float u1, float v1) {
    const auto x = [this](float pixel) {
      return pixel / float(width_) * 2.f - 1.f;
    };
    const auto y = [this](float pixel) {
      return 1.f - pixel / float(height_) * 2.f;
    };
    const std::array vertices{
        UiVertex{{x(x0), y(y0)}, {u0, v0}},
        UiVertex{{x(x1), y(y0)}, {u1, v0}},
        UiVertex{{x(x1), y(y1)}, {u1, v1}},
        UiVertex{{x(x0), y(y0)}, {u0, v0}},
        UiVertex{{x(x1), y(y1)}, {u1, v1}},
        UiVertex{{x(x0), y(y1)}, {u0, v1}},
    };
    frame_.vertices.insert(frame_.vertices.end(), vertices.begin(),
                           vertices.end());
  }

  void Flush(std::array<float, 4> color, bool textured) {
    const auto first = flushed_vertices_;
    const auto count = frame_.vertices.size() - first;
    if (!count)
      return;
    frame_.batches.push_back(
        {.first_vertex = static_cast<std::uint32_t>(first),
         .vertex_count = static_cast<std::uint32_t>(count),
         .color = color,
         .textured = textured});
    flushed_vertices_ = frame_.vertices.size();
  }

  const mcf::Font &font_;
  std::uint32_t width_;
  std::uint32_t height_;
  std::size_t flushed_vertices_ = 0;
  UiFrame frame_;
};

void BuildHud(UiBuilder &builder, const mcf::Font &font,
              const GameUiContent &content) {
  if (!content.show_hud || content.hud_rows.empty())
    return;
  constexpr float kScale = 2.f;
  const float glyph_scale =
      kScale * 17.f / float(font.line_height());
  constexpr float kMargin = 16.f;
  const float line_height = float(font.line_height()) * glyph_scale + kScale;
  float widest = 0.f;
  for (const auto &row : content.hud_rows)
    widest = std::max(widest, builder.Measure(row, glyph_scale));
  const float box_width = widest + 16.f;
  const float box_height =
      line_height * float(content.hud_rows.size()) + 12.f;
  builder.Solid(kMargin, kMargin, kMargin + box_width,
                kMargin + box_height, {0.05f, 0.07f, 0.18f, 0.72f});
  if (content.low_hp)
    builder.Solid(kMargin, kMargin + 4.f, kMargin + box_width,
                  kMargin + 4.f + line_height,
                  {0.6f, 0.1f, 0.1f, 0.8f});
  builder.Text(content.hud_rows, kMargin + 8.f, kMargin + 6.f, glyph_scale,
               line_height);
}

void BuildLevelUp(UiBuilder &builder, const mcf::Font &font,
                  std::uint32_t width, std::uint32_t height,
                  const GameUiContent &content) {
  if (!content.level_up_open || content.level_up_rows.empty())
    return;
  constexpr float kScale = 2.f;
  const float glyph_scale =
      kScale * 17.f / float(font.line_height());
  constexpr float kMargin = 40.f;
  const float line_height = float(font.line_height()) * glyph_scale + kScale;
  const float box_height =
      line_height * float(content.level_up_rows.size()) + 24.f;
  const float box_y = (float(height) - box_height) * .5f;
  builder.Solid(0.f, 0.f, float(width), float(height),
                {0.f, 0.f, 0.f, 0.55f});
  builder.Solid(kMargin, box_y, float(width) - kMargin, box_y + box_height,
                {0.05f, 0.07f, 0.18f, 0.92f});
  builder.Solid(kMargin, box_y, float(width) - kMargin, box_y + 2.f, kBorder);
  builder.Text(content.level_up_rows, kMargin + 16.f, box_y + 12.f,
               glyph_scale, line_height);
}

void BuildMessage(UiBuilder &builder, const mcf::Font &font,
                  std::uint32_t width, std::uint32_t height,
                  const std::string &message) {
  if (message.empty())
    return;
  constexpr float kScale = 2.f;
  const float glyph_scale =
      kScale * 17.f / float(font.line_height());
  constexpr float kPaddingX = 14.f;
  constexpr float kPaddingY = 10.f;
  constexpr float kMargin = 16.f;
  const float line_height = float(font.line_height()) * glyph_scale + kScale;
  const float available = float(width) - kMargin * 2.f - kPaddingX * 2.f;
  const auto lines = builder.Wrap(message, glyph_scale, available);
  const float box_height = line_height * float(lines.size()) + kPaddingY * 2.f;
  const float box_y = float(height) - box_height - kMargin;
  builder.Solid(kMargin, box_y, float(width) - kMargin, box_y + box_height,
                kPanel);
  builder.Solid(kMargin, box_y, float(width) - kMargin, box_y + 2.f, kBorder);
  builder.Text(lines, kMargin + kPaddingX, box_y + kPaddingY, glyph_scale,
               line_height);
}

} // namespace

UiFrame BuildGameUi(const mcf::Font &font, std::uint32_t width,
                    std::uint32_t height, const GameUiContent &content) {
  UiBuilder builder(font, width, height);
  BuildHud(builder, font, content);
  BuildLevelUp(builder, font, width, height, content);
  BuildMessage(builder, font, width, height, content.message);
  return builder.Finish();
}

} // namespace mana
