// The application mode machine -- the engine's boot-to-game path.
//
// Reversed from ApplicationMode @ 0x2c1110 and its ProcessMain @ 0x2c133c,
// which is the mode FACTORY: it switches on the pending mode word and news the
// matching class. That switch is where the enum values come from, so they are
// the game's own numbers rather than a port invention:
//
//     2  ModeInit         3  ModeCESA        4  ModeMakerLogo
//     5  ModeTitle        6  ModeGame     (55,304 bytes)
//
// The chain is read from each mode's own Process, by the argument it passes to
// ApplicationMode::SetNextMode @ 0x2c0d04 (which is just `str w1, [x0, #0x5c]`):
//
//     ModeInit      @0x2f6778 -> 3   ModeCESA
//     ModeCESA      @0x2d1ef0 -> 4   ModeMakerLogo
//     ModeMakerLogo @0x2f6a2c -> 5   ModeTitle
//     ModeTitle     @0x3070bc -> 6   ModeGame
//     ModeGame      -> 5 from three places: Process_GameOver @0x2dea58,
//                      Process_SystemMenu @0x2f4514, and Process itself
//
// So game over does not end the program -- it returns to the TITLE. The port
// previously logged that mode 5 was "a mode this port does not have"; it is the
// title screen, and it is now named.
#pragma once

#include <cstdint>
#include <string>

namespace mcf {

// ApplicationMode::EMODE. Values are the engine's, from ProcessMain's switch.
enum class Mode : int32_t {
    kNone = 0,
    kInit = 2,
    kCESA = 3,
    kMakerLogo = 4,
    kTitle = 5,
    kGame = 6,
};

const char* ModeName(Mode m);

// The two splash modes are frame-counter state machines. ModeCESA::Process
// compares a step counter against 0x13, 0x14, 0x1e and 0x28, advancing at step
// 0x28; ModeMakerLogo does the same and additionally requires its frame counter
// at +0x318 to have reached 0xb1 before it moves on. The engine runs at 60fps
// (MainProcess::Initialize calls SiDrawServer::SetFrameParSecond(60)), so 177
// frames is about 2.95 seconds.
//
// NOT REVERSED: what those steps DRAW. The logo artwork has not been located in
// the archive, so the port holds each screen for the engine's own duration and
// draws nothing, rather than inventing a logo or skipping the stage silently.
constexpr int kMakerLogoFrames = 0xb1;   // ModeMakerLogo, +0x318 >= 0xb1
constexpr int kSplashStepEnd = 0x28;     // both, the advancing step

struct ModeMachine {
    Mode current = Mode::kNone;
    Mode next = Mode::kInit;      // MainProcess boots into ModeInit
    int  frames = 0;              // frames spent in `current`

    // True when the mode changed this tick.
    bool Step(float dt_frames);

    // The engine's own transition for the mode we are in, given how long we
    // have been in it. Returns kNone to stay.
    Mode Advance() const;
};

// ---------------------------------------------------------------------------
// The title screen.
//
// ModeTitle keeps two counters: a load phase at +0x318 (values 0..4, which
// drive GameSaveDataHeaderLoad) and the screen state at +0x31c, which the
// Process body compares against 1..13. The selected save slot is at +0x324.
//
// The menu is a table of string ids, memcpy'd 0x180 bytes from 0xbd354 at
// stride 0x80 by ModeTitle::Render @ 0x3087cc -- so it is exactly three
// entries, in this order. SYS_TITLE_MENU_OPTION ("Settings") is in the string
// table but NOT in that table, so it is not offered here.
// ---------------------------------------------------------------------------
struct TitleMenu {
    // The attract screen comes first: the logo plus a prompt, and any button
    // moves on. Then the three-item menu.
    enum class Phase { kAttract, kMenu };

    // ModeTitle::Render's table at 0xbd354, verbatim and in its order.
    static constexpr int kItemCount = 3;
    static const char* const kItemId[kItemCount];

    // Drawn on the attract screen. The engine picks a platform variant of
    // SYS_TITLE_START; _PAD is the keyboard/controller one, which is what this
    // port has. (_TOUCH and _PSVITA are the others.)
    static constexpr const char* kStartId = "SYS_TITLE_START_PAD";
    static constexpr const char* kCopyrightId = "SYS_TITLE_COPYRIGHT_1";

    Phase phase = Phase::kAttract;
    int   cursor = 0;         // 0..kItemCount-1
    bool  chosen = false;     // set when the player confirms
    int   choice = -1;        // the confirmed item, once `chosen`

    void Down();
    void Up();
    // Confirm the cursor. Items whose data is absent are refused rather than
    // silently accepted; `enabled` says which ones can be picked.
    bool Confirm(bool (*enabled)(int));
};

// ---------------------------------------------------------------------------
// The opening crawl -- ApplicationMode sub-mode 10, reached from New Game
// (SetNextSubMode(10) @ 0x307bc8).
//
// New Game's setup @ 0x306df0 loads FORTY lines, `SYS_TITLE_OPENING_%d` for
// i = 0..0x27, into ModeTitle+0x328 (an array of char*). ModeTitle::Render
// @ 0x308e60 draws them and STOPS AT THE FIRST EMPTY STRING (`ldrb`/`cbz` @
// 0x308f58), which is why only 15 show: `_11` is a single space, and `_15`
// onward are empty. So the count is data, not a constant.
//
// Geometry, from Render:
//   line 0 sits at  scroll + 0x240   (0x308e94)
//   each line is    0x28 lower       (`add w22, w22, #0x28` @ 0x308f50)
//   font size       0x20             each line drawn twice, a 0x40 shadow
//                                    then the 0xf0 body
//   alpha fades in over the first 0x50 and out over the last 0x54, and is
//   zero past y > 0x20c
//
// Scroll, from Process @ 0x306ec8:
//   scroll -= dt_ms / 30.0           normally
//   scroll -= dt_ms / 4.0            while skipping (7.5x)
//   done when the last line's y <= -0x15, then SetNextSubMode(11)
// ---------------------------------------------------------------------------
struct OpeningCrawl {
    static constexpr int kIdCount = 0x28;     // ids loaded, 0..39
    static constexpr float kFirstY = 576.f;   // 0x240
    static constexpr float kLineStep = 40.f;  // 0x28
    static constexpr float kSlowDiv = 30.f;
    static constexpr float kFastDiv = 4.f;    // while skipping
    static constexpr float kEndY = -21.f;     // -0x15
    static constexpr float kFadeIn = 80.f;    // 0x50
    static constexpr float kFadeOut = 84.f;   // 0x54
    static constexpr float kHideY = 524.f;    // 0x20c
    static constexpr const char* kSkipId = "SYS_TITLE_BTN_SKIP";

    float scroll = 0.f;
    bool  skipping = false;

    // Advance by a frame. `lines` is how many are actually drawn. Returns true
    // once the crawl is over.
    bool Step(float dt_ms, int lines);
    // Alpha 0..1 for a line at app-space y.
    static float Alpha(float y);
};

// ---------------------------------------------------------------------------
// Name entry. New Game asks for two names, and ModeTitle::Process @ 0x307ec8
// validates each one against a character set the game states outright in
// SYS_NAMEENTRY_USE: it walks the entered name a UTF-8 code point at a time and
// searches that string for a match.
//
// The rules, read from 0x307ec8..0x30800c:
//
//     err = any code point not in SYS_NAMEENTRY_USE            -> 1
//     name equals SYS_COMMON_NULLSPACE (strncmp @ 0x307fd8)    -> 1
//     zero code points                                         -> 3
//     GetLanguage() == 0 (Japanese) and count > 4              -> 2
//     GetLanguage() != 0             and count > 8             -> 2
//
// Japanese additionally permits U+301C (bytes e3 80 9c), tested inline at
// 0x307f60 -- a wave dash, which is not in the shared set.
//
// The message for each error comes from a relative-offset table at 0xbd550:
// `GetStringResource(0xbd554 + (int32)table[err])`, i.e. the entry is a
// displacement from the table's own base, not a pointer.
// ---------------------------------------------------------------------------
struct NameEntry {
    enum Error {
        kOk = 0,
        kProhibited = 1,   // SYS_NAMEENTRY_USAGE_PROHIBITED_STRING
        kTooLong = 2,      // SYS_NAMEENTRY_NUMBER_EXCESS
        kEmpty = 3,        // SYS_NAMEENTRY_NOT_BEEN_ENTER
    };
    // The engine's own message id for an error, or nullptr for kOk.
    static const char* ErrorId(int err);

    static constexpr int kMaxJa = 4;    // GetLanguage() == 0
    static constexpr int kMaxOther = 8;

    // `allowed` is SYS_NAMEENTRY_USE verbatim; `nullspace` is
    // SYS_COMMON_NULLSPACE. Both come from the shipping string table, so this
    // holds no character list of its own.
    static Error Validate(const std::string& name, const std::string& allowed,
                          const std::string& nullspace, bool japanese);

    // How many UTF-8 code points `s` has. UTF8_OctBytes @ 0x3db5f0 is what the
    // engine counts with.
    static int CodePoints(const std::string& s);
};

}  // namespace mcf
