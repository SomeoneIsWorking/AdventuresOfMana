#include "engine/world.h"

#include <cmath>
#include <format>
#include <numbers>

namespace mcf {

Actor& World::Spawn(const std::string& handle, int type_id, float x, float y, float z) {
    if (auto* a = Find(handle)) {   // scripts reuse handles across rooms
        a->type_id = type_id;
        a->pos[0] = x; a->pos[1] = y; a->pos[2] = z;
        a->alive = true;
        return *a;
    }
    Actor a;
    a.handle = handle;
    a.type_id = type_id;
    a.pos[0] = x; a.pos[1] = y; a.pos[2] = z;
    index_[handle] = actors_.size();
    actors_.push_back(std::move(a));
    return actors_.back();
}

Actor* World::Find(const std::string& handle) {
    auto it = index_.find(handle);
    return it == index_.end() ? nullptr : &actors_[it->second];
}

bool World::Remove(const std::string& handle) {
    auto it = index_.find(handle);
    if (it == index_.end()) return false;
    actors_[it->second].alive = false;
    return true;
}

void World::TickScriptMoves(float dt, const ScriptMoveBlocked& blocked) {
    for (auto& a : actors_) {
        if (!a.alive || !a.script_auto_move) continue;
        a.data[chr_data::kIsHitMap] = 0.f;
        float dx = a.script_move_target[0] - a.pos[0];
        float dy = a.script_move_target[1] - a.pos[1];
        float dz = a.script_move_target[2] - a.pos[2];
        float left = std::sqrt(dx * dx + dy * dy + dz * dz);
        float step = a.script_move_speed * dt;
        float nx = a.script_move_target[0];
        float nz = a.script_move_target[2];
        if (left > step && left > 0.0001f) {
            nx = a.pos[0] + dx / left * step;
            nz = a.pos[2] + dz / left * step;
        }
        if (blocked && a.Get(chr_data::kMapCollision) != 0.f && blocked(a, nx, nz)) {
            a.data[chr_data::kIsHitMap] = 1.f;
            ++a.script_map_hits;
            a.script_auto_move = false;
            continue;
        }
        if (left <= step || left <= 0.0001f) {
            a.script_distance_moved += left;
            for (int k = 0; k < 3; ++k) a.pos[k] = a.script_move_target[k];
            a.script_auto_move = false;
            continue;
        }
        a.pos[0] += dx / left * step;
        a.pos[1] += dy / left * step;
        a.pos[2] += dz / left * step;
        a.script_distance_moved += step;
    }
}

void World::TickMotions(float frames) {
    for (auto& a : actors_) {
        if (!a.alive || a.motion_duration <= 0.f) continue;
        a.motion_frame = std::min(a.motion_frame + frames, a.motion_duration);
    }
}

void World::TickLookTargets() {
    for (auto& a : actors_) {
        if (!a.alive || a.look_target.empty()) continue;
        const Actor* target = Find(a.look_target);
        if (!target || !target->alive) continue;
        float dx = target->pos[0] - a.pos[0];
        float dz = target->pos[2] - a.pos[2];
        if (dx == 0.f && dz == 0.f) continue;
        float deg = std::atan2(dz, dx) * (180.f / float(std::numbers::pi));
        a.data[chr_data::kLookAtDeg] = deg;
        a.rot_y = std::atan2(dx, dz);
    }
}

bool HitArcSphere(const float a[3], float ar, float arc_deg, float yaw,
                  const float d[3], float dr) {
    float dx = d[0] - a[0], dy = d[1] - a[1], dz = d[2] - a[2];
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist > ar + dr) return false;
    if (arc_deg >= 359.f) return true;          // full circle: no angular limit
    float ang = std::atan2(dx, dz) - yaw;
    while (ang > 3.14159265f) ang -= 6.2831853f;
    while (ang < -3.14159265f) ang += 6.2831853f;
    return std::fabs(ang) <= arc_deg * 0.5f * (3.14159265f / 180.f);
}

EventBox* World::FindBox(const std::string& name) {
    for (auto& b : boxes)
        if (b.name == name) return &b;
    return nullptr;
}

void World::Reset() {
    actors_.clear();
    index_.clear();
    spawn_serial_ = 0;
    boxes.clear();
}

std::string World::MotionPrefix(const std::string& model, int motion_id) {
    return std::format("sk1/{}_{:03d}_", model, motion_id);
}

}  // namespace mcf
