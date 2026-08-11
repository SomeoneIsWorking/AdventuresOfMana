#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;

namespace mcf {

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
    std::vector<std::string> Globals() const;

    const std::string& last_error() const { return last_error_; }
    static size_t api_size();
    static std::string ShiftJisToUtf8(std::span<const uint8_t> in);

    std::map<std::string, CallRecord> calls;
    bool trace_first = false;

private:
    lua_State* L_ = nullptr;
    std::string last_error_;
};

}  // namespace mcf
