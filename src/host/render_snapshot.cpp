#include "host/render_snapshot.h"

#include <lucent/log.h>

namespace mana {

void RenderSnapshot::Add(const mcf::RenderAsset &asset,
                         std::array<float, 3> position, float yaw,
                         const mcf::Motion *motion, float motion_time) {
  instances.push_back({.asset = &asset,
                       .motion = motion,
                       .position = position,
                       .yaw = yaw,
                       .motion_time = motion_time});
}

std::size_t RenderSnapshot::skinned_count() const {
  std::size_t count = 0;
  for (const auto &instance : instances)
    count += instance.asset->skinned();
  return count;
}

int RunRenderSnapshotSelfTest() {
  int failures = 0;
  int checked = 0;
  auto check = [&](const char *name, bool pass) {
    ++checked;
    if (pass)
      lucent::info("render", "  ok: {}", name);
    else {
      ++failures;
      lucent::error("render", "SNAPSHOT SELFTEST FAIL: {}", name);
    }
  };
  CameraFrame camera;
  camera.eye = {1.f, 2.f, 3.f};
  RenderSnapshot snapshot(camera);
  mcf::RenderAsset room;
  mcf::RenderAsset actor;
  actor.model.layout.push_back(
      {.usage = mcf::VertexUsage::kWeight,
       .type = mcf::VertexType::kFloat4,
       .offset = 0});
  mcf::Motion motion;
  snapshot.Add(room, {0.f, 0.f, 0.f});
  snapshot.Add(actor, {4.f, 5.f, 6.f}, .75f, &motion, 12.f);
  check("snapshot retains its resolved camera frame",
        snapshot.camera.eye == std::array<float, 3>{1.f, 2.f, 3.f});
  check("snapshot retains static and animated instance state",
        snapshot.instances.size() == 2 &&
            snapshot.instances[1].position ==
                std::array<float, 3>{4.f, 5.f, 6.f} &&
            snapshot.instances[1].yaw == .75f &&
            snapshot.instances[1].motion == &motion &&
            snapshot.instances[1].motion_time == 12.f);
  check("snapshot distinguishes one skinned instance from one static instance",
        snapshot.skinned_count() == 1);
  lucent::info("render", "SNAPSHOT SELFTEST: {} cases, {} failures", checked,
               failures);
  return failures;
}

} // namespace mana
