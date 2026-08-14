#include "host/render_normals.h"

#include <cmath>
#include <cstring>
#include <format>

namespace mcf {
namespace {

std::uint32_t ReadIndex(const Model &model, std::uint32_t element) {
  std::uint32_t value = 0;
  const auto indices = model.indices();
  std::memcpy(&value, indices.data() + std::size_t(element) * model.index_size,
              model.index_size);
  return value;
}

void ReadPosition(const Model &model, const VertexAttribute &position,
                  std::uint32_t vertex, float *out) {
  const auto vertices = model.vertices();
  std::memcpy(out,
              vertices.data() + std::size_t(vertex) * model.vertex_stride +
                  position.offset,
              sizeof(float) * 3);
}

} // namespace

NormalGeneration GenerateNormals(const Model &model) {
  if (model.index_size != 2 && model.index_size != 4)
    throw Error(std::format(
        "normal generation requires 16- or 32-bit indices, received {} bits",
        model.index_size * 8));
  if (model.index_count % 3 != 0)
    throw Error(std::format(
        "normal generation requires triangles, received {} trailing indices",
        model.index_count % 3));
  const auto *position = model.Find(VertexUsage::kPosition);
  if (!position || position->type != VertexType::kFloat3)
    throw Error("normal generation requires a float3 position attribute");

  NormalGeneration result;
  result.values.resize(std::size_t(model.vertex_count) * 3);
  result.triangles = model.index_count / 3;
  for (std::uint32_t triangle = 0; triangle < result.triangles; ++triangle) {
    std::uint32_t index[3];
    for (int corner = 0; corner < 3; ++corner) {
      index[corner] = ReadIndex(model, triangle * 3 + corner);
      if (index[corner] >= model.vertex_count)
        throw Error(std::format(
            "normal generation triangle {} index {} is outside {} vertices",
            triangle, index[corner], model.vertex_count));
    }
    float point[3][3];
    for (int corner = 0; corner < 3; ++corner)
      ReadPosition(model, *position, index[corner], point[corner]);
    const float first[3]{point[1][0] - point[0][0], point[1][1] - point[0][1],
                         point[1][2] - point[0][2]};
    const float second[3]{point[2][0] - point[0][0], point[2][1] - point[0][1],
                          point[2][2] - point[0][2]};
    const float normal[3]{first[1] * second[2] - first[2] * second[1],
                          first[2] * second[0] - first[0] * second[2],
                          first[0] * second[1] - first[1] * second[0]};
    const float length_squared =
        normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
    if (!(length_squared > 0.f) || !std::isfinite(length_squared)) {
      ++result.degenerate_triangles;
      continue;
    }
    for (const auto vertex : index)
      for (int axis = 0; axis < 3; ++axis)
        result.values[std::size_t(vertex) * 3 + axis] += normal[axis];
  }

  for (std::uint32_t vertex = 0; vertex < model.vertex_count; ++vertex) {
    float *normal = result.values.data() + std::size_t(vertex) * 3;
    const float length_squared =
        normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
    if (!(length_squared > 0.f) || !std::isfinite(length_squared)) {
      normal[0] = normal[1] = normal[2] = 0.f;
      ++result.vertices_without_normal;
      continue;
    }
    const float inverse_length = 1.f / std::sqrt(length_squared);
    for (int axis = 0; axis < 3; ++axis)
      normal[axis] *= inverse_length;
  }
  return result;
}

} // namespace mcf
