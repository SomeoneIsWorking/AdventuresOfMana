#include "host/story_driver.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

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

bool StoryDriver::EquipAcquiredSubItem(int item_id) {
    if (!enabled_) return true;
    for (int slot = 4; slot < 8; ++slot) {
        if (inventory_.Equipped(slot) != 0) continue;
        if (!inventory_.Equip(slot, item_id)) {
            lucent::error("inventory", "headless sub-item {} equip failed", item_id);
            failed_ = true;
            return false;
        }
        lucent::info("inventory", "equipped sub-item {} in button slot {}",
                     item_id, slot);
        return true;
    }
    lucent::error("inventory", "no free button for acquired sub-item {}",
                  item_id);
    failed_ = true;
    return false;
}

std::optional<StoryTarget> StoryDriver::Target(
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
    if (room == "M0013_09_01") {
        // Both authored pressure switches must be pressed before SetUpGim
        // creates down_1. Each switch disables itself after its pressed edge,
        // making box state the progression state rather than a host counter.
        for (std::string_view name : {"sw_01", "sw_02", "down_1"})
            if (auto target = eventTarget(name)) return target;
    }
    if (room == "M0013_08_04")
        return StoryTarget{room_width + 30.f, room_height * .5f};
    if (room == "M0013_09_04") {
        // EnemyDead authors Fire first and Mirror second. Actor order retains
        // that AddBox order; keep the driver in the boss room until each live
        // reward has gone through the ordinary treasure interaction path.
        for (const auto& actor : world.actors())
            if (actor.alive && actor.treasure_box && !actor.treasure_open)
                return StoryTarget{actor.pos[0], actor.pos[2] + 20.f};
    }
    if (inventory_.Has(31)) {
        if (room == "M0000_14_08")
            return StoryTarget{room_width * .5f, room_height + 30.f};
        if (room == "M0000_14_09" || room == "M0000_13_09" ||
            room == "M0000_12_09" ||
            room == "M0000_11_09")
            return StoryTarget{-30.f, room_height * .5f};
        if (room == "M0000_10_09") return eventTarget("in_01");
        if (room == "M0012_01_01")
            for (const auto& actor : world.actors())
                if (actor.alive && actor.handle == "BULTER")
                    return StoryTarget{actor.pos[0], actor.pos[2] + 20.f};
    }
    if (inventory_.Has(505) && !inventory_.Has(31)) {
        if (room == "M0012_01_01" || room == "M0012_02_01" ||
            room == "M0012_06_00" || room == "M0012_07_00" ||
            room == "M0012_10_02")
            return StoryTarget{room_width + 30.f, room_height * .5f};
        if (room == "M0012_03_01" || room == "M0012_11_02" ||
            room == "M0012_11_01")
            return StoryTarget{room_width * .5f, -30.f};
        if (room == "M0012_03_00" || room == "M0012_08_02")
            return StoryTarget{-30.f, room_height * .5f};
        if (room == "M0012_02_00") return eventTarget("up_1");
        if (room == "M0012_08_00" || room == "M0012_08_01")
            return StoryTarget{room_width * .5f, room_height + 30.f};
        if (room == "M0012_07_02")
            for (std::string_view name : {"sw_1", "down_1"})
                if (auto target = eventTarget(name)) return target;
        if (room == "M0012_11_00")
            for (const auto& actor : world.actors())
                if (actor.alive && actor.treasure_box && !actor.treasure_open)
                    return StoryTarget{actor.pos[0], actor.pos[2] + 20.f};
    }
    return std::nullopt;
}

int RunStoryDriverSelfTest() {
    int bad = 0;
    auto check = [&](std::string_view what, bool condition) {
        if (!condition) {
            ++bad;
            lucent::error("story", "SELFTEST FAIL: {}", what);
        }
    };

    mcf::Inventory inventory;
    inventory.Add(18);
    inventory.Add(30);
    inventory.Equip(4, 18);
    StoryDriver driver(true, inventory);
    check("later key uses a free button without replacing Keyring",
          driver.EquipAcquiredSubItem(30) && inventory.Equipped(4) == 18 &&
              inventory.Equipped(5) == 30);

    mcf::World world;
    for (const auto& [name, z] :
         std::initializer_list<std::pair<const char*, float>>{
             {"sw_01", 180.f}, {"sw_02", 30.f}}) {
        mcf::EventBox box;
        box.name = name;
        box.lo[0] = 150.f;
        box.hi[0] = 180.f;
        box.lo[2] = z;
        box.hi[2] = z + 30.f;
        world.boxes.push_back(std::move(box));
    }
    auto target = driver.Target("M0013_09_01", world, 330.f, 270.f, "",
                                30.f, 7.5f);
    check("Hydra hidden stair targets the first live authored switch",
          target && target->event_box == "sw_01");
    world.boxes[0].enabled = false;
    target = driver.Target("M0013_09_01", world, 330.f, 270.f, "", 30.f,
                           7.5f);
    check("Hydra hidden stair advances to the remaining authored switch",
          target && target->event_box == "sw_02");
    target = driver.Target("M0013_08_04", world, 330.f, 270.f, "", 30.f,
                           7.5f);
    check("Hydra stair landing continues through its open east edge",
          target && target->x == 360.f && target->z == 135.f &&
              target->event_box.empty());

    auto& fire = world.Spawn("_BOX", 32, 165.f, 0.f, 75.f);
    fire.treasure_box = true;
    fire.treasure_item = 505;
    auto& mirror = world.Spawn("_BOX_1", 32, 195.f, 0.f, 75.f);
    mirror.treasure_box = true;
    mirror.treasure_item = 31;
    target = driver.Target("M0013_09_04", world, 300.f, 240.f, "", 30.f,
                           7.5f);
    check("Hydra rewards preserve authored Fire-first box order",
          target && target->x == 165.f && target->z == 95.f);
    world.Find("_BOX")->treasure_open = true;
    target = driver.Target("M0013_09_04", world, 300.f, 240.f, "", 30.f,
                           7.5f);
    check("Hydra reward driver advances to Mirror after Fire opens",
          target && target->x == 195.f && target->z == 95.f);
    inventory.Add(31);
    check("Mirror equips in the next free button without replacing keys",
          driver.EquipAcquiredSubItem(31) && inventory.Equipped(4) == 18 &&
              inventory.Equipped(5) == 30 && inventory.Equipped(6) == 31);
    target = driver.Target("M0000_14_08", world, 300.f, 240.f, "", 30.f,
                           7.5f);
    check("post-Hydra route takes the reachable south exit toward Kett",
          target && target->x == 150.f && target->z == 270.f);
    inventory.Del(31);
    inventory.Add(505);
    target = driver.Target("M0012_01_01", world, 330.f, 270.f, "", 30.f,
                           7.5f);
    check("post-Steward route starts through the authored east door",
          target && target->x == 360.f && target->z == 135.f);
    mcf::World chain_world;
    auto& chain = chain_world.Spawn("_BOX", 32, 150.f, 0.f, 105.f);
    chain.treasure_box = true;
    chain.treasure_item = 104;
    target = driver.Target("M0012_11_00", chain_world, 330.f, 270.f, "", 30.f,
                           7.5f);
    check("Kett trap room targets its live Chain Flail chest",
          target && target->x == 150.f && target->z == 125.f);

    lucent::info("story", "SELFTEST: 10 cases, {} failures", bad);
    return bad;
}

}  // namespace mana::host
