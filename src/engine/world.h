// The actor system: live characters and weapons addressed by the string handles
// the Lua scripts pass around ("player", NPC names, spawned enemy handles).
//
// Scope is set by measurement, not guesswork: docs/lua-census.md shows the
// character/weapon accessors are ~60% of everything the shipping scripts do.
//
// The data-slot enums are the game's OWN, lifted from sk1.lua where the original
// developers documented every field in Japanese. Slot numbers are theirs.
#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

#include "mcf/mcf.h"   // EnemyStats::AiMachine, held by value below
#include <unordered_map>
#include <utility>
#include <vector>

namespace mcf {

// eChrGetData, from sk1.lua. Only the slots the scripts actually touch are named
// here; the rest are carried generically so nothing is silently dropped.
namespace chr_data {
enum : int {
    kHP = 0, kMaxHP = 1, kMP = 2, kMaxMP = 3,
    kScale = 100, kFlight = 101, kMapCollision = 102, kObjCollision = 103,
    kThWepCollision = 104, kChrCollision = 105, kFloorType = 106,
    kRotateSmooth = 107, kTalkTarget = 108, kInsideRoom = 109,
};
}

// eWepGetData, from sk1.lua.
namespace wep_data {
enum : int {
    kVecX = 0, kVecY = 1, kVecZ = 2,
    kAccX = 3, kAccY = 4, kAccZ = 5,
    kLifetime = 6, kMapCollision = 7, kHitDead = 8,
    kLookAt = 9, kLookAtDeg = 10, kInsideRoom = 11, kObjCollision = 12,
};
}

// eCamGetData, from sk1.lua. Scripts drive these directly.
namespace cam_data {
enum : int {
    kRotateX = 0, kRotateY = 1, kRotateZ = 2,
    kAngle = 3,       // field of view, degrees; scripts use 15 and 20
    kDistance = 4,    // scripts use 300, 380, 450
    kTargetSubX = 5, kTargetSubY = 6, kTargetSubZ = 7,
    kSpeed = 8,       // lerp rate; sk1.lua's comment gives the default as 0.3
    kRoomBoxLf = 9, kRoomBoxRi = 10, kRoomBoxUp = 11, kRoomBoxDn = 12,
    kNear = 13,       // sk1.lua: def 40.0
    kFar = 14,        // sk1.lua: def 5000.0
    kControlType = 15,
};
}

// Spherical follow camera. Slot values and their documented defaults are the
// game's; the default PITCH is a port choice -- sk1.lua does not state one and
// scripts only ever set ROTATE_X to 0, so the engine's own default must come
// from AppCameraGame, which is not reversed yet.
struct Camera {
    std::map<int, float> data;
    std::string target_chr;          // CamSetTargetChr
    float target_pos[3]{};
    bool has_target_pos = false;
    float pitch_default = 38.f;      // PORT CHOICE, not a reversed value

    float Get(int slot, float dflt) const {
        auto it = data.find(slot);
        return it == data.end() ? dflt : it->second;
    }
    void Reset() {
        data.clear();
        target_chr.clear();
        has_target_pos = false;
    }
};

// Combat volumes, both attached to a named bone.
//   ChrDamageBoneSet(chr, i, bone); ChrDamageBoneSize(chr, i, radius)
//     -> a SPHERE (scripts use radius 15 on r_hand / l_hand)
//   ChrAttackBoneSet(chr, i, bone); ChrAttackBoneSize(chr, i, radius, degrees, ...)
//     -> an ARC/fan (scripts use 35 at 180 degrees, 50 at 360)
struct HitVolume {
    std::string bone;
    float radius = 0;
    float arc_deg = 360;      // attack only; damage volumes are spheres
    float offset[3]{};
    bool valid = false;
    float rate = 1.f;         // ChrAttackBoneAttackRate
};

struct Actor {
    std::string handle;          // the name scripts address it by
    std::string model;           // e.g. "B0000_00"
    int type_id = -1;            // NPC / enemy id from the spawning call
    // Which spawner made this, because that selects the model prefix:
    // 'N' AddNPC, 'E' AddEnemy, 'B' AddBoss, 'C' AddParty.
    char kind = 'N';
    // AppCharacterBase::GetType(), the engine's own faction tag: 1 Player,
    // 2 Party, 3 NPC, 4 Enemy. Damage is filtered on it -- see CharType() and
    // CanDamage() below.
    enum Type { kPlayer = 1, kParty = 2, kNpc = 3, kEnemy = 4 };
    float pos[3]{};
    float rot_y = 0;
    int motion = 0;              // eMOTION index; also the .smot numeric prefix
    bool alive = true;
    // Combat status. For 'E'/'B' actors these come from sk1/enemydat.bin: the
    // engine's GetStatusMaxHp reads record +0x04 and Damage subtracts record
    // +0x0C, so these are the game's own numbers, not port inventions.
    int hp = 0, max_hp = 0, defence = 0, attack_power = 0;
    int exp = 0, money = 0;
    // enemydat +0x68 and +0x64. The speed is used; the AI type is recorded
    // only -- AppCharacterBase::UpdateAI switches 27 ways on it and none of the
    // 27 behaviours is reversed.
    float move_speed = 0.f;
    int ai_type = 0;
    // The AI state machine, RE-VERIFIED -- see mcf::NextAiState. `ai_record`
    // is the toggle at actor +0x3894, `ai_state` the word at +0x38e8 and
    // `ai_timer` the countdown at +0x38ec, in frames. The engine resets the
    // pair to {0, -1}, so a fresh actor rolls its first duration immediately.
    int ai_record = 0;
    int ai_state = 0;
    float ai_timer = -1.f;
    mcf::EnemyStats::AiMachine ai[2];
    bool has_ai = false;
    // Attack volumes hit every frame they overlap. The engine applies a hit
    // once per swing, so each swing gets an id and a target is only damaged
    // once per id. Without this a 24-frame swing dealt damage 24 times.
    uint32_t swing_id = 0;
    std::vector<std::pair<uint32_t, std::string>> hit_this_swing;
    // Set when a script spawned this NPC at exactly (0,0) -- the engine's
    // signal to choose the position itself (see Dispatch's AddNPC).
    bool random_place = false;
    float place_extent = 0.f;
    bool is_weapon = false;
    std::map<int, float> data;   // sparse: slot -> value
    std::map<int, HitVolume> attack;   // ChrAttackBone*
    std::map<int, HitVolume> damage;   // ChrDamageBone*

    float Get(int slot, float dflt = 0) const {
        auto it = data.find(slot);
        return it == data.end() ? dflt : it->second;
    }
};

// AddEventBox(name, x0,y0,z0, x1,y1,z1, flag) -- the most-called cmd function
// (552 calls across the shipping scripts). When the player enters the volume the
// engine calls the global Lua function of the same name, which is how map
// transitions, cutscenes and shops all start.
struct EventBox {
    std::string name;
    float lo[3]{}, hi[3]{};
    bool enabled = true;
    bool no_touch = false;   // SetEventBoxNoTouchEvent
    bool inside = false;     // edge-triggered: fire on entry, not every frame
};

// The engine's faction tag for a spawned actor. The player is the one actor the
// scripts address as "MainPlayer"; everything else follows its spawner.
inline int CharType(const Actor& a) {
    if (a.handle == "MainPlayer") return Actor::kPlayer;
    switch (a.kind) {
        case 'E': case 'B': return Actor::kEnemy;
        case 'C': return Actor::kParty;
        default: return Actor::kNpc;
    }
}

// Whether `attacker` may damage `defender`, straight out of the two Damage
// overrides. AppCharacterEnemy::Damage @ 0x2b2b00 reads the attacker's GetType
// and returns unless it is 1 or 2; AppCharacterPlayer::Damage @ 0x2b5b7c
// returns unless it is 4. So enemies cannot hurt each other and the player
// cannot hurt a party member -- a filter, not a house rule.
inline bool CanDamage(const Actor& attacker, const Actor& defender) {
    int at = CharType(attacker), dt = CharType(defender);
    if (dt == Actor::kEnemy) return at == Actor::kPlayer || at == Actor::kParty;
    if (dt == Actor::kPlayer) return at == Actor::kEnemy;
    return false;   // NPCs and party members: no Damage override reached here
}

// SetFade(kind, milliseconds) starts a fade; scripts then block in a coroutine
// on IsFadeFinish(). Stubbing IsFadeFinish to true would 'work' but would also
// remove the wait that map transitions are built around, so the timer is real.
struct Fade {
    float remaining_ms = 0;
    float duration_ms = 0;
    int kind = 0;                 // 1 = out, 0 = in (as scripts use it)
    uint8_t colour[3]{0, 0, 0};
    bool Finished() const { return remaining_ms <= 0.f; }
    void Tick(float dt_ms) { remaining_ms = std::max(0.f, remaining_ms - dt_ms); }
    // 0 at the start of a fade-out, 1 when fully covered.
    float Coverage() const {
        if (duration_ms <= 0) return kind ? 1.f : 0.f;
        float p = 1.f - remaining_ms / duration_ms;
        return kind ? p : 1.f - p;
    }
};

// Attack-arc vs damage-sphere overlap. `yaw` is the attacker's facing.
// Pure geometry, so it can be tested against both a hit and a miss without a
// running game -- see --combat-selftest.
bool HitArcSphere(const float atk_pos[3], float atk_radius, float arc_deg, float yaw,
                  const float dmg_pos[3], float dmg_radius);

class World {
public:
    Actor& Spawn(const std::string& handle, int type_id, float x, float y, float z);
    Actor* Find(const std::string& handle);
    bool Remove(const std::string& handle);

    const std::vector<Actor>& actors() const { return actors_; }
    std::vector<Actor>& actors_mutable() { return actors_; }
    Camera camera;
    Fade fade;
    std::vector<EventBox> boxes;

    EventBox* FindBox(const std::string& name);

    void Reset();

    // Monotonic per-World counter for engine-assigned actor handles.
    uint32_t NextSpawnSerial() { return ++spawn_serial_; }

    // Motion files are `<model>_<NNN>_<LABEL>.smot` where NNN is the eMOTION
    // index. Verified on 1584 of 1592 parseable files; the 8 disagreements and
    // 129 non-canonical labels are all in the LABEL, never the number, so
    // lookup goes by number and ignores the label entirely.
    static std::string MotionPrefix(const std::string& model, int motion_id);

private:
    std::vector<Actor> actors_;
    std::unordered_map<std::string, size_t> index_;
    uint32_t spawn_serial_ = 0;
};

}  // namespace mcf
