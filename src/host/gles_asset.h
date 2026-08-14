#pragma once

#include <GLES2/gl2.h>

#include <string>
#include <vector>

#include "host/render_asset.h"

namespace mcf {
class Archive;
}

namespace mana::gles {

// Transitional GLES upload owner. World and GPU-neutral code consume the
// contained RenderAsset; only the legacy GLES renderer touches these handles.
struct Asset {
  Asset() = default;
  ~Asset();

  Asset(const Asset &) = delete;
  Asset &operator=(const Asset &) = delete;
  Asset(Asset &&other) noexcept;
  Asset &operator=(Asset &&other) noexcept;

  mcf::RenderAsset source;
  GLuint vertices = 0;
  GLuint indices = 0;
  GLuint white = 0;
  std::vector<GLuint> textures;
  std::vector<GLuint> draw_textures;

  bool skinned() const { return source.skinned(); }

private:
  void Release();
};

bool LoadAsset(const mcf::Archive &archive, const std::string &name,
               GLuint white, Asset *out);

} // namespace mana::gles
