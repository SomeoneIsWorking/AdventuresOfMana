#pragma once

#include <string>

#include "host/render_ui.h"

namespace mcf {
struct PlayerStats;
class StringTable;
}

namespace mana {

GameUiContent BuildGameUiContent(const mcf::StringTable &strings,
                                 const mcf::PlayerStats &player,
                                 bool show_hud, bool level_up_open,
                                 int level_up_choice,
                                 const std::string &message);

} // namespace mana
