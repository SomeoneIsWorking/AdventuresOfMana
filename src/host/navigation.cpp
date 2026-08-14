#include "host/navigation.h"

#include <lucent/log.h>

namespace mana::host {

bool IsRouteHeightTransition(float from, float to, bool descending_goal) {
    constexpr float kStepHeight = 30.f;
    constexpr float kEpsilon = .01f;
    if (to <= from + kEpsilon)
        return descending_goal || from - to <= kStepHeight;
    return to - from <= kStepHeight;
}

int RunNavigationSelfTest() {
    int bad = 0;
    auto check = [&](const char* name, bool pass) {
        if (!pass) { ++bad; lucent::error("navigation", "SELFTEST FAIL: {}", name); }
        else lucent::info("navigation", "  ok: {}", name);
    };
    check("30-unit climb", IsRouteHeightTransition(0, 30, false));
    check("oversized climb rejected", !IsRouteHeightTransition(0, 30.25f, true));
    check("ordinary oversized drop rejected", !IsRouteHeightTransition(60, 0, false));
    check("authored descent accepts fall", IsRouteHeightTransition(60, 0, true));
    return bad;
}

}  // namespace mana::host
