// The Lua bridge: a real Lua state with the game's own 200-function `cmd`
// module bound to it, so the shipping scripts run unmodified.
//
// Every binding is currently a STUB that records the call. That is deliberate
// and is the point of this stage: running the real scripts against a recording
// harness measures which of the 200 functions the game actually exercises, and
// how often, so engine work is ordered by evidence instead of guesswork.
//
// The binding table is GENERATED from the shipping binary (cmd_api.inc, via
// tools/gen_cmd_api.py), so it cannot drift from the real engine's Lua surface.
#include "engine/script.h"
#include "engine/world.h"

#include <algorithm>
#include <cstring>
#include <format>

#include <lucent/log.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace mcf {
namespace {

struct CmdDef {
    const char* name;
    const char* args;
    char ret;
};

constexpr CmdDef kCmdApi[] = {
#define CMD(n, a, r) {n, a, r},
#include "engine/cmd_api.inc"
#undef CMD
};
constexpr size_t kCmdCount = sizeof(kCmdApi) / sizeof(kCmdApi[0]);

Script* FromState(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "mcf_script");
    auto* s = static_cast<Script*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return s;
}

// Returns true if handled, having pushed its own results.
bool Dispatch(lua_State* L, const CmdDef* def, World& w);

// One generic trampoline for all 200 bindings; the definition arrives as an
// upvalue so there is no generated code per function to keep in sync.
int CmdStub(lua_State* L) {
    const auto* def = static_cast<const CmdDef*>(lua_touserdata(L, lua_upvalueindex(1)));
    Script* self = FromState(L);
    if (self) {
        auto& rec = self->calls[def->name];
        if (rec.count == 0 && self->trace_first) {
            std::string a;
            int n = lua_gettop(L);
            for (int i = 1; i <= n; ++i) {
                if (i > 1) a += ", ";
                if (lua_type(L, i) == LUA_TSTRING) a += std::format("\"{}\"", lua_tostring(L, i));
                else if (lua_isnumber(L, i)) a += std::format("{:g}", lua_tonumber(L, i));
                else a += luaL_typename(L, i);
            }
            lucent::debug("lua", "{}({})", def->name, a);
        }
        ++rec.count;
    }
    // Dispatch the measured-hot character and weapon calls to the actor system.
    // Everything else still records only; see docs/lua-census.md for the order.
    if (self && self->world) {
        int before = lua_gettop(L);
        if (Dispatch(L, def, *self->world)) return lua_gettop(L) - before;
    }

    // Return a value of the type the real wrapper pushes, so scripts that
    // branch on a result keep running rather than erroring on nil.
    switch (def->ret) {
        case 'n': lua_pushnumber(L, 0); return 1;
        case 'b': lua_pushboolean(L, 0); return 1;
        case 's': lua_pushstring(L, ""); return 1;
        default: return 0;
    }
}

// The character/weapon accessors, ordered by the census. Handles are strings;
// AddNPC/AddEnemy create them, everything else addresses them by name.
bool Dispatch(lua_State* L, const CmdDef* def, World& w) {
    std::string_view n = def->name;
    auto S = [&](int i) { return lua_isstring(L, i) ? lua_tostring(L, i) : ""; };
    auto N = [&](int i) { return float(luaL_optnumber(L, i, 0)); };

    if (n == "AddNPC") {                    // (name, id, x, y, z, deg)
        auto& a = w.Spawn(S(1), int(N(2)), N(3), N(4), N(5));
        a.rot_y = N(6); a.kind = 'N';
        return true;
    }
    if (n == "AddNPCSubType") {             // (name, id, sub, x, y, z, deg)
        auto& a = w.Spawn(S(1), int(N(2)), N(4), N(5), N(6));
        a.rot_y = N(7); a.kind = 'N';
        return true;
    }
    if (n == "AddEnemy" || n == "AddBoss" || n == "AddParty") {
        // (id, x, y, z). Unlike AddNPC these carry no handle -- the engine
        // assigns one. The handle MUST be unique per spawn, not per type: a
        // room placing three of the same enemy is three actors, and keying on
        // the type id silently collapsed them into one.
        int id = int(N(1));
        const char* kind = n == "AddBoss" ? "boss" : (n == "AddParty" ? "party" : "enemy");
        auto& a = w.Spawn(std::format("{}{}_{}", kind, id, w.NextSpawnSerial()), id,
                          N(2), N(3), N(4));
        a.kind = n == "AddBoss" ? 'B' : (n == "AddParty" ? 'C' : 'E');
        return true;
    }
    if (n == "DelNPC" || n == "DeadEnemy") { w.Remove(S(1)); return true; }

    if (n == "ChrSetData") {                // (name, slot, value)
        if (auto* a = w.Find(S(1))) a->data[int(N(2))] = N(3);
        return true;
    }
    if (n == "ChrGetData") {
        auto* a = w.Find(S(1));
        lua_pushnumber(L, a ? a->Get(int(N(2))) : 0);
        return true;
    }
    if (n == "ChrSetPos") {
        if (auto* a = w.Find(S(1))) { a->pos[0] = N(2); a->pos[1] = N(3); a->pos[2] = N(4); }
        return true;
    }
    if (n == "ChrGetLocalPosX" || n == "ChrGetLocalPosY" || n == "ChrGetLocalPosZ") {
        auto* a = w.Find(S(1));
        int k = n == "ChrGetLocalPosX" ? 0 : (n == "ChrGetLocalPosY" ? 1 : 2);
        lua_pushnumber(L, a ? a->pos[k] : 0);
        return true;
    }
    if (n == "ChrMoveTo") {                 // (name, x, y, z) -- instant for now
        if (auto* a = w.Find(S(1))) { a->pos[0] = N(2); a->pos[1] = N(3); a->pos[2] = N(4); }
        return true;
    }
    if (n == "ChrMotion" || n == "ChrMotionForce") {
        if (auto* a = w.Find(S(1))) a->motion = int(N(2));
        return true;
    }
    if (n == "ChrMotionGetID") {
        auto* a = w.Find(S(1));
        lua_pushnumber(L, a ? a->motion : 0);
        return true;
    }
    if (n == "ChrIsAlive") {
        auto* a = w.Find(S(1));
        lua_pushboolean(L, a && a->alive);
        return true;
    }
    if (n == "WepSetData") {
        if (auto* a = w.Find(S(1))) a->data[int(N(2))] = N(3);
        return true;
    }
    if (n == "WepGetData") {
        auto* a = w.Find(S(1));
        lua_pushnumber(L, a ? a->Get(int(N(2))) : 0);
        return true;
    }
    if (n == "WepIsAlive") {
        auto* a = w.Find(S(1));
        lua_pushboolean(L, a && a->alive);
        return true;
    }
    if (n == "WepDel") { w.Remove(S(1)); return true; }
    return false;
}

}  // namespace

Script::Script() {
    L_ = luaL_newstate();
    luaL_openlibs(L_);
    lua_pushlightuserdata(L_, this);
    lua_setfield(L_, LUA_REGISTRYINDEX, "mcf_script");

    // The engine registers these as a flat module: tolua_module(L, NULL, 0)
    // followed by tolua_beginmodule(L, NULL), i.e. straight into globals.
    for (size_t i = 0; i < kCmdCount; ++i) {
        lua_pushlightuserdata(L_, const_cast<CmdDef*>(&kCmdApi[i]));
        lua_pushcclosure(L_, CmdStub, 1);
        lua_setglobal(L_, kCmdApi[i].name);
    }
}

Script::~Script() {
    if (L_) lua_close(L_);
}

size_t Script::api_size() { return kCmdCount; }

// sk1.lua is Shift-JIS while every map script is UTF-8 (see docs/assets.md).
// Shift-JIS trail bytes can be 0x5C, which Lua would read as a backslash escape
// inside a string literal, so the bytes are transcoded rather than fed raw.
std::string Script::ShiftJisToUtf8(std::span<const uint8_t> in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        uint8_t c = in[i];
        if (c < 0x80) { out.push_back(char(c)); continue; }
        // Non-ASCII: emit U+FFFD per source character. The scripts' Japanese
        // text is comments and display strings; nothing here compares it, and a
        // wrong-but-valid transcode would be worse than an explicit placeholder.
        bool lead = (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
        if (lead && i + 1 < in.size()) ++i;
        out += "\xEF\xBF\xBD";
    }
    return out;
}

bool Script::Run(std::string_view name, std::span<const uint8_t> source) {
    bool sjis = false;
    for (size_t i = 0; i + 1 < source.size(); ++i) {
        uint8_t c = source[i];
        if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) {
            uint8_t n = source[i + 1];
            if (n == 0x5C || (n >= 0x40 && n <= 0x7E)) { sjis = true; break; }
        }
        if (c >= 0xC0) break;  // looks like UTF-8 lead
    }
    std::string text;
    if (sjis) {
        text = ShiftJisToUtf8(source);
        lucent::debug("lua", "{}: transcoded from Shift-JIS", name);
    } else {
        text.assign(reinterpret_cast<const char*>(source.data()), source.size());
    }

    if (luaL_loadbuffer(L_, text.data(), text.size(), std::string(name).c_str()) != LUA_OK ||
        lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        last_error_ = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "unknown error";
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

bool Script::CallFunction(std::string_view fn) {
    lua_getglobal(L_, std::string(fn).c_str());
    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 1);
        last_error_ = std::format("'{}' is not a global function", fn);
        return false;
    }
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        last_error_ = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "unknown error";
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

std::vector<std::string> Script::Globals() const {
    std::vector<std::string> out;
    lua_pushglobaltable(L_);
    lua_pushnil(L_);
    while (lua_next(L_, -2)) {
        if (lua_type(L_, -2) == LUA_TSTRING && lua_isfunction(L_, -1))
            out.push_back(lua_tostring(L_, -2));
        lua_pop(L_, 1);
    }
    lua_pop(L_, 1);
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace mcf
