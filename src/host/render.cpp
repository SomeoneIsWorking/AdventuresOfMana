#include "host/render.h"

#include <format>
#include <string_view>

#include <lucent/log.h>

namespace mcf {

std::string ActorModelName(char kind, int type_id) {
    // ModeGame::AddEnemy @ 0x2dda74 normally formats sk1/E%04d_00, but its
    // shipping id-123 branch replaces that result with the literal
    // sk1/B0023_00 before CharacterSetFileName. The Butler transformation is
    // authored as AddEnemy(123), so retain enemy ownership while using the
    // Steward Wolf boss model selected by the original engine.
    if (kind == 'E' && type_id == 123) return "B0023_00";

    // NPCs are offset by 10: eNPC id 10 is N0000, 11 is N0001, and so on. This
    // is not inferred -- the original developers annotated the enum in sk1.lua
    // with the model for each id ("MAN = 13, -- 13 N0003 00 villager(man)"),
    // and all 25 annotated entries say id-10, none say id.
    //
    // The old id==model rule was worse than a missing model: for ids where an
    // N<id>_00 happened to exist it silently drew the WRONG character.
    //
    // Ids 0..9 are the named party members and use the CHARACTER prefix
    // id-for-id: 0 is the hero (the enum literally reads "NONE = 0, -- none
    // (hero)"), and C0001..C0009 all exist. Confirmed by rendering the two most
    // distinctive: eNPC 5 is CHOCOBO and C0005_00 is a chocobo, eNPC 6 is
    // CHOCOBOT and C0006_00 is the same bird in armour. That is a semantic
    // check, not just "a file with that name exists".
    if (kind == 'N') {
        // eNPC.TRANS = -1, annotated 透明 ("transparent") in the enum: a
        // deliberately invisible NPC used as a pure trigger/anchor. It has no
        // model by design, so returning a name at all would make the room
        // census report a missing asset that does not exist.
        if (type_id < 0) return {};
        // `npc()` is also the authored cutscene-character constructor. The
        // Lua enum deliberately folds enemy and boss model namespaces into
        // eNPC with ENEMY=100 and BOSS=1000, then adds the corresponding enum
        // id (for example Julius is 1000+20). Decode those tagged ranges
        // before applying the ordinary-NPC offset.
        if (type_id >= 1000) return std::format("B{:04d}_00", type_id - 1000);
        if (type_id >= 100) return std::format("E{:04d}_00", type_id - 100);
        if (type_id < 10) return std::format("C{:04d}_00", type_id);
        return std::format("N{:04d}_00", type_id - 10);
    }
    return std::format("{}{:04d}_00", kind, type_id);
}

int RunActorModelSelfTest() {
    int bad = 0;
    auto check = [&](std::string_view what, bool pass) {
        if (!pass) { ++bad; lucent::error("render", "SELFTEST FAIL: {}", what); }
        else lucent::info("render", "  ok: {}", what);
    };
    check("NPC-tagged enemies use the enemy namespace",
          ActorModelName('N', 100) == "E0000_00" &&
              ActorModelName('N', 173) == "E0073_00");
    check("NPC-tagged bosses use the boss namespace",
          ActorModelName('N', 1010) == "B0010_00" &&
              ActorModelName('N', 1020) == "B0020_00");
    check("ordinary and party NPC mappings remain distinct",
          ActorModelName('N', 10) == "N0000_00" &&
              ActorModelName('N', 5) == "C0005_00");
    check("AddEnemy 123 alone selects the Steward Wolf boss model",
          ActorModelName('E', 123) == "B0023_00" &&
              ActorModelName('E', 23) == "E0023_00");
    lucent::info("render", "SELFTEST: 4 cases, {} failures", bad);
    return bad;
}

}  // namespace mcf
