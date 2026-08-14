#pragma once

#include <cstdint>
#include <string>

namespace mcf {

// AddEventBox's shipping volume and edge-trigger state. Map scripts may join
// several physical boxes under one callback name.
struct EventBox {
    static constexpr uint32_t kWallUp = 0x01;
    static constexpr uint32_t kWallDown = 0x02;

    std::string name;
    float lo[3]{};
    float hi[3]{};
    uint32_t flags = 0;
    bool floor_y = false;
    bool enabled = true;
    bool no_touch = false;
    bool inside = false;

    bool IsHit(float x, float y, float z, float room_x, float room_z) const;
    bool CharacterContact(float x, float y, float z, float room_x,
                          float room_z, float radius) const;

    // AddEventBox @ 0x2c84f0 samples a sentinel box at X centre and one-third
    // of the way from its high-Z edge, using a Y=10000 floor query.
    float FloorProbeLocalX() const;
    float FloorProbeLocalZ() const;
    void ResolveFloorY(float floor);
};

}  // namespace mcf
