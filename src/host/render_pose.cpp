#include "host/render_pose.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace mcf {
namespace {

void MatMul4(const float *a, const float *b, float *out) {
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      float sum = 0;
      for (int k = 0; k < 4; ++k)
        sum += a[k * 4 + row] * b[column * 4 + k];
      out[column * 4 + row] = sum;
    }
  }
}

void QuatTrans(const float quaternion[4], const float translation[3],
               float *out) {
  const float x = quaternion[0];
  const float y = quaternion[1];
  const float z = quaternion[2];
  const float w = quaternion[3];
  out[0] = 1 - 2 * (y * y + z * z);
  out[1] = 2 * (x * y + z * w);
  out[2] = 2 * (x * z - y * w);
  out[3] = 0;
  out[4] = 2 * (x * y - z * w);
  out[5] = 1 - 2 * (x * x + z * z);
  out[6] = 2 * (y * z + x * w);
  out[7] = 0;
  out[8] = 2 * (x * z + y * w);
  out[9] = 2 * (y * z - x * w);
  out[10] = 1 - 2 * (x * x + y * y);
  out[11] = 0;
  out[12] = translation[0];
  out[13] = translation[1];
  out[14] = translation[2];
  out[15] = 1;
}

std::array<float, 16> AnimatedLocal(const Bone &bone, const Motion *motion,
                                    float time) {
  std::array<float, 16> local{};
  std::memcpy(local.data(), bone.local, sizeof(local));
  if (!motion)
    return local;
  for (const auto &track : motion->tracks) {
    if (track.name != bone.name || track.times.empty())
      continue;
    std::size_t key = 0;
    while (key + 1 < track.times.size() && track.times[key + 1] <= time)
      ++key;
    float translation[3]{bone.local[12], bone.local[13], bone.local[14]};
    if (!track.trans.empty()) {
      for (int axis = 0; axis < 3; ++axis)
        translation[axis] = track.trans[key][axis];
    }
    if (!track.rot.empty())
      QuatTrans(track.rot[key].data(), translation, local.data());
    else
      std::copy(translation, translation + 3, local.begin() + 12);
    break;
  }
  return local;
}

std::vector<std::array<float, 16>> BuildWorldPose(const Model &model,
                                                  const Motion *motion,
                                                  float time) {
  std::vector<std::array<float, 16>> world(model.bones.size());
  for (std::size_t i = 0; i < model.bones.size(); ++i) {
    const auto &bone = model.bones[i];
    const auto local = AnimatedLocal(bone, motion, time);
    if (bone.parent < 0)
      world[i] = local;
    else
      MatMul4(world[std::size_t(bone.parent)].data(), local.data(),
              world[i].data());
  }
  return world;
}

} // namespace

void BuildJointPalette(const Model &model, const Motion *motion, float time,
                       std::vector<float> *out) {
  out->assign(80 * 3 * 4, 0.f);
  const auto world = BuildWorldPose(model, motion, time);
  for (std::size_t i = 0; i < model.bones.size() && i < 80; ++i) {
    const auto &bone = model.bones[i];
    float skin[16];
    if (bone.degenerate) {
      std::memset(skin, 0, sizeof(skin));
      skin[0] = skin[5] = skin[10] = skin[15] = 1.f;
    } else {
      MatMul4(world[i].data(), bone.inv_world, skin);
    }
    for (int row = 0; row < 3; ++row) {
      for (int column = 0; column < 4; ++column) {
        (*out)[(i * 3 + std::size_t(row)) * 4 + std::size_t(column)] =
            skin[column * 4 + row];
      }
    }
  }
}

bool BoneLocalPos(const Model &model, const Motion *motion, float time,
                  const std::string &bone_name, float out[3]) {
  const auto world = BuildWorldPose(model, motion, time);
  for (std::size_t i = 0; i < model.bones.size(); ++i) {
    if (model.bones[i].name != bone_name)
      continue;
    for (int axis = 0; axis < 3; ++axis)
      out[axis] = world[i][12 + axis];
    return true;
  }
  return false;
}

} // namespace mcf
