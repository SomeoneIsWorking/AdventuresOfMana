#pragma once

namespace mana::host {

// Character movement may fall to any lower floor when an authored objective
// is below. Upward movement remains bounded by the shipping 30-unit step.
bool IsRouteHeightTransition(float from, float to, bool descending_goal);
bool ShouldRebuildForFloorMismatch(float planned_y, float live_y,
                                   bool player_on_wall,
                                   bool has_mapjump_floor_owner);
int RunNavigationSelfTest();

}  // namespace mana::host
