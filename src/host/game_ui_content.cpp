#include "host/game_ui_content.h"

#include <format>
#include <stdexcept>

#include "mcf/mcf.h"

namespace mana {

GameUiContent BuildGameUiContent(const mcf::StringTable &strings,
                                 const mcf::PlayerStats &player,
                                 bool show_hud, bool level_up_open,
                                 int level_up_choice,
                                 const std::string &message) {
  if (level_up_choice < 0 || level_up_choice >= 4)
    throw std::invalid_argument("level-up UI choice must be in 0..3");
  const auto text = [&](const char *id, const char *fallback) {
    const std::string *value = strings.Find(id);
    return value ? *value : std::string(fallback);
  };
  GameUiContent content;
  content.show_hud = show_hud;
  content.low_hp = player.max_hp() > 0 && player.hp * 4 <= player.max_hp();
  content.level_up_open = level_up_open;
  content.message = message;
  if (show_hud) {
    content.hud_rows = {
        std::format("{} {:>3}/{:<3}  {} {:>2}/{:<2}",
                    text("SYS_COMMON_STATUS_LABEL_4", "HP"), player.hp,
                    player.max_hp(), text("SYS_COMMON_STATUS_LABEL_5", "MP"),
                    player.mp, player.max_mp()),
        std::format("{} {:<5}  {} {}/{}",
                    text("SYS_COMMON_STATUS_LABEL_6", "GP"), player.money,
                    text("SYS_COMMON_STATUS_LABEL_7", "EXP"), player.exp,
                    player.next_exp()),
        std::format(
            "{} {:<3} {} {:<3} {} {}",
            text("SYS_COMMON_STATUS_LABEL_8", "ATK"), player.attack(),
            text("SYS_COMMON_STATUS_LABEL_9", "DEF"), player.defence(),
            text("SYS_COMMON_STATUS_LABEL_1", "Lv"),
            player.level_up_due()
                ? std::format("{} {}", player.level,
                              text("SYS_COMMON_BUTTON_LEVELUP", "Lv Up!"))
                : std::format("{}", player.level)),
    };
  }
  if (level_up_open) {
    constexpr const char *type_ids[]{"SYS_LEVELUP_TYPE_1",
                                     "SYS_LEVELUP_TYPE_2",
                                     "SYS_LEVELUP_TYPE_3",
                                     "SYS_LEVELUP_TYPE_4"};
    constexpr const char *help_ids[]{"SYS_HELP_LEVELUP_FIGHTER",
                                     "SYS_HELP_LEVELUP_MONK",
                                     "SYS_HELP_LEVELUP_WIZARD",
                                     "SYS_HELP_LEVELUP_WISEMAN"};
    constexpr const char *fallback[]{"Warrior", "Monk", "Mage", "Sage"};
    content.level_up_rows = {
        text("SYS_HELP_LEVELUP_START_TOUCH", "Select a training regimen."),
        ""};
    for (int index = 0; index < 4; ++index)
      content.level_up_rows.push_back(std::format(
          "{} {}", index == level_up_choice ? ">" : " ",
          text(type_ids[index], fallback[index])));
    content.level_up_rows.emplace_back();
    const std::string help = text(help_ids[level_up_choice], "");
    for (std::size_t begin = 0, end; begin <= help.size(); begin = end + 1) {
      end = help.find('\n', begin);
      if (end == std::string::npos)
        end = help.size();
      content.level_up_rows.push_back(help.substr(begin, end - begin));
      if (end == help.size())
        break;
    }
  }
  return content;
}

} // namespace mana
