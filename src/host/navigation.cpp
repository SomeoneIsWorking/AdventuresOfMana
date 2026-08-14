#include "host/navigation.h"

#include <cmath>

#include <lucent/log.h>

namespace mana::host {

bool IsRouteHeightTransition(float from, float to, bool descending_goal) {
    constexpr float kStepHeight = 30.f;
    constexpr float kEpsilon = .01f;
    if (to <= from + kEpsilon)
        return descending_goal || from - to <= kStepHeight;
    return to - from <= kStepHeight;
}

bool ShouldRebuildForFloorMismatch(float planned_y, float live_y,
                                   bool player_on_wall,
                                   bool has_mapjump_floor_owner) {
    // An arrival event volume can temporarily own an elevated ledge while the
    // route's point samples already see the lower continuation. Rebuilding the
    // same point-ground route cannot change that expected body/point mismatch.
    return !player_on_wall && !has_mapjump_floor_owner &&
           std::fabs(planned_y - live_y) >= 30.f;
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
    check("arrival-floor ownership keeps the existing point route",
          !ShouldRebuildForFloorMismatch(0, 30, false, true));
    check("unowned 30-unit mismatch rebuilds from live state",
          ShouldRebuildForFloorMismatch(0, 30, false, false));
    return bad;
}

}  // namespace mana::host
