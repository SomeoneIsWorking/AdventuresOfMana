#include "host/interaction.h"

#include <cstdlib>
#include <limits>

#include <lucent/log.h>

#include "engine/world.h"

namespace mana::host {

bool IsInteractionNeighbour(float player_x, float player_z,
                            float target_x, float target_z) {
    constexpr float kChipSize = 30.f;
    // AArch64 fcvtzs truncates toward zero, as does this conversion.
    const int player_col = int(player_x / kChipSize);
    const int player_row = int(player_z / kChipSize);
    const int target_col = int(target_x / kChipSize);
    const int target_row = int(target_z / kChipSize);
    return std::abs(target_col - player_col) +
               std::abs(target_row - player_row) <= 1;
}

bool IsNpcInteractionCandidate(float player_x, float player_z,
                               float target_x, float target_z,
                               float collision_circle) {
    const float dx = target_x - player_x;
    const float dz = target_z - player_z;
    return IsInteractionNeighbour(player_x, player_z, target_x, target_z) ||
           dx * dx + dz * dz <= collision_circle * collision_circle;
}

const mcf::Actor* FindNearestNpc(const mcf::World& world,
                                 float player_x, float player_z) {
    const mcf::Actor* nearest = nullptr;
    float nearest_d2 = std::numeric_limits<float>::infinity();
    for (const auto& actor : world.actors()) {
        if (!actor.alive || actor.kind != 'N' || actor.handle == "MainPlayer")
            continue;
        const float dx = actor.pos[0] - player_x;
        const float dz = actor.pos[2] - player_z;
        const float d2 = dx * dx + dz * dz;
        if (IsNpcInteractionCandidate(player_x, player_z,
                                      actor.pos[0], actor.pos[2],
                                      actor.collision_circle) &&
            d2 < nearest_d2) {
            nearest = &actor;
            nearest_d2 = d2;
        }
    }
    return nearest;
}

mcf::Actor* FindNearestTreasure(mcf::World& world,
                                float player_x, float player_z) {
    mcf::Actor* nearest = nullptr;
    float nearest_d2 = std::numeric_limits<float>::infinity();
    for (auto& actor : world.actors_mutable()) {
        if (!actor.alive || !actor.treasure_box || actor.treasure_open) continue;
        const float dx = actor.pos[0] - player_x;
        const float dz = actor.pos[2] - player_z;
        const float d2 = dx * dx + dz * dz;
        if (IsInteractionNeighbour(player_x, player_z,
                                   actor.pos[0], actor.pos[2]) &&
            d2 < nearest_d2) {
            nearest = &actor;
            nearest_d2 = d2;
        }
    }
    return nearest;
}

int RunInteractionSelfTest() {
    int bad = 0;
    auto check = [&](const char* name, bool pass) {
        if (!pass) { ++bad; lucent::error("interaction", "SELFTEST FAIL: {}", name); }
        else lucent::info("interaction", "  ok: {}", name);
    };
    check("cardinal chip", IsInteractionNeighbour(195, 115, 196, 85));
    check("NPC collision circle", IsNpcInteractionCandidate(195, 120, 196, 85, 42));
    check("diagonal rejected", !IsInteractionNeighbour(95, 95, 125, 125));
    check("two chips rejected", !IsInteractionNeighbour(95, 95, 155, 95));
    return bad;
}

}  // namespace mana::host
