#pragma once

#include <string_view>
#include <optional>
#include <string>
#include <vector>

namespace mcf {
struct Inventory;
class World;
}

namespace mana::host {

struct StoryTarget {
    float x = 0.f;
    float z = 0.f;
    std::string event_box;
};

// Drives authored story interactions during deterministic, windowless runs.
// It owns interaction state; navigation and simulation remain host services.
class StoryDriver {
public:
    StoryDriver(bool enabled, mcf::Inventory& inventory);

    int SelectChoice(std::string_view room, const std::vector<std::string>& choices);
    int SelectShopItem(std::string_view room, const std::vector<int>& stock, int mode);
    bool EquipAcquiredKey(int item_id);
    std::optional<StoryTarget> ToppleTarget(
        std::string_view room, const mcf::World& world,
        float room_width, float room_height,
        std::string_view arrival_handler, float body_radius,
        float route_sample) const;

    bool failed() const { return failed_; }

private:
    bool enabled_ = false;
    mcf::Inventory& inventory_;
    int key_shop_phase_ = 0;
    bool failed_ = false;
};

}  // namespace mana::host
