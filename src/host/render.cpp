#include "host/render.h"

#include <array>
#include <cstring>
#include <format>
#include <string_view>

#include <lucent/log.h>

namespace mcf {

static void MatMul4(const float* a, const float* b, float* o) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[k * 4 + r] * b[c * 4 + k];
            o[c * 4 + r] = s;
        }
}

static void QuatTrans(const float q[4], const float t[3], float* o) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    o[0]  = 1 - 2 * (y*y + z*z); o[1]  = 2 * (x*y + z*w);     o[2]  = 2 * (x*z - y*w);     o[3]  = 0;
    o[4]  = 2 * (x*y - z*w);     o[5]  = 1 - 2 * (x*x + z*z); o[6]  = 2 * (y*z + x*w);     o[7]  = 0;
    o[8]  = 2 * (x*z + y*w);     o[9]  = 2 * (y*z - x*w);     o[10] = 1 - 2 * (x*x + y*y); o[11] = 0;
    o[12] = t[0]; o[13] = t[1]; o[14] = t[2]; o[15] = 1;
}

void BuildJointPalette(const Model& m, const Motion* motion, float time,
                       std::vector<float>* out) {
    out->assign(80 * 3 * 4, 0.f);
    const size_t nb = m.bones.size();
    std::vector<std::array<float, 16>> world(nb);
    for (size_t i = 0; i < nb; ++i) {
        const auto& bn = m.bones[i];
        std::array<float, 16> local{};
        std::memcpy(local.data(), bn.local, 64);
        if (motion) {
            for (const auto& tr : motion->tracks) {
                if (tr.name != bn.name || tr.times.empty()) continue;
                size_t k = 0;
                while (k + 1 < tr.times.size() && tr.times[k + 1] <= time) ++k;
                float t[3]{bn.local[12], bn.local[13], bn.local[14]};
                if (!tr.trans.empty()) for (int j = 0; j < 3; ++j) t[j] = tr.trans[k][j];
                if (!tr.rot.empty()) QuatTrans(tr.rot[k].data(), t, local.data());
                else { local[12] = t[0]; local[13] = t[1]; local[14] = t[2]; }
                break;
            }
        }
        // Bones are topologically sorted in every shipped model, so one forward
        // pass suffices.
        if (bn.parent < 0) world[i] = local;
        else MatMul4(world[size_t(bn.parent)].data(), local.data(), world[i].data());

        if (i >= 80) continue;
        float skin[16];
        // A zero-scale bone's inverse bind is inf/nan in the shipped files;
        // feeding that in turns every vertex weighted to it into NaN, which
        // renders as scattered garbage rather than an obvious fault.
        if (bn.degenerate) {
            std::memset(skin, 0, sizeof skin);
            skin[0] = skin[5] = skin[10] = skin[15] = 1.f;
        } else {
            MatMul4(world[i].data(), bn.inv_world, skin);
        }
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                (*out)[(i * 3 + size_t(r)) * 4 + size_t(c)] = skin[c * 4 + r];
    }
}

bool BoneLocalPos(const Model& m, const Motion* motion, float time,
                  const std::string& bone, float out[3]) {
    const size_t nb = m.bones.size();
    std::vector<std::array<float, 16>> world(nb);
    size_t want = nb;
    for (size_t i = 0; i < nb; ++i) {
        const auto& bn = m.bones[i];
        if (bn.name == bone) want = i;
        std::array<float, 16> local{};
        std::memcpy(local.data(), bn.local, 64);
        if (motion) {
            for (const auto& tr : motion->tracks) {
                if (tr.name != bn.name || tr.times.empty()) continue;
                size_t k = 0;
                while (k + 1 < tr.times.size() && tr.times[k + 1] <= time) ++k;
                float t[3]{bn.local[12], bn.local[13], bn.local[14]};
                if (!tr.trans.empty()) for (int j = 0; j < 3; ++j) t[j] = tr.trans[k][j];
                if (!tr.rot.empty()) QuatTrans(tr.rot[k].data(), t, local.data());
                else { local[12] = t[0]; local[13] = t[1]; local[14] = t[2]; }
                break;
            }
        }
        if (bn.parent < 0) world[i] = local;
        else MatMul4(world[size_t(bn.parent)].data(), local.data(), world[i].data());
    }
    if (want == nb) return false;
    for (int k = 0; k < 3; ++k) out[k] = world[want][12 + k];
    return true;
}

std::string ActorModelName(char kind, int type_id) {
    // ModeGame::AddEnemy @ 0x2dda74 normally formats sk1/E%04d_00, but its
    // shipping id-123 branch replaces that result with the literal
    // sk1/B0023_00 before CharacterSetFileName. The Butler transformation is
    // authored as AddEnemy(123), so retain enemy ownership while using the
    // Steward Wolf boss model selected by the original engine.
    if (kind == 'E' && type_id == 123) return "B0023_00";

    // NPCs are offset by 10: eNPC id 10 is N0000, 11 is N0001, and so on. This
    // is not inferred -- the original developers annotated the enum in sk1.lua
    // with the model for each id ("MAN = 13, -- 13 N0003 00 villager(man)"),
    // and all 25 annotated entries say id-10, none say id.
    //
    // The old id==model rule was worse than a missing model: for ids where an
    // N<id>_00 happened to exist it silently drew the WRONG character.
    //
    // Ids 0..9 are the named party members and use the CHARACTER prefix
    // id-for-id: 0 is the hero (the enum literally reads "NONE = 0, -- none
    // (hero)"), and C0001..C0009 all exist. Confirmed by rendering the two most
    // distinctive: eNPC 5 is CHOCOBO and C0005_00 is a chocobo, eNPC 6 is
    // CHOCOBOT and C0006_00 is the same bird in armour. That is a semantic
    // check, not just "a file with that name exists".
    if (kind == 'N') {
        // eNPC.TRANS = -1, annotated 透明 ("transparent") in the enum: a
        // deliberately invisible NPC used as a pure trigger/anchor. It has no
        // model by design, so returning a name at all would make the room
        // census report a missing asset that does not exist.
        if (type_id < 0) return {};
        // `npc()` is also the authored cutscene-character constructor. The
        // Lua enum deliberately folds enemy and boss model namespaces into
        // eNPC with ENEMY=100 and BOSS=1000, then adds the corresponding enum
        // id (for example Julius is 1000+20). Decode those tagged ranges
        // before applying the ordinary-NPC offset.
        if (type_id >= 1000) return std::format("B{:04d}_00", type_id - 1000);
        if (type_id >= 100) return std::format("E{:04d}_00", type_id - 100);
        if (type_id < 10) return std::format("C{:04d}_00", type_id);
        return std::format("N{:04d}_00", type_id - 10);
    }
    return std::format("{}{:04d}_00", kind, type_id);
}

int RunActorModelSelfTest() {
    int bad = 0;
    auto check = [&](std::string_view what, bool pass) {
        if (!pass) { ++bad; lucent::error("render", "SELFTEST FAIL: {}", what); }
        else lucent::info("render", "  ok: {}", what);
    };
    check("NPC-tagged enemies use the enemy namespace",
          ActorModelName('N', 100) == "E0000_00" &&
              ActorModelName('N', 173) == "E0073_00");
    check("NPC-tagged bosses use the boss namespace",
          ActorModelName('N', 1010) == "B0010_00" &&
              ActorModelName('N', 1020) == "B0020_00");
    check("ordinary and party NPC mappings remain distinct",
          ActorModelName('N', 10) == "N0000_00" &&
              ActorModelName('N', 5) == "C0005_00");
    check("AddEnemy 123 alone selects the Steward Wolf boss model",
          ActorModelName('E', 123) == "B0023_00" &&
              ActorModelName('E', 23) == "E0023_00");
    lucent::info("render", "SELFTEST: 4 cases, {} failures", bad);
    return bad;
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
