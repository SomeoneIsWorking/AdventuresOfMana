#include "host/render_asset.h"

#include <algorithm>
#include <cstring>
#include <format>

#include <lucent/log.h>

namespace mcf {

const Texture *RenderAsset::TextureAt(std::uint32_t index) const {
  if (index >= textures.size() || !textures[index])
    return nullptr;
  const TextureRef ref = *textures[index];
  if (ref.set >= texture_sets.size() ||
      ref.texture >= texture_sets[ref.set].textures.size())
    return nullptr;
  return &texture_sets[ref.set].textures[ref.texture];
}

bool LoadRenderAsset(const Archive &archive, const std::string &name,
                     RenderAsset *out) {
  const std::string model_path = std::format("sk1/{}.smdl", name);
  if (!archive.Has(model_path))
    return false;

  RenderAsset asset;
  asset.name = name;
  asset.model = ParseSmdl(archive.Read(model_path));
  asset.normals = GenerateNormals(asset.model);

  const std::string stex_path = std::format("sk1/{}.stex", name);
  const std::string info_path = std::format("sk1/{}.stexinfo", name);
  if (archive.Has(stex_path)) {
    asset.texture_sets.push_back(ParseStex(archive.Read(stex_path)));
    const auto &set = asset.texture_sets.back();
    for (std::uint32_t i = 0; i < set.textures.size(); ++i)
      asset.textures.push_back(TextureRef{0, i});
  } else if (archive.Has(info_path)) {
    const auto names = ParseStexInfo(archive.Read(info_path));
    asset.texture_sets.reserve(names.size());
    for (const auto &texture_name : names) {
      const std::string path = std::format("sk1/{}.mtex", texture_name);
      if (!archive.Has(path)) {
        lucent::warn("render", "{}: .stexinfo names '{}' but {} is absent",
                     name, texture_name, path);
        asset.texture_sets.push_back({});
      } else {
        asset.texture_sets.push_back(
            ParseMtex(archive.Read(path), texture_name));
      }
      const std::uint32_t set = asset.texture_sets.size() - 1;
      asset.textures.push_back(asset.texture_sets.back().textures.empty()
                                   ? std::optional<TextureRef>{}
                                   : TextureRef{set, 0});
    }
  }

  asset.draw_textures.resize(asset.model.draws.size());
  for (std::size_t i = 0; i < asset.model.draws.size(); ++i) {
    const std::uint32_t material = asset.model.draws[i].material;
    if (material >= asset.model.materials.size()) {
      lucent::warn("render", "{}: draw range {} references material {} of {}",
                   name, i, material, asset.model.materials.size());
      continue;
    }
    const auto &description = asset.model.materials[material];
    if (description.texture_index >= asset.textures.size() ||
        !asset.TextureAt(description.texture_index)) {
      lucent::warn("render", "{}: material '{}' references texture {} of {}",
                   name, description.name, description.texture_index,
                   asset.textures.size());
      continue;
    }
    asset.draw_textures[i] = description.texture_index;
  }

  const auto *position = asset.model.Find(VertexUsage::kPosition);
  if (!position)
    throw Error(std::format("{}: model has no position attribute", name));
  const auto vertices = asset.model.vertices();
  for (int axis = 0; axis < 3; ++axis) {
    asset.lo[axis] = 1e30f;
    asset.hi[axis] = -1e30f;
  }
  for (std::uint32_t i = 0; i < asset.model.vertex_count; ++i) {
    float point[3];
    std::memcpy(point,
                vertices.data() + i * asset.model.vertex_stride +
                    position->offset,
                sizeof(point));
    for (int axis = 0; axis < 3; ++axis) {
      asset.lo[axis] = std::min(asset.lo[axis], point[axis]);
      asset.hi[axis] = std::max(asset.hi[axis], point[axis]);
    }
  }
  *out = std::move(asset);
  return true;
}

} // namespace mcf
