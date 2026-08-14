#pragma once

#include <string>
#include <vector>

#include "mcf/mcf.h"

namespace mcf {

// Builds the shipping 80-bone, three-row skinning palette. With no motion the
// bind pose collapses to identity for every non-degenerate bone.
void BuildJointPalette(const Model &model, const Motion *motion, float time,
                       std::vector<float> *out);

// World-space position of a named bone in the animated model-local pose.
bool BoneLocalPos(const Model &model, const Motion *motion, float time,
                  const std::string &bone, float out[3]);

} // namespace mcf
