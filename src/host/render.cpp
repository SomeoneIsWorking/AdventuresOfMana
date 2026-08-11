#include "host/render.h"

#include <cstring>
#include <format>

#include <lucent/log.h>

namespace mcf {

std::string ActorModelName(char kind, int type_id) {
    return std::format("{}{:04d}_00", kind, type_id);
}

static GLuint UploadTexture(const Texture& t) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(t.width), GLsizei(t.height), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, t.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return id;
}

bool LoadRenderable(const Archive& ar, const std::string& name, GLuint white,
                    Renderable* out) {
    std::string mdl = std::format("sk1/{}.smdl", name);
    if (!ar.Has(mdl)) return false;
    out->model = ParseSmdl(ar.Read(mdl));
    out->white = white;

    std::vector<const Texture*> src;
    std::string stex = std::format("sk1/{}.stex", name);
    std::string info = std::format("sk1/{}.stexinfo", name);
    if (ar.Has(stex)) {
        out->owned.push_back(ParseStex(ar.Read(stex)));
        for (const auto& t : out->owned.back().textures) src.push_back(&t);
    } else if (ar.Has(info)) {
        auto names = ParseStexInfo(ar.Read(info));
        out->owned.reserve(names.size());
        for (const auto& n : names) {
            std::string mt = std::format("sk1/{}.mtex", n);
            if (!ar.Has(mt)) {
                // Silence here would shift every later index and mis-texture
                // the whole room.
                lucent::warn("render", "{}: .stexinfo names '{}' but {} is absent",
                             name, n, mt);
                out->owned.push_back({});
                continue;
            }
            out->owned.push_back(ParseMtex(ar.Read(mt), n));
        }
        for (const auto& o : out->owned)
            src.push_back(o.textures.empty() ? nullptr : &o.textures[0]);
    }
    for (const auto* t : src) out->textures.push_back(t ? UploadTexture(*t) : white);

    glGenBuffers(1, &out->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, out->vbo);
    auto vs = out->model.vertices();
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vs.size()), vs.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &out->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out->ibo);
    auto is = out->model.indices();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(is.size()), is.data(), GL_STATIC_DRAW);

    // Resolve every draw range up front so a bad reference is named rather than
    // silently drawing whatever texture happened to be bound.
    out->draw_tex.assign(out->model.draws.size(), white);
    for (size_t i = 0; i < out->model.draws.size(); ++i) {
        uint32_t mi = out->model.draws[i].material;
        if (mi >= out->model.materials.size()) {
            lucent::warn("render", "{}: draw range {} references material {} of {}",
                         name, i, mi, out->model.materials.size());
            continue;
        }
        uint32_t ti = out->model.materials[mi].texture_index;
        if (ti >= out->textures.size()) {
            lucent::warn("render", "{}: material '{}' references texture {} of {}",
                         name, out->model.materials[mi].name, ti, out->textures.size());
            continue;
        }
        out->draw_tex[i] = out->textures[ti];
    }

    const auto* pa = out->model.Find(VertexUsage::kPosition);
    for (int k = 0; k < 3; ++k) { out->lo[k] = 1e30f; out->hi[k] = -1e30f; }
    for (uint32_t i = 0; i < out->model.vertex_count; ++i) {
        float p[3];
        std::memcpy(p, vs.data() + i * out->model.vertex_stride + pa->offset, 12);
        for (int k = 0; k < 3; ++k) {
            out->lo[k] = std::min(out->lo[k], p[k]);
            out->hi[k] = std::max(out->hi[k], p[k]);
        }
    }
    return true;
}

}  // namespace mcf
