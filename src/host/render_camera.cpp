#include "host/render_camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <lucent/log.h>

namespace mana {

CameraFrame CameraTracker::Update(
    const mcf::World &world, const std::array<float, 3> &player_world,
    const std::array<float, 3> &room_origin, float delta_seconds) {
  const auto &camera = world.camera;
  CameraFrame frame;
  frame.target = {player_world[0], player_world[1] + 20.f, player_world[2]};
  if (!camera.target_chr.empty()) {
    if (const auto *actor = world.Find(camera.target_chr)) {
      frame.target = {actor->pos[0] + room_origin[0],
                      actor->pos[1] + room_origin[1] + 20.f,
                      actor->pos[2] + room_origin[2]};
    }
  }
  if (camera.has_target_pos) {
    for (int axis = 0; axis < 3; ++axis)
      frame.target[axis] = camera.target_pos[axis] + room_origin[axis];
  }
  for (int axis = 0; axis < 3; ++axis)
    frame.target[axis] += camera.target_sub[axis];

  constexpr float degrees = std::numbers::pi_v<float> / 180.f;
  const float distance = camera.Get(mcf::cam_data::kDistance, 450.f);
  const float yaw = camera.Get(mcf::cam_data::kRotateY, 0.f) * degrees;
  const float pitch =
      camera.Get(mcf::cam_data::kRotateX, camera.pitch_default) * degrees;
  std::array<float, 3> desired{
      frame.target[0] + std::sin(yaw) * std::cos(pitch) * distance,
      frame.target[1] + std::sin(pitch) * distance,
      frame.target[2] + std::cos(yaw) * std::cos(pitch) * distance};
  if (camera.has_eye_pos) {
    for (int axis = 0; axis < 3; ++axis)
      desired[axis] = camera.eye_pos[axis] + room_origin[axis];
  }
  if (!initialized_) {
    eye_ = desired;
    initialized_ = true;
  }
  const float speed = camera.Get(mcf::cam_data::kSpeed, .3f);
  const float step =
      1.f - std::pow(1.f - std::min(speed, .99f), delta_seconds * 30.f);
  for (int axis = 0; axis < 3; ++axis)
    eye_[axis] += (desired[axis] - eye_[axis]) * step;

  frame.eye = eye_;
  frame.vertical_fov_radians =
      camera.Get(mcf::cam_data::kAngle, 20.f) * 2.f * degrees;
  frame.near_plane = camera.Get(mcf::cam_data::kNear, 40.f);
  frame.far_plane = camera.Get(mcf::cam_data::kFar, 5000.f);
  return frame;
}

int RunCameraTrackerSelfTest() {
  int failures = 0;
  int checked = 0;
  auto check = [&](const char *name, bool pass) {
    ++checked;
    if (pass)
      lucent::info("camera", "  ok: {}", name);
    else {
      ++failures;
      lucent::error("camera", "TRACKER SELFTEST FAIL: {}", name);
    }
  };
  const auto close = [](float left, float right) {
    return std::fabs(left - right) < 1e-3f;
  };

  mcf::World world;
  const std::array<float, 3> player{10.f, 5.f, 20.f};
  const std::array<float, 3> origin{300.f, 0.f, 240.f};
  CameraTracker tracker;
  auto frame = tracker.Update(world, player, origin, 1.f / 30.f);
  check("default target follows the world-space player",
        frame.target == std::array<float, 3>{10.f, 25.f, 20.f});
  check("default first eye snaps to the spherical camera",
        close(frame.eye[0], 10.f) && frame.eye[1] > frame.target[1] &&
            frame.eye[2] > frame.target[2]);

  world.camera.has_target_pos = true;
  world.camera.target_pos[0] = 1.f;
  world.camera.target_pos[1] = 2.f;
  world.camera.target_pos[2] = 3.f;
  world.camera.target_sub[0] = 4.f;
  world.camera.target_sub[1] = 5.f;
  world.camera.target_sub[2] = 6.f;
  world.camera.has_eye_pos = true;
  world.camera.eye_pos[0] = 7.f;
  world.camera.eye_pos[1] = 8.f;
  world.camera.eye_pos[2] = 9.f;
  tracker.Reset();
  frame = tracker.Update(world, player, origin, 1.f / 30.f);
  check("fixed target applies room origin then target offset",
        frame.target == std::array<float, 3>{305.f, 7.f, 249.f});
  check("explicit eye applies room origin exactly",
        frame.eye == std::array<float, 3>{307.f, 8.f, 249.f});

  world.camera.Reset();
  auto &actor = world.Spawn("focus", 0, 11.f, 12.f, 13.f);
  actor.kind = 'C';
  world.camera.target_chr = "focus";
  tracker.Reset();
  frame = tracker.Update(world, player, origin, 1.f / 30.f);
  check("character target resolves room-local actor coordinates",
        frame.target == std::array<float, 3>{311.f, 32.f, 253.f});
  world.camera.target_chr = "missing";
  tracker.Reset();
  frame = tracker.Update(world, player, origin, 1.f / 30.f);
  check("missing character target visibly falls back to the player",
        frame.target == std::array<float, 3>{10.f, 25.f, 20.f});

  world.camera.Reset();
  world.camera.has_eye_pos = true;
  world.camera.eye_pos[0] = world.camera.eye_pos[1] =
      world.camera.eye_pos[2] = 0.f;
  tracker.Reset();
  (void)tracker.Update(world, player, {0.f, 0.f, 0.f}, 1.f / 30.f);
  world.camera.eye_pos[0] = 100.f;
  frame = tracker.Update(world, player, {0.f, 0.f, 0.f}, 1.f / 30.f);
  check("shipping speed 0.3 advances one 30 Hz step by 30 percent",
        close(frame.eye[0], 30.f));

  lucent::info("camera", "TRACKER SELFTEST: {} cases, {} failures", checked,
               failures);
  return failures;
}

} // namespace mana
