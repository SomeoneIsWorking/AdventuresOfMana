// GPU-resident model: buffers, textures, and per-material draw ranges.
#pragma once

#include <GLES2/gl2.h>

#include <string>
#include <vector>

#include "mcf/mcf.h"

namespace mcf {

class Archive;

struct Renderable {
    Model model;
    std::vector<TextureSet> owned;
    GLuint vbo = 0, ibo = 0;
    GLuint white = 0;                 // 1x1 fallback; see LoadRenderable
    std::vector<GLuint> textures;     // parallel to the model's texture list
    std::vector<GLuint> draw_tex;     // resolved per draw range
    float lo[3]{}, hi[3]{};

    bool skinned() const { return model.Find(VertexUsage::kWeight) != nullptr; }
};

// Loads `sk1/<name>.smdl` plus whichever texture path that model uses -- an
// inline `.stex` for characters, or a `.stexinfo` name list resolving to shared
// `.mtex` files for maps. Returns false if the model is not in the archive.
bool LoadRenderable(const Archive& ar, const std::string& name, GLuint white,
                    Renderable* out);

// Fills `out` (80*3*4 floats) with the vJoint palette: three vec4 rows per bone
// of `world_animated * inv_world_bind`. With `motion == nullptr` this collapses
// to identity, i.e. the bind pose.
void BuildJointPalette(const Model& m, const Motion* motion, float time,
                       std::vector<float>* out);

// World-space position of a named bone in the model's animated pose, before the
// actor's own placement transform. Returns false if the model has no such bone.
bool BoneLocalPos(const Model& m, const Motion* motion, float time,
                  const std::string& bone, float out[3]);

// Resolves a script-level actor type id to a model name. Verified against the
// enums in sk1.lua: all 74 eENEMY ids have E<id>_00 and all 24 eBOSS ids have
// B<id>_00.
//
// NPCs are NOT id-for-id: eNPC id 10 is N0000, 11 is N0001, and so on. The
// developers annotated the enum with the model for each id and all 25 annotated
// entries say id-10, none say id. Under that rule every eNPC id >= 10 resolves;
// ids 1..9 are named party members with no N#### model.
std::string ActorModelName(char kind_prefix, int type_id);

}  // namespace mcf
