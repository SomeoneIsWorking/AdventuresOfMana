#include "engine/world.h"

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
