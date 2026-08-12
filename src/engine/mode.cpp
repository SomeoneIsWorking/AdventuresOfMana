#include "engine/mode.h"

#include <lucent/log.h>

namespace mcf {

const char* ModeName(Mode m) {
    switch (m) {
        case Mode::kInit:      return "ModeInit";
        case Mode::kCESA:      return "ModeCESA";
        case Mode::kMakerLogo: return "ModeMakerLogo";
        case Mode::kTitle:     return "ModeTitle";
        case Mode::kGame:      return "ModeGame";
        default:               return "none";
    }
}

Mode ModeMachine::Advance() const {
    switch (current) {
        // ModeInit::Process @ 0x2f6778 hands straight on. Its own work is
        // resource setup, which the port does at load time instead.
        case Mode::kInit:
            return Mode::kCESA;
        // Both splash screens run a step counter and advance at step 0x28. The
        // port has no step machine to drive them, so it holds each for the
        // maker logo's measured 0xb1 frames -- the one duration the engine
        // states outright -- and says so at the call site.
        case Mode::kCESA:
            return frames >= kMakerLogoFrames ? Mode::kMakerLogo : Mode::kNone;
        case Mode::kMakerLogo:
            return frames >= kMakerLogoFrames ? Mode::kTitle : Mode::kNone;
        // ModeTitle::Process @ 0x3070bc advances on the player's choice, not a
        // timer. The port has no title UI, so the host drives this one.
        case Mode::kTitle:
        case Mode::kGame:
        default:
            return Mode::kNone;
    }
}

// ModeTitle::Render @ 0x3087cc: memcpy(dst, 0xbd354, 0x180) over a 0x80 stride.
const char* const TitleMenu::kItemId[TitleMenu::kItemCount] = {
    "SYS_TITLE_MENU_NEWGAME",
    "SYS_TITLE_MENU_CONTINUE",
    "SYS_TITLE_MENU_LOADGAME",
};

const char* NameEntry::ErrorId(int err) {
    switch (err) {
        // The relative-offset table at 0xbd550, resolved.
        case kProhibited: return "SYS_NAMEENTRY_USAGE_PROHIBITED_STRING";
        case kTooLong:    return "SYS_NAMEENTRY_NUMBER_EXCESS";
        case kEmpty:      return "SYS_NAMEENTRY_NOT_BEEN_ENTER";
        default:          return nullptr;
    }
}

// UTF8_OctBytes @ 0x3db5f0: how many bytes the lead byte claims.
static int OctBytes(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;                       // a stray continuation byte counts as one
}

int NameEntry::CodePoints(const std::string& s) {
    int n = 0;
    for (size_t i = 0; i < s.size(); i += size_t(OctBytes((unsigned char)s[i])))
        ++n;
    return n;
}

NameEntry::Error NameEntry::Validate(const std::string& name,
                                     const std::string& allowed,
                                     const std::string& nullspace,
                                     bool japanese) {
    // The engine tests the character set first, then length, then empty --
    // but the empty test OVERRIDES the set result (csel @ 0x307fec) and the
    // length test overrides both (csel @ 0x307ffc/0x308004), so the effective
    // precedence is length > empty > prohibited. Reproduced in that order.
    int err = kOk;
    for (size_t i = 0; i < name.size();) {
        int n = OctBytes((unsigned char)name[i]);
        std::string cp = name.substr(i, size_t(n));
        bool found = false;
        for (size_t j = 0; j < allowed.size();) {
            int m = OctBytes((unsigned char)allowed[j]);
            if (allowed.compare(j, size_t(m), cp) == 0) { found = true; break; }
            j += size_t(m);
        }
        // Japanese also permits U+301C, tested inline at 0x307f60 as the
        // byte sequence e3 80 9c. Written as escapes rather than as a
        // literal character, so the source file's own encoding cannot
        // silently change what this compares against -- it already did
        // once: a UTF-8 round trip turned the 3 bytes into 6.
        if (!found && japanese && cp == "\xe3\x80\x9c") found = true;
        if (!found) { err = kProhibited; break; }
        i += size_t(n);
    }
    if (!nullspace.empty() && name.compare(0, nullspace.size(), nullspace) == 0)
        err = kProhibited;
    int count = CodePoints(name);
    if (count == 0) err = kEmpty;
    if (count > (japanese ? kMaxJa : kMaxOther)) err = kTooLong;
    return Error(err);
}

void TitleMenu::Down() { cursor = (cursor + 1) % kItemCount; }
void TitleMenu::Up()   { cursor = (cursor + kItemCount - 1) % kItemCount; }

bool TitleMenu::Confirm(bool (*enabled)(int)) {
    if (enabled && !enabled(cursor)) {
        // Refuse rather than accept-and-do-nothing: an item that looks like it
        // was taken but changed nothing is the worse failure.
        lucent::info("title", "{} has no data to act on", kItemId[cursor]);
        return false;
    }
    chosen = true;
    choice = cursor;
    lucent::info("title", "chose {}", kItemId[cursor]);
    return true;
}

bool ModeMachine::Step(float dt_frames) {
    if (next != Mode::kNone && next != current) {
        lucent::info("mode", "{} -> {}", ModeName(current), ModeName(next));
        current = next;
        next = Mode::kNone;
        frames = 0;
        return true;
    }
    frames += int(dt_frames <= 0.f ? 1.f : dt_frames);
    Mode n = Advance();
    if (n != Mode::kNone) next = n;
    return false;
}

}  // namespace mcf
