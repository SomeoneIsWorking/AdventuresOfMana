#pragma once

#include <cstdint>
#include <vector>

#include "mcf/mcf.h"

namespace mcf {

struct NormalGeneration {
  std::vector<float> values;
  std::uint32_t triangles = 0;
  std::uint32_t degenerate_triangles = 0;
  std::uint32_t vertices_without_normal = 0;
};

// Derives area-weighted smooth normals from the shipping indexed triangle
// list. Vertices unsupported by a non-degenerate triangle remain exactly zero;
// callers can then apply ambient light without inventing surface orientation.
NormalGeneration GenerateNormals(const Model &model);

} // namespace mcf
