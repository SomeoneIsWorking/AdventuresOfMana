#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mcf/mcf.h"   // FormatParams, held by value below

struct lua_State;


namespace mcf {

class World;
class Audio;
class StringTable;

// A Lua 5.3 state with the game's own 200-function `cmd` API bound into globals.
// See src/engine/script.cpp for why the bindings currently record rather than act.
class Script {
public:
    struct CallRecord { uint64_t count = 0; };

    Script();
    ~Script();
    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;

    bool Run(std::string_view name, std::span<const uint8_t> source);
    bool CallFunction(std::string_view fn);
    bool HasFunction(std::string_view fn) const;

    // A map script shares scenario globals (sccnt, scflagNN) with later maps,
    // but its callbacks and active coroutines belong to that map only. Drop
    // those before evaluating the next room without resetting the Lua state.
    void ClearRoomScript();
    void RememberRoomFunctions(const std::vector<std::string>& before);

    // Event handlers YIELD (fadeout/fadein call coroutine.yield), so the engine
    // runs them as coroutines -- hence NewCoroutine in the cmd API and
    // GameScript::NewCoroutine/Update in the binary. A plain pcall cannot yield
    // and dies with "attempt to yield from outside a coroutine".
    bool StartCoroutine(std::string_view fn);
    void ResumeCoroutines();
    size_t live_coroutines() const { return co_.size(); }
    std::vector<std::string> Globals() const;

    const std::string& last_error() const { return last_error_; }
    static size_t api_size();
    static std::string ShiftJisToUtf8(std::span<const uint8_t> in);

    World* world = nullptr;   // optional; when set, hot cmd calls act
    // The game's string table. When set, GetIDString returns real text instead
    // of echoing the id back.
    const StringTable* strings = nullptr;
    // Sets `strings` and seeds the names @H and @G expand to, which is what
    // ModeInit::Process does at 0x2f65e4 before any save is loaded.
    void SetStrings(const StringTable* t);
    // The values the text control codes expand to. See mcf::CnvFormatString.
    FormatParams fmt;
    // Runs `t` through CnvFormatString with this script's parameters -- the
    // conversion every text window does before it draws.
    std::string FormatText(const std::string& t) const;
    // The last line SetMessageWnd was handed, so dialogue is observable before
    // there is any UI to draw it in.
    std::string last_message;
    // True while a line is on screen. The host clears it when the player
    // dismisses the message, which is what lets the script's coroutine resume.
    bool message_pending = false;
    long messages_shown = 0, message_ids_missing = 0;
    Audio* audio = nullptr;   // optional; when set, BgmPlay/SePlay sound
    // Audio the scripts asked for; the host services these because it owns the
    // archive. -1 means "no request".
    struct SeReq { int id; bool loop; };
    // MapJump(mapid, mapx, mapy, plx, ply, plz, arrow). The host services it,
    // because loading a room means touching the archive and the GL state.
    struct JumpReq { int map, gx, gy, arrow; float x, y, z; };
    bool has_jump = false;
    JumpReq jump{};
    int pending_bgm = -1;
    int current_bgm = 0;
    std::vector<SeReq> pending_se;
    std::vector<int> pending_se_stop;
    bool stop_all_se = false;
    // GetGameTimeMs is the game clock the prelude's wait(n) uses. The host
    // advances it once per gameplay frame; it is deliberately not wall time.
    int game_time_ms = 0;

    std::map<std::string, CallRecord> calls;
    bool trace_first = false;

private:
    std::vector<int> co_;      // registry refs to live threads
    std::vector<std::string> room_functions_;
    lua_State* L_ = nullptr;
    std::string last_error_;
};

}  // namespace mcf
