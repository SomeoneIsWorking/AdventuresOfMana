#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <lucent/log.h>

#include "host/render_normals.h"

namespace {

mcf::Model TriangleModel(std::uint32_t index_size, bool degenerate) {
  mcf::Model model;
  model.vertex_count = 3;
  model.vertex_stride = 12;
  model.vertex_offset = 0;
  model.index_count = 3;
  model.index_size = index_size;
  model.index_offset = model.vertex_count * model.vertex_stride;
  model.layout.push_back(
      {mcf::VertexUsage::kPosition, mcf::VertexType::kFloat3, 0});
  const float vertices[] = {0.f,
                            0.f,
                            0.f,
                            1.f,
                            0.f,
                            0.f,
                            degenerate ? 1.f : 0.f,
                            degenerate ? 0.f : 1.f,
                            0.f};
  model.data.resize(model.index_offset + model.index_count * index_size);
  std::memcpy(model.data.data(), vertices, sizeof(vertices));
  for (std::uint32_t index = 0; index < 3; ++index)
    std::memcpy(model.data.data() + model.index_offset + index * index_size,
                &index, index_size);
  return model;
}

bool IsNormal(const std::vector<float> &values, float z) {
  for (std::size_t vertex = 0; vertex < 3; ++vertex) {
    const float *normal = values.data() + vertex * 3;
    if (std::abs(normal[0]) > 1e-6f || std::abs(normal[1]) > 1e-6f ||
        std::abs(normal[2] - z) > 1e-6f)
      return false;
  }
  return true;
}

} // namespace

int main() {
  for (const std::uint32_t index_size : {2u, 4u}) {
    auto model = TriangleModel(index_size, false);
    const auto positive = mcf::GenerateNormals(model);
    if (positive.triangles != 1 || positive.degenerate_triangles != 0 ||
        positive.vertices_without_normal != 0 ||
        !IsNormal(positive.values, 1.f)) {
      lucent::error(
          "normals",
          "NORMAL SELFTEST FAIL: {}-bit positive scanned {} triangles, {} "
          "degenerate, {} vertices without normals; expected 1, 0, 0 and +Z",
          index_size * 8, positive.triangles, positive.degenerate_triangles,
          positive.vertices_without_normal);
      return 1;
    }
    const auto negative = mcf::GenerateNormals(TriangleModel(index_size, true));
    if (negative.triangles != 1 || negative.degenerate_triangles != 1 ||
        negative.vertices_without_normal != 3 ||
        !IsNormal(negative.values, 0.f)) {
      lucent::error("normals",
                    "NORMAL SELFTEST FAIL: {}-bit negative scanned {} "
                    "triangles, {} degenerate, {} vertices without normals; "
                    "expected 1, 1, 3 and exact zero vectors",
                    index_size * 8, negative.triangles,
                    negative.degenerate_triangles,
                    negative.vertices_without_normal);
      return 1;
    }
  }
  lucent::info("normals",
               "NORMAL SELFTEST: 16- and 32-bit positives produced +Z for all "
               "3 vertices; both degenerate negatives scanned 1 triangle and "
               "reported 1 degenerate plus 3 exact-zero normals");
  return 0;
}
