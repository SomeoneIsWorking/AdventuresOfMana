// Desktop host: opens sk1.mpk, uploads a model + its texture, and draws it with
// the game's OWN GLES2 shaders (lifted verbatim from libmcfandroid.so .rodata).
//
// --screenshot renders a single frame and writes a PNG, so correctness can be
// checked without a display. That is the acceptance test for this stage.
#include <SDL3/SDL.h>
#include <GLES2/gl2.h>

#include <cmath>
#include <cstring>
#include <format>
#include <numbers>
#include <string>
#include <vector>

#include <lucent/config.h>
#include <lucent/log.h>

#include "mcf/mcf.h"

namespace {

// --- the shipping game's shaders, byte-for-byte from the binary --------------
// Matches the 24-byte vertex layout (position, color, texcoord0).
constexpr const char* kVS =
    "attribute vec4 position; attribute vec4 color; attribute vec2 texcoord0; "
    "uniform mat4 mVP; varying vec4 colorVarying; varying vec2 texcoordVarying; "
    "void main() { gl_Position = mVP * position; colorVarying = color; "
    "texcoordVarying = texcoord0; }";

// The game's skinning vertex shader, from .rodata. vJoint holds THREE vec4 per
// bone -- the rows of a 3x4 matrix -- and the blend is two-bone with weights
// (w, 1-w). Lighting terms are dropped here to match the simple fragment shader
// above; the skinning math is verbatim.
constexpr const char* kVSkin =
    "attribute vec4 position; attribute vec2 texcoord0; attribute vec4 color; "
    "attribute vec4 weight; attribute vec4 incidence; "
    "varying vec2 texcoordVarying; varying vec4 colorVarying; "
    "uniform mat4 mVP; uniform vec4 vJoint[ 80 * 3 ]; "
    "vec3 SkinningPosition( in vec4 vPosition , int sJointID ) { vec3 vResult; "
    "vResult.x = dot( vPosition , vJoint[ sJointID ] ); "
    "vResult.y = dot( vPosition , vJoint[ sJointID + 1 ] ); "
    "vResult.z = dot( vPosition , vJoint[ sJointID + 2 ] ); return vResult; } "
    "void main() { vec4 vPosition = vec4( 0,0,0,1 ); "
    "vPosition.xyz += SkinningPosition( position , int( incidence.x ) * 3 ) * (weight[0]); "
    "vPosition.xyz += SkinningPosition( position , int( incidence.y ) * 3 ) * (1.0 - weight[0]); "
    "texcoordVarying = texcoord0; colorVarying = color; gl_Position = mVP * vPosition; }";

constexpr const char* kFS =
    "precision highp float; uniform sampler2D texture0; varying vec4 colorVarying; "
    "varying vec2 texcoordVarying; void main() { vec4 color = texture2D( texture0 , "
    "texcoordVarying ); gl_FragColor = colorVarying * color; }";

struct Mat4 {
    float m[16]{};
    static Mat4 Identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
        return r;
    }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int i = 0; i < 4; ++i) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + i] * o.m[c * 4 + k];
                r.m[c * 4 + i] = s;
            }
        return r;
    }
};

Mat4 Perspective(float fovy, float aspect, float zn, float zf) {
    Mat4 r;
    float f = 1.f / std::tan(fovy * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf);
    r.m[11] = -1.f;
    r.m[14] = (2.f * zf * zn) / (zn - zf);
    return r;
}

Mat4 LookAt(const float e[3], const float c[3], const float up[3]) {
    auto sub = [](const float a[3], const float b[3], float o[3]) {
        for (int i = 0; i < 3; ++i) o[i] = a[i] - b[i];
    };
    auto norm = [](float v[3]) {
        float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (l > 0) for (int i = 0; i < 3; ++i) v[i] /= l;
    };
    auto cross = [](const float a[3], const float b[3], float o[3]) {
        o[0] = a[1] * b[2] - a[2] * b[1];
        o[1] = a[2] * b[0] - a[0] * b[2];
        o[2] = a[0] * b[1] - a[1] * b[0];
    };
    float fwd[3], s[3], u[3];
    sub(c, e, fwd); norm(fwd);
    cross(fwd, up, s); norm(s);
    cross(s, fwd, u);
    Mat4 r = Mat4::Identity();
    r.m[0] = s[0];  r.m[4] = s[1];  r.m[8]  = s[2];
    r.m[1] = u[0];  r.m[5] = u[1];  r.m[9]  = u[2];
    r.m[2] = -fwd[0]; r.m[6] = -fwd[1]; r.m[10] = -fwd[2];
    r.m[12] = -(s[0]*e[0] + s[1]*e[1] + s[2]*e[2]);
    r.m[13] = -(u[0]*e[0] + u[1]*e[1] + u[2]*e[2]);
    r.m[14] =  (fwd[0]*e[0] + fwd[1]*e[1] + fwd[2]*e[2]);
    return r;
}

// Column-major 4x4, matching the file layout and GL.
void MatMul(const float* a, const float* b, float* o) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[k * 4 + r] * b[c * 4 + k];
            o[c * 4 + r] = s;
        }
}

void QuatTransToMat(const float q[4], const float t[3], float* o) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    o[0]  = 1 - 2 * (y * y + z * z); o[1]  = 2 * (x * y + z * w);     o[2]  = 2 * (x * z - y * w);     o[3]  = 0;
    o[4]  = 2 * (x * y - z * w);     o[5]  = 1 - 2 * (x * x + z * z); o[6]  = 2 * (y * z + x * w);     o[7]  = 0;
    o[8]  = 2 * (x * z + y * w);     o[9]  = 2 * (y * z - x * w);     o[10] = 1 - 2 * (x * x + y * y); o[11] = 0;
    o[12] = t[0]; o[13] = t[1]; o[14] = t[2]; o[15] = 1;
}

GLuint Compile(GLenum kind, const char* src) {
    GLuint s = glCreateShader(kind);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        throw mcf::Error(std::format("shader compile failed: {}", log));
    }
    return s;
}

void WritePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);

}  // namespace

int main(int argc, char** argv) {
    lucent::config::set_prefix("MANA_");

    std::string archive = "scratch/raw/assets/sk1/sk1.mpk";
    std::string model = "B0000_00";
    std::string shot, anim;
    float anim_t = 0.f;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--archive" && i + 1 < argc) archive = argv[++i];
        else if (a == "--model" && i + 1 < argc) model = argv[++i];
        else if (a == "--screenshot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--anim" && i + 1 < argc) anim = argv[++i];
        else if (a == "--time" && i + 1 < argc) anim_t = std::stof(argv[++i]);
        else if (a == "--help") {
            std::printf("usage: %s [--archive sk1.mpk] [--model NAME] "
                        "[--screenshot out.png]\n", argv[0]);
            return 0;
        }
    }

    try {
        mcf::Archive ar(archive);
        lucent::info("assets", "opened {} ({} entries)", archive, ar.entries().size());

        auto mdl = mcf::ParseSmdl(ar.Read(std::format("sk1/{}.smdl", model)));
        lucent::info("assets", "{}: {} verts, {} indices ({}-bit), stride {}, {} bones",
                     model, mdl.vertex_count, mdl.index_count, mdl.index_size * 8,
                     mdl.vertex_stride, mdl.bone_count);

        mcf::Motion motion;
        bool have_anim = false;
        if (!anim.empty()) {
            motion = mcf::ParseSmot(ar.Read(std::format("sk1/{}.smot", anim)));
            have_anim = true;
            lucent::info("assets", "motion {}: {} tracks, {:.0f} frames", anim,
                         motion.tracks.size(), motion.duration);
            if (motion.tracks.size() != mdl.bones.size())
                lucent::warn("assets", "motion has {} tracks but model has {} bones",
                             motion.tracks.size(), mdl.bones.size());
        }

        // Two texture paths. Characters ship a .stex holding their atlases
        // inline. Maps instead ship a .stexinfo NAME LIST, and each name
        // resolves to a shared sk1/<name>.mtex -- so map textures are pooled
        // across rooms rather than duplicated per room.
        std::vector<mcf::TextureSet> owned;
        std::vector<const mcf::Texture*> textures_src;
        std::string texname = std::format("sk1/{}.stex", model);
        std::string infoname = std::format("sk1/{}.stexinfo", model);

        if (ar.Has(texname)) {
            owned.push_back(mcf::ParseStex(ar.Read(texname)));
            for (const auto& t : owned.back().textures) textures_src.push_back(&t);
        } else if (ar.Has(infoname)) {
            auto names = mcf::ParseStexInfo(ar.Read(infoname));
            lucent::info("assets", "  .stexinfo lists {} shared textures", names.size());
            owned.reserve(names.size());
            for (const auto& n : names) {
                std::string mt = std::format("sk1/{}.mtex", n);
                if (!ar.Has(mt)) {
                    // Must not be silent: a missing entry shifts every later
                    // index and would mis-texture the whole room.
                    lucent::warn("assets", "  .stexinfo names '{}' but {} is not in "
                                 "the archive; that slot draws white", n, mt);
                    owned.push_back({});
                    continue;
                }
                owned.push_back(mcf::ParseMtex(ar.Read(mt), n));
            }
            for (const auto& o : owned)
                textures_src.push_back(o.textures.empty() ? nullptr : &o.textures[0]);
        }
        mcf::TextureSet tex;
        if (!textures_src.empty()) {
            for (const auto* t : textures_src)
                if (t) lucent::info("assets", "  texture '{}' {}x{} mips={}", t->name,
                                    t->width, t->height, t->mips);
        } else {
            // Map geometry does not carry a .stex; it binds shared textures via
            // .stexinfo + .mtex through AppMapTexture::SetBinary, which is not
            // reversed yet. Not an error -- a known gap.
            lucent::warn("assets", "neither {} nor {} in archive; drawing untextured",
                         texname, infoname);
        }

        for (size_t i = 0; i < mdl.materials.size(); ++i)
            lucent::info("assets", "  material {} '{}' -> texture {}", i,
                         mdl.materials[i].name, mdl.materials[i].texture_index);
        lucent::info("assets", "  {} draw range(s)", mdl.draws.size());

        if (shot.empty() && !SDL_getenv("DISPLAY") && !SDL_getenv("WAYLAND_DISPLAY"))
            lucent::warn("host", "no display detected; use --screenshot for headless");
        if (!shot.empty() && !SDL_getenv("DISPLAY") && !SDL_getenv("WAYLAND_DISPLAY"))
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

        if (!SDL_Init(SDL_INIT_VIDEO))
            throw mcf::Error(std::format("SDL_Init: {}", SDL_GetError()));

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        const int W = 720, H = 720;
        SDL_Window* win = SDL_CreateWindow("Adventures of Mana", W, H, SDL_WINDOW_OPENGL);
        if (!win) throw mcf::Error(std::format("SDL_CreateWindow: {}", SDL_GetError()));
        SDL_GLContext ctx = SDL_GL_CreateContext(win);
        if (!ctx) throw mcf::Error(std::format("SDL_GL_CreateContext: {}", SDL_GetError()));
        lucent::info("host", "GL_VERSION  {}", (const char*)glGetString(GL_VERSION));
        lucent::info("host", "GL_RENDERER {}", (const char*)glGetString(GL_RENDERER));

        const bool skinned = mdl.Find(mcf::VertexUsage::kWeight) != nullptr;
        GLuint prog = glCreateProgram();
        glAttachShader(prog, Compile(GL_VERTEX_SHADER, skinned ? kVSkin : kVS));
        glAttachShader(prog, Compile(GL_FRAGMENT_SHADER, kFS));
        glBindAttribLocation(prog, 0, "position");
        glBindAttribLocation(prog, 1, "color");
        glBindAttribLocation(prog, 2, "texcoord0");
        glBindAttribLocation(prog, 3, "weight");
        glBindAttribLocation(prog, 4, "incidence");
        glLinkProgram(prog);
        GLint linked = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[2048]{};
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            throw mcf::Error(std::format("link failed: {}", log));
        }

        GLuint vbo, ibo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        auto vs = mdl.vertices();
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vs.size()), vs.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        auto is = mdl.indices();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(is.size()), is.data(),
                     GL_STATIC_DRAW);

        // A missing texture must not render as black -- that is visually
        // identical to "nothing drew", and would make a real failure look like
        // a content gap (or vice versa). Untextured geometry shows white.
        GLuint gltex = 0;
        glGenTextures(1, &gltex);
        glBindTexture(GL_TEXTURE_2D, gltex);
        const uint8_t kWhite[4] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, kWhite);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        std::vector<GLuint> textures;
        for (const auto* tp : textures_src) {
            if (!tp) { textures.push_back(gltex); continue; }
            const auto& t = *tp;
            GLuint id = 0;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(t.width), GLsizei(t.height),
                         0, GL_RGBA, GL_UNSIGNED_BYTE, t.pixels.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            textures.push_back(id);
        }

        // Resolve each draw range to a GL texture once, up front, so a bad
        // reference is reported by name rather than silently drawing the wrong
        // atlas. B0000_00 ships exactly one such range: 2 triangles pointing at
        // material 2 when the model declares 2 materials. That is a defect in
        // the shipped data (1 of 20,642 ranges across all 1375 models), so it
        // falls back to white rather than aborting the draw.
        std::vector<GLuint> draw_tex(mdl.draws.size(), gltex);
        for (size_t i = 0; i < mdl.draws.size(); ++i) {
            uint32_t mi = mdl.draws[i].material;
            if (mi >= mdl.materials.size()) {
                lucent::warn("assets", "draw range {} references material {} but the "
                             "model declares {}; drawing it untextured",
                             i, mi, mdl.materials.size());
                continue;
            }
            uint32_t ti = mdl.materials[mi].texture_index;
            if (ti >= textures.size()) {
                lucent::warn("assets", "material '{}' references texture {} but the "
                             ".stex holds {}; drawing it untextured",
                             mdl.materials[mi].name, ti, textures.size());
                continue;
            }
            draw_tex[i] = textures[ti];
        }

        // Frame the model from its own bounds so any model fills the view.
        const auto* pa = mdl.Find(mcf::VertexUsage::kPosition);
        if (!pa) throw mcf::Error("model has no position attribute");
        float lo[3]{1e30f, 1e30f, 1e30f}, hi[3]{-1e30f, -1e30f, -1e30f};
        for (uint32_t i = 0; i < mdl.vertex_count; ++i) {
            float p[3];
            std::memcpy(p, vs.data() + i * mdl.vertex_stride + pa->offset, 12);
            for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], p[k]); hi[k] = std::max(hi[k], p[k]); }
        }
        float ctr[3], radius = 0;
        for (int k = 0; k < 3; ++k) { ctr[k] = (lo[k] + hi[k]) * .5f; radius = std::max(radius, hi[k] - lo[k]); }
        lucent::info("host", "bounds ({:.1f},{:.1f},{:.1f})..({:.1f},{:.1f},{:.1f})",
                     lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);

        const auto* ca = mdl.Find(mcf::VertexUsage::kColor);
        const auto* ta = mdl.Find(mcf::VertexUsage::kTexcoord0);

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.10f, 0.11f, 0.14f, 1.f);

        float yaw = 0.6f;
        bool running = true;
        int frames = 0;
        while (running) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_EVENT_QUIT) running = false;
                if (ev.type == SDL_EVENT_KEY_DOWN) {
                    if (ev.key.key == SDLK_ESCAPE) running = false;
                    if (ev.key.key == SDLK_LEFT) yaw -= 0.15f;
                    if (ev.key.key == SDLK_RIGHT) yaw += 0.15f;
                }
            }

            float d = radius * 1.6f;
            float eye[3]{ctr[0] + std::sin(yaw) * d, ctr[1] + radius * 0.25f,
                         ctr[2] + std::cos(yaw) * d};
            float up[3]{0, 1, 0};
            Mat4 vp = Perspective(45.f * float(std::numbers::pi) / 180.f, float(W) / H,
                                  radius * 0.05f, radius * 8.f) *
                      LookAt(eye, ctr, up);

            glViewport(0, 0, W, H);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(prog);
            glUniformMatrix4fv(glGetUniformLocation(prog, "mVP"), 1, GL_FALSE, vp.m);

            if (skinned) {
                // world = parent_world * local; skin = world * inv_bind_world.
                // Bones are topologically sorted (parent index < own index) in
                // every shipped model, so one forward pass suffices.
                const size_t nb = mdl.bones.size();
                std::vector<std::array<float, 16>> world(nb);
                std::vector<float> joints(80 * 3 * 4, 0.f);
                for (size_t bi = 0; bi < nb; ++bi) {
                    const auto& bn = mdl.bones[bi];
                    std::array<float, 16> local{};
                    std::memcpy(local.data(), bn.local, 64);
                    if (have_anim) {
                        for (const auto& tr : motion.tracks) {
                            if (tr.name != bn.name || tr.times.empty()) continue;
                            size_t k = 0;
                            while (k + 1 < tr.times.size() && tr.times[k + 1] <= anim_t) ++k;
                            float t[3]{bn.local[12], bn.local[13], bn.local[14]};
                            if (!tr.trans.empty())
                                for (int j = 0; j < 3; ++j) t[j] = tr.trans[k][j];
                            if (!tr.rot.empty())
                                QuatTransToMat(tr.rot[k].data(), t, local.data());
                            else { local[12] = t[0]; local[13] = t[1]; local[14] = t[2]; }
                            break;
                        }
                    }
                    if (bn.parent < 0) world[bi] = local;
                    else MatMul(world[size_t(bn.parent)].data(), local.data(),
                                world[bi].data());

                    if (bi >= 80) continue;
                    float skin[16];
                    // A zero-scale bone has an infinite inverse bind matrix (the
                    // shipped files literally store inf/nan). Feeding that to the
                    // shader turns every vertex weighted to it into NaN, which
                    // renders as scattered garbage rather than an obvious error,
                    // so those bones are pinned to identity instead.
                    if (bn.degenerate) {
                        std::memset(skin, 0, sizeof skin);
                        skin[0] = skin[5] = skin[10] = skin[15] = 1.f;
                    } else {
                        MatMul(world[bi].data(), bn.inv_world, skin);
                    }
                    // vJoint holds the three ROWS of the 3x4 skin matrix.
                    for (int r = 0; r < 3; ++r)
                        for (int c = 0; c < 4; ++c)
                            joints[(bi * 3 + size_t(r)) * 4 + size_t(c)] = skin[c * 4 + r];
                }
                glUniform4fv(glGetUniformLocation(prog, "vJoint"), 80 * 3, joints.data());
            }
            glUniform1i(glGetUniformLocation(prog, "texture0"), 0);
            glActiveTexture(GL_TEXTURE0);

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            GLsizei stride = GLsizei(mdl.vertex_stride);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                                  (void*)(uintptr_t)pa->offset);
            if (ca) {
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                                      (void*)(uintptr_t)ca->offset);
            } else {
                glDisableVertexAttribArray(1);
                glVertexAttrib4f(1, 1, 1, 1, 1);
            }
            if (skinned) {
                const auto* wa = mdl.Find(mcf::VertexUsage::kWeight);
                const auto* ia = mdl.Find(mcf::VertexUsage::kIncidence);
                glEnableVertexAttribArray(3);
                glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                                      (void*)(uintptr_t)wa->offset);
                glEnableVertexAttribArray(4);
                glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_FALSE, stride,
                                      (void*)(uintptr_t)ia->offset);
            }
            if (ta) {
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                                      (void*)(uintptr_t)ta->offset);
            }
            GLenum itype = mdl.index_size == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
            if (mdl.draws.empty()) {
                glBindTexture(GL_TEXTURE_2D, gltex);
                glDrawElements(GL_TRIANGLES, GLsizei(mdl.index_count), itype, nullptr);
            } else {
                for (size_t i = 0; i < mdl.draws.size(); ++i) {
                    glBindTexture(GL_TEXTURE_2D, draw_tex[i]);
                    glDrawElements(GL_TRIANGLES, GLsizei(mdl.draws[i].index_count), itype,
                                   (void*)(uintptr_t)mdl.draws[i].byte_offset);
                }
            }

            if (!shot.empty()) {
                std::vector<uint8_t> px(size_t(W) * H * 4);
                glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                // glReadPixels is bottom-up; flip to image order.
                std::vector<uint8_t> flip(px.size());
                for (int y = 0; y < H; ++y)
                    std::memcpy(&flip[size_t(y) * W * 4],
                                &px[size_t(H - 1 - y) * W * 4], size_t(W) * 4);
                WritePng(shot, W, H, flip);
                lucent::info("host", "wrote {}", shot);
                running = false;
            }
            SDL_GL_SwapWindow(win);
            ++frames;
        }
        lucent::info("host", "{} frames", frames);
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
    } catch (const std::exception& e) {
        lucent::error("host", "{}", e.what());
        return 1;
    }
    return 0;
}

// --- minimal PNG writer (zlib "stored" blocks; no external image dep) --------
namespace {
uint32_t Crc32(const uint8_t* d, size_t n, uint32_t crc = 0) {
    static uint32_t tbl[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            tbl[i] = c;
        }
        init = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = tbl[(crc ^ d[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

void Put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(uint8_t(x >> 24)); v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 8));  v.push_back(uint8_t(x));
}

void Chunk(std::vector<uint8_t>& out, const char* tag, const std::vector<uint8_t>& d) {
    Put32(out, uint32_t(d.size()));
    std::vector<uint8_t> body(tag, tag + 4);
    body.insert(body.end(), d.begin(), d.end());
    out.insert(out.end(), body.begin(), body.end());
    Put32(out, Crc32(body.data(), body.size()));
}

void WritePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba) {
    std::vector<uint8_t> raw;
    raw.reserve(size_t(h) * (size_t(w) * 4 + 1));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), &rgba[size_t(y) * w * 4], &rgba[size_t(y) * w * 4] + size_t(w) * 4);
    }
    std::vector<uint8_t> z{0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    for (size_t i = 0; i < raw.size(); i += 65535) {
        uint16_t n = uint16_t(std::min<size_t>(65535, raw.size() - i));
        z.push_back(i + n >= raw.size() ? 1 : 0);
        z.push_back(uint8_t(n)); z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(~n)); z.push_back(uint8_t(~n >> 8));
        z.insert(z.end(), raw.begin() + long(i), raw.begin() + long(i + n));
    }
    Put32(z, (b << 16) | a);

    std::vector<uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    Put32(ihdr, uint32_t(w)); Put32(ihdr, uint32_t(h));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    Chunk(png, "IHDR", ihdr);
    Chunk(png, "IDAT", z);
    Chunk(png, "IEND", {});
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fwrite(png.data(), 1, png.size(), f);
        std::fclose(f);
    }
}
}  // namespace
