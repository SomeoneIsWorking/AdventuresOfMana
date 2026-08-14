#include "engine/event_box.h"

#include <algorithm>

namespace mcf {

bool EventBox::IsHit(float x, float y, float z,
                     float room_x, float room_z) const {
    return enabled && !no_touch &&
           x > lo[0] + room_x && x < hi[0] + room_x &&
           y > lo[1] && y < hi[1] &&
           z > lo[2] + room_z && z < hi[2] + room_z;
}

bool EventBox::CharacterContact(float x, float y, float z,
                                float room_x, float room_z,
                                float radius) const {
    if (!enabled || no_touch || y <= lo[1] || y >= hi[1]) return false;
    const float lx = x - room_x;
    const float lz = z - room_z;
    const float dx = std::max({lo[0] - lx, 0.f, lx - hi[0]});
    const float dz = std::max({lo[2] - lz, 0.f, lz - hi[2]});
    return dx * dx + dz * dz < radius * radius;
}

float EventBox::FloorProbeLocalX() const {
    return (lo[0] + hi[0]) * .5f;
}

float EventBox::FloorProbeLocalZ() const {
    return (lo[2] + 2.f * hi[2]) / 3.f;
}

void EventBox::ResolveFloorY(float floor) {
    if (!floor_y) return;
    // AddEventBox passes floor + resolved_y0 - 1 to AppEventBoxBase::Set.
    // Sentinel y0 becomes zero after a successful floor query.
    lo[1] = floor - 1.f;
    hi[1] += floor;
    floor_y = false;
}

}  // namespace mcf
