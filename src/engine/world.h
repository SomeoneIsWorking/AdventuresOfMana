// The actor system: live characters and weapons addressed by the string handles
// the Lua scripts pass around ("player", NPC names, spawned enemy handles).
//
// Scope is set by measurement, not guesswork: docs/lua-census.md shows the
// character/weapon accessors are ~60% of everything the shipping scripts do.
//
// The data-slot enums are the game's OWN, lifted from sk1.lua where the original
// developers documented every field in Japanese. Slot numbers are theirs.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
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

struct Actor {
    std::string handle;          // the name scripts address it by
    std::string model;           // e.g. "B0000_00"
    int type_id = -1;            // NPC / enemy id from the spawning call
    // Which spawner made this, because that selects the model prefix:
    // 'N' AddNPC, 'E' AddEnemy, 'B' AddBoss, 'C' AddParty.
    char kind = 'N';
    float pos[3]{};
    float rot_y = 0;
    int motion = 0;              // eMOTION index; also the .smot numeric prefix
    bool alive = true;
    bool is_weapon = false;
    std::map<int, float> data;   // sparse: slot -> value

    float Get(int slot, float dflt = 0) const {
        auto it = data.find(slot);
        return it == data.end() ? dflt : it->second;
    }
};

class World {
public:
    Actor& Spawn(const std::string& handle, int type_id, float x, float y, float z);
    Actor* Find(const std::string& handle);
    bool Remove(const std::string& handle);

    const std::vector<Actor>& actors() const { return actors_; }
    Camera camera;

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
