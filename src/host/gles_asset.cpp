#include "host/gles_asset.h"

#include <utility>

#include "mcf/mcf.h"

namespace mana::gles {
namespace {

GLuint UploadTexture(const mcf::Texture &texture) {
  GLuint id = 0;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(texture.width),
               GLsizei(texture.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
               texture.pixels.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  return id;
}

} // namespace

Asset::~Asset() { Release(); }

Asset::Asset(Asset &&other) noexcept { *this = std::move(other); }

Asset &Asset::operator=(Asset &&other) noexcept {
  if (this == &other)
    return *this;
  Release();
  source = std::move(other.source);
  vertices = std::exchange(other.vertices, 0);
  indices = std::exchange(other.indices, 0);
  white = std::exchange(other.white, 0);
  textures = std::move(other.textures);
  draw_textures = std::move(other.draw_textures);
  return *this;
}

void Asset::Release() {
  for (const GLuint texture : textures) {
    if (texture && texture != white)
      glDeleteTextures(1, &texture);
  }
  if (indices)
    glDeleteBuffers(1, &indices);
  if (vertices)
    glDeleteBuffers(1, &vertices);
  textures.clear();
  draw_textures.clear();
  indices = 0;
  vertices = 0;
  white = 0;
}

bool LoadAsset(const mcf::Archive &archive, const std::string &name,
               GLuint white, Asset *out) {
  Asset asset;
  if (!mcf::LoadRenderAsset(archive, name, &asset.source))
    return false;
  asset.white = white;

  for (std::uint32_t i = 0; i < asset.source.textures.size(); ++i) {
    const mcf::Texture *texture = asset.source.TextureAt(i);
    asset.textures.push_back(texture ? UploadTexture(*texture) : white);
  }

  glGenBuffers(1, &asset.vertices);
  glBindBuffer(GL_ARRAY_BUFFER, asset.vertices);
  const auto vertices = asset.source.model.vertices();
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size()), vertices.data(),
               GL_STATIC_DRAW);
  glGenBuffers(1, &asset.indices);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, asset.indices);
  const auto indices = asset.source.model.indices();
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(indices.size()),
               indices.data(), GL_STATIC_DRAW);

  asset.draw_textures.assign(asset.source.model.draws.size(), white);
  for (std::size_t i = 0; i < asset.source.draw_textures.size(); ++i) {
    if (asset.source.draw_textures[i])
      asset.draw_textures[i] = asset.textures[*asset.source.draw_textures[i]];
  }
  *out = std::move(asset);
  return true;
}

} // namespace mana::gles
