#include "host/story_driver.h"

#include <algorithm>

#include <lucent/log.h>

#include "mcf/mcf.h"
#include "engine/world.h"

namespace mana::host {

StoryDriver::StoryDriver(bool enabled, mcf::Inventory& inventory)
    : enabled_(enabled), inventory_(inventory) {}

int StoryDriver::SelectChoice(std::string_view room,
                              const std::vector<std::string>& choices) {
    if (!enabled_ || room != "M0002_06_00") return -1;
    if (key_shop_phase_ == 0 && choices.size() >= 3) {
        key_shop_phase_ = 1;
        return 2;  // Authored shop main menu: Buy.
    }
    if (key_shop_phase_ == 2 && choices.size() == 2) {
        key_shop_phase_ = 3;
        return 1;  // Confirm the Keyring purchase.
    }
    if (key_shop_phase_ == 4 && choices.size() >= 3) {
        key_shop_phase_ = 5;
        return 0;  // Leave after the purchase.
    }
    return -1;
}

int StoryDriver::SelectShopItem(std::string_view room,
                                const std::vector<int>& stock, int mode) {
    if (!enabled_ || room != "M0002_06_00" || mode != 1) return -1;
    if (key_shop_phase_ == 1 &&
        std::find(stock.begin(), stock.end(), 18) != stock.end()) {
        key_shop_phase_ = 2;
        return 18;
    }
    if (key_shop_phase_ == 3 && inventory_.Has(18)) {
        if (!inventory_.Equip(4, 18)) {
            lucent::error("inventory",
                          "headless Keyring equip failed after authored shop purchase");
            failed_ = true;
        } else {
            lucent::info("inventory",
                         "bought and equipped Keyring item 18 from Motie's authored "
                         "shop for {} GP",
                         mcf::ItemBuyPrice(18));
        }
        key_shop_phase_ = 4;
        return 0;
    }
    return -1;
}

bool StoryDriver::EquipAcquiredKey(int item_id) {
    if (!enabled_) return true;
    for (int slot = 4; slot < 8; ++slot) {
        if (inventory_.Equipped(slot) != 0) continue;
        if (!inventory_.Equip(slot, item_id)) {
            lucent::error("inventory", "headless key item {} equip failed", item_id);
            failed_ = true;
            return false;
        }
        lucent::info("inventory", "equipped key item {} in sub-item slot {}",
                     item_id, slot);
        return true;
    }
    lucent::error("inventory", "no free sub-item button for acquired key item {}",
                  item_id);
    failed_ = true;
    return false;
}

std::optional<StoryTarget> StoryDriver::ToppleTarget(
    std::string_view room, const mcf::World& world,
    float room_width, float room_height,
    std::string_view arrival_handler, float body_radius,
    float route_sample) const {
    auto eventTarget = [&](std::string_view name) -> std::optional<StoryTarget> {
        for (const auto& box : world.boxes)
            if (box.enabled && !box.no_touch && box.name == name)
                return StoryTarget{(box.lo[0] + box.hi[0]) * .5f,
                                   (box.lo[2] + box.hi[2]) * .5f,
                                   box.name};
        return std::nullopt;
    };

    if (room == "M0002_00_01") {
        if (!inventory_.Has(18)) return StoryTarget{room_width * .5f, -30.f};
        auto target = eventTarget("out_1");
        if (target && arrival_handler == "out_1")
            for (const auto& box : world.boxes)
                if (box.enabled && !box.no_touch && box.name == "out_1") {
                    target->z = box.lo[2] - body_radius - route_sample;
                    target->event_box.clear();
                    break;
                }
        return target;
    }
    if (room == "M0002_00_00")
        return inventory_.Has(18)
            ? StoryTarget{room_width * .5f, room_height + 30.f}
            : StoryTarget{room_width + 30.f, room_height * .5f};
    if (room == "M0002_01_00")
        return inventory_.Has(18)
            ? std::optional<StoryTarget>(StoryTarget{-30.f, room_height * .5f})
            : eventTarget("in_1");
    if (room == "M0002_06_00") {
        if (inventory_.Has(18)) return eventTarget("out_1");
        for (const auto& actor : world.actors())
            if (actor.alive && actor.handle == "NPC_01")
                return StoryTarget{actor.pos[0], actor.pos[2] + 20.f};
    }
    return std::nullopt;
}

}  // namespace mana::host
