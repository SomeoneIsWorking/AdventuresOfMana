// GPU-resident model: buffers, textures, and per-material draw ranges.
#pragma once

#include <GLES2/gl2.h>

#include <string>
#include <vector>

#include "host/render_asset.h"
#include "host/render_pose.h"
#include "mcf/mcf.h"

namespace mcf {

class Archive;

struct Renderable {
    RenderAsset asset;
    GLuint vbo = 0, ibo = 0;
    GLuint white = 0;                 // 1x1 fallback; see LoadRenderable
    std::vector<GLuint> textures;     // parallel to the model's texture list
    std::vector<GLuint> draw_tex;     // resolved per draw range

    bool skinned() const { return asset.skinned(); }
};

// Loads `sk1/<name>.smdl` plus whichever texture path that model uses -- an
// inline `.stex` for characters, or a `.stexinfo` name list resolving to shared
// `.mtex` files for maps. Returns false if the model is not in the archive.
bool LoadRenderable(const Archive& ar, const std::string& name, GLuint white,
                    Renderable* out);

// Resolves a script-level actor type id to a model name. Verified against the
// enums in sk1.lua: all 73 ordinary positive eENEMY ids have E<id>_00 and all
// 23 positive eBOSS ids have B<id>_00 (counting distinct ids > 0; several
// names alias the same id). AddEnemy id 123 is the shipping exception and
// selects the Steward Wolf model B0023_00.
//
// Ordinary NPCs need two rules, and together they resolve all 35 direct eNPC
// ids. The enum also tags cutscene enemies/bosses as 100+eENEMY and
// 1000+eBOSS; ActorModelName decodes those into E/B namespaces first.
//   id 0..9   -> C<id>_00   the named party members, id-for-id on the CHARACTER
//                           prefix (0 is the hero)
//   id >= 10  -> N<id-10>_00
// The offset is the developers' own: they annotated the enum with the model for
// each id, and all 25 annotated entries say id-10, none say id.
std::string ActorModelName(char kind_prefix, int type_id);
int RunActorModelSelfTest();

}  // namespace mcf
