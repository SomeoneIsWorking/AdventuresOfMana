#pragma once

namespace mcf {
struct Actor;
class World;
}

namespace mana::host {

// ModeGame::PartyTalk searches the player's current 30-unit chip and its four
// cardinal neighbours through CheckAddPos.
bool IsInteractionNeighbour(float player_x, float player_z,
                            float target_x, float target_z);
bool IsNpcInteractionCandidate(float player_x, float player_z,
                               float target_x, float target_z,
                               float collision_circle);
const mcf::Actor* FindNearestNpc(const mcf::World& world,
                                 float player_x, float player_z);
mcf::Actor* FindNearestTreasure(mcf::World& world,
                                float player_x, float player_z);
int RunInteractionSelfTest();

}  // namespace mana::host
