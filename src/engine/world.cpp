#include "engine/world.h"

#include <cmath>
#include <format>

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
