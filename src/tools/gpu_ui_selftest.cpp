#include <cstdint>
#include <cstring>
#include <exception>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <lucent/log.h>

#include "host/gpu_device.h"
#include "host/gpu_sprite.h"
#include "host/gpu_ui.h"
#include "host/image_compare.h"
#include "host/image_write.h"
#include "host/render_ui.h"
#include "host/render_sprite.h"
#include "host/title_ui.h"
#include "mcf/mcf.h"

namespace {

std::uint32_t Changed(std::span<const std::uint8_t> left,
                      std::span<const std::uint8_t> right) {
  return mana::PixelDifference(left, right);
}

int Run(const char *archive_path, const char *capture_path,
        bool atlas_negative) {
  mcf::Archive archive(archive_path);
  constexpr std::string_view font_path = "sk1/font_en.bin";
  if (!archive.Has(font_path)) {
    lucent::error("gpu-ui",
                  "UI SELFTEST FAIL: scanned archive for {}, matched 0",
                  font_path);
    return 1;
  }
  mcf::Font font;
  if (!font.LoadFontBin(archive.Read(font_path))) {
    lucent::error("gpu-ui",
                  "UI SELFTEST FAIL: {} was present but did not parse",
                  font_path);
    return 1;
  }
  mcf::StringTable strings;
  if (!archive.Has("sk1/str_en.bin") ||
      !strings.Load(archive.Read("sk1/str_en.bin"))) {
    lucent::error("gpu-ui",
                  "UI SELFTEST FAIL: shipping English strings are missing or "
                  "malformed");
    return 1;
  }

  constexpr std::uint32_t width = 192;
  constexpr std::uint32_t height = 128;
  mana::GameUiContent hud{
      .show_hud = true,
      .low_hp = true,
      .hud_rows = {"HP  4/19  MP 6/6", "GP 50  EXP 0/16", "ATK 6 DEF 7 Lv 1"},
  };
  const auto hud_frame = mana::BuildGameUi(font, width, height, hud);
  mana::GameUiContent level{
      .level_up_open = true,
      .level_up_rows = {"Select a training regimen.", "", "> Warrior",
                        "  Monk", "  Mage", "  Sage", "", "Power rises."},
  };
  const auto level_frame = mana::BuildGameUi(font, width, height, level);
  const auto multibyte = mcf::Utf8Codepoints("©");
  if (multibyte.size() != 1 || multibyte.front() != 0xa9 ||
      !font.Find(multibyte.front())) {
    lucent::error(
        "gpu-ui",
        "UI SELFTEST FAIL: UTF-8 discriminator decoded {} codepoints; first "
        "is U+{:04X}, glyph present={}; expected 1, U+00A9, true",
        multibyte.size(), multibyte.empty() ? 0 : multibyte.front(),
        !multibyte.empty() && font.Find(multibyte.front()));
    return 1;
  }
  mana::GameUiContent message{.message = "Arena Guard:\nFight!"};
  const auto message_frame = mana::BuildGameUi(font, width, height, message);
  const auto title_text = mana::BuildTextUi(
      font, width, height,
      {{.text = "New Game",
        .x = float(width) * .5f,
        .y = 48.f,
        .scale = 1.f,
        .centered = true,
        .color = {1.f, .8f, .3f, 1.f}}});
  mana::TitleUiState title_state{.screen = mana::TitleUiScreen::kMenu,
                                 .cursor = 0};
  constexpr std::uint32_t title_width = 720;
  constexpr std::uint32_t title_height = 720;
  const auto shipping_title =
      mana::BuildTitleUi(font, strings, title_width, title_height, title_state);
  constexpr std::string_view title_path = "sk1/titlelogo_en_color.png";
  if (!archive.Has(title_path)) {
    lucent::error("gpu-ui",
                  "UI SELFTEST FAIL: scanned archive for {}, matched 0",
                  title_path);
    return 1;
  }
  const auto title_image = mana::DecodeSprite(archive.Read(title_path));
  constexpr std::string_view ja_font_path = "sk1/font_ja.bin";
  constexpr std::string_view ja_strings_path = "sk1/str_ja.bin";
  if (!archive.Has(ja_font_path) || !archive.Has(ja_strings_path)) {
    lucent::error(
        "gpu-ui",
        "UI SELFTEST FAIL: scanned archive for Japanese UI corpus; font={} "
        "strings={}; expected both true",
        archive.Has(ja_font_path), archive.Has(ja_strings_path));
    return 1;
  }
  mcf::Font ja_font;
  mcf::StringTable ja_strings;
  if (!ja_font.LoadFontBin(archive.Read(ja_font_path)) ||
      !ja_strings.Load(archive.Read(ja_strings_path))) {
    lucent::error(
        "gpu-ui",
        "UI SELFTEST FAIL: Japanese font or string table was present but did "
        "not parse");
    return 1;
  }
  const std::string *ja_game_over = ja_strings.Find("SYS_GAMEOVER_MSG");
  const auto ja_frame = ja_game_over
                            ? mana::BuildGameUi(
                                  ja_font, width, height,
                                  mana::GameUiContent{.message = *ja_game_over})
                            : mana::UiFrame{};
  if (hud_frame.solid_quads != 2 || hud_frame.glyph_quads == 0 ||
      hud_frame.missing_glyphs != 0 || level_frame.solid_quads != 3 ||
      level_frame.glyph_quads == 0 || level_frame.missing_glyphs != 0 ||
      message_frame.solid_quads != 2 || message_frame.glyph_quads == 0 ||
      message_frame.missing_glyphs != 0 || !ja_game_over ||
      ja_frame.glyph_quads == 0 || ja_frame.missing_glyphs != 0 ||
      title_text.glyph_quads == 0 || title_text.batches.size() != 1 ||
      title_text.batches.front().color !=
          std::array<float, 4>{1.f, .8f, .3f, 1.f} ||
      shipping_title.glyph_quads == 0 ||
      shipping_title.missing_glyphs != 0) {
    lucent::error(
        "gpu-ui",
        "UI SELFTEST FAIL: layout scanned HUD {}/{}/{}, level {}/{}/{}, "
        "message {}/{}/{} solid/glyph/missing; expected 2/>0/0, 3/>0/0, "
        "2/>0/0; Japanese game-over present={}, glyph/missing={}/{}; first "
        "message miss is U+{:04X}, first Japanese miss U+{:04X}",
        hud_frame.solid_quads, hud_frame.glyph_quads,
        hud_frame.missing_glyphs, level_frame.solid_quads,
        level_frame.glyph_quads, level_frame.missing_glyphs,
        message_frame.solid_quads, message_frame.glyph_quads,
        message_frame.missing_glyphs, bool(ja_game_over),
        ja_frame.glyph_quads, ja_frame.missing_glyphs,
        message_frame.first_missing_codepoint,
        ja_frame.first_missing_codepoint);
    return 1;
  }

  mana::gpu::Device device;
  mana::gpu::UiRenderer renderer(device, font);
  mana::gpu::SpriteRenderer sprite(device, title_image, title_width,
                                   title_height);
  const mana::UiFrame empty;
  const auto clear_pixels = renderer.DrawAndReadback(width, height, empty);
  auto atlas_control = message_frame;
  for (auto &batch : atlas_control.batches)
    batch.textured = false;
  const auto expected_pixels =
      renderer.DrawAndReadback(width, height, message_frame);
  const auto control_pixels =
      renderer.DrawAndReadback(width, height, atlas_control);
  const auto changed_from_clear = Changed(clear_pixels, expected_pixels);
  const auto atlas_differences = Changed(expected_pixels, control_pixels);
  renderer.Prepare(shipping_title);
  const auto title_clear = device.RenderAndReadback(
      title_width, title_height, SDL_FColor{.1f, .11f, .14f, 1.f}, {}, true);
  const auto sprite_only = device.RenderAndReadback(
      title_width, title_height, SDL_FColor{.1f, .11f, .14f, 1.f},
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        sprite.Draw(command, pass);
      },
      true);
  const auto title_pixels = device.RenderAndReadback(
      title_width, title_height, SDL_FColor{.1f, .11f, .14f, 1.f},
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        sprite.Draw(command, pass);
        renderer.Draw(command, pass);
      },
      true);
  const auto title_text_differences = Changed(sprite_only, title_pixels);
  const auto title_sprite_differences = Changed(title_clear, sprite_only);
  if (atlas_negative) {
    lucent::error(
        "gpu-ui",
        "UI SELFTEST FAIL: atlas negative substituted solid glyph quads; {} "
        "of {} pixels differ from textured output",
        atlas_differences, width * height);
    return 1;
  }

  bool invalid_batch_failed = false;
  std::string invalid_batch_message;
  try {
    mana::UiFrame invalid;
    invalid.vertices.resize(3);
    invalid.batches.push_back({.first_vertex = 2, .vertex_count = 3});
    renderer.Prepare(invalid);
  } catch (const std::invalid_argument &error) {
    invalid_batch_message = error.what();
    invalid_batch_failed =
        invalid_batch_message == "UI batch references invalid triangle vertices";
  }
  if (!changed_from_clear || !atlas_differences || !title_text_differences ||
      !title_sprite_differences || !invalid_batch_failed) {
    lucent::error(
        "gpu-ui",
        "UI SELFTEST FAIL: render scanned {} pixels; {} changed from clear, "
        "{} differ from solid-atlas control, title has {} sprite and {} text "
        "pixels, invalid batch rejected={} ({}); expected all "
        "nonzero/true",
        width * height, changed_from_clear, atlas_differences,
        title_sprite_differences, title_text_differences, invalid_batch_failed,
        invalid_batch_message);
    return 1;
  }
  if (capture_path)
    mana::WritePng(capture_path, title_width, title_height, title_pixels);
  lucent::info(
      "gpu-ui",
      "UI SELFTEST: font has {} glyphs; layout scanned HUD {}/{}, level "
      "{}/{}, message {}/{} solid/glyph quads with 0 missing; render scanned "
      "{} pixels, {} changed from clear and {} differ from solid-atlas "
      "control; title has {} sprite and {} text pixels; Japanese game-over "
      "produced {} glyph quads with 0 missing; "
      "invalid batch rejected{}",
      font.glyphs(), hud_frame.solid_quads, hud_frame.glyph_quads,
      level_frame.solid_quads, level_frame.glyph_quads,
      message_frame.solid_quads, message_frame.glyph_quads, width * height,
      changed_from_clear, atlas_differences, title_sprite_differences,
      title_text_differences,
      ja_frame.glyph_quads,
      capture_path ? "; capture written" : "");
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const bool negative =
        argc == 3 && std::string_view(argv[1]) == "--atlas-negative-control";
    const bool capture = argc == 4 && std::string_view(argv[1]) == "--capture";
    const char *archive = negative       ? argv[2]
                          : capture      ? argv[3]
                          : argc == 2    ? argv[1]
                                         : nullptr;
    if (!archive) {
      lucent::error("gpu-ui",
                    "usage: {} [--atlas-negative-control] ARCHIVE | "
                    "--capture PNG ARCHIVE",
                    argv[0]);
      return 2;
    }
    return Run(archive, capture ? argv[2] : nullptr, negative);
  } catch (const std::exception &error) {
    lucent::error("gpu-ui", "UI SELFTEST FATAL: {}", error.what());
    return 2;
  }
}
