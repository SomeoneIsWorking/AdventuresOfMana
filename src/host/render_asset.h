#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "host/render_normals.h"
#include "mcf/mcf.h"

namespace mcf {

class Archive;

struct TextureRef {
  std::uint32_t set = 0;
  std::uint32_t texture = 0;
};

// Backend-independent render input. Texture spans remain valid because their
// owning TextureSets live beside the model for the asset's entire lifetime.
struct RenderAsset {
  std::string name;
  Model model;
  std::vector<TextureSet> texture_sets;
  std::vector<std::optional<TextureRef>> textures;
  std::vector<std::optional<std::uint32_t>> draw_textures;
  NormalGeneration normals;
  float lo[3]{}, hi[3]{};

  bool skinned() const { return model.Find(VertexUsage::kWeight) != nullptr; }
  const Texture *TextureAt(std::uint32_t index) const;
};

// Parses one model and resolves its inline or shared texture list without
// creating resources for any graphics backend.
bool LoadRenderAsset(const Archive &archive, const std::string &name,
                     RenderAsset *out);

} // namespace mcf
