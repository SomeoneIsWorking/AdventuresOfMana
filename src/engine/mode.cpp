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
