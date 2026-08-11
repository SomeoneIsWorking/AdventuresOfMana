// Desktop host: opens sk1.mpk, uploads a model + its texture, and draws it with
// the game's OWN GLES2 shaders (lifted verbatim from libmcfandroid.so .rodata).
//
// --screenshot renders a single frame and writes a PNG, so correctness can be
// checked without a display. That is the acceptance test for this stage.
#include <SDL3/SDL.h>
#include <GLES2/gl2.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <numbers>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <lucent/config.h>
#include <lucent/log.h>

#include "engine/script.h"
#include "engine/audio.h"
#include "engine/world.h"
#include "host/render.h"
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

// The game's own fade shader pair, verbatim from .rodata: a clip-space quad in
// a flat colour.
constexpr const char* kVFade =
    "attribute vec4 position; uniform vec4 vColor; varying vec4 colorVarying; "
    "void main() { gl_Position = position; colorVarying = vColor; }";
constexpr const char* kFFade =
    "precision highp float; varying vec4 colorVarying; void main() "
    "{ gl_FragColor = colorVarying; }";

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

GLuint LinkProgram(const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, Compile(GL_VERTEX_SHADER, vs));
    glAttachShader(p, Compile(GL_FRAGMENT_SHADER, fs));
    glBindAttribLocation(p, 0, "position");
    glBindAttribLocation(p, 1, "color");
    glBindAttribLocation(p, 2, "texcoord0");
    glBindAttribLocation(p, 3, "weight");
    glBindAttribLocation(p, 4, "incidence");
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        throw mcf::Error(std::format("link failed: {}", log));
    }
    return p;
}

}  // namespace

int main(int argc, char** argv) {
    lucent::config::set_prefix("MANA_");

    // PORT CHOICE, not a reversed value. A new game's starting room is set by
    // the save/new-game path, which is not reversed: GameParameter::Init grants
    // the starting equipment but no map, and MapJump is only ever called from
    // Lua. Rooms connect by walking, so any real room is a usable entry point.
    static constexpr const char* kDefaultRoom = "M0000_00_00";
    std::string archive = "scratch/raw/assets/sk1/sk1.mpk";
    std::string model = "B0000_00";
    std::string shot, anim;
    float anim_t = 0.f;
    bool census = false;
    bool room_census = false;
    std::string room, render_room;
    std::string bgm_dir = "scratch/raw/assets";
    bool audio_selftest = false;
    std::string probe;
    float spawn_x = 0, spawn_z = 0;
    bool has_spawn = false;
    bool fade_test = false;
    bool combat_selftest = false;
    bool auto_attack = false;
    int warmup = 0;
    bool fixed_step = false;
    bool combat_demo = false;
    bool explicit_model = false;
    bool walk_to = false;
    float walk_x = 0, walk_z = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--archive" && i + 1 < argc) archive = argv[++i];
        else if (a == "--model" && i + 1 < argc) { model = argv[++i]; explicit_model = true; }
        else if (a == "--screenshot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--anim" && i + 1 < argc) anim = argv[++i];
        else if (a == "--time" && i + 1 < argc) anim_t = std::stof(argv[++i]);
        else if (a == "--script-census") census = true;
        else if (a == "--room-census") room_census = true;
        else if (a == "--run-room" && i + 1 < argc) room = argv[++i];
        else if (a == "--render-room" && i + 1 < argc) render_room = argv[++i];
        else if (a == "--bgm-dir" && i + 1 < argc) bgm_dir = argv[++i];
        else if (a == "--audio-selftest") audio_selftest = true;
        else if (a == "--combat-selftest") combat_selftest = true;
        else if (a == "--auto-attack") auto_attack = true;
        // --warmup implies --fixed-step: see the loop, a frame count on an
        // uncapped loop is not a duration.
        else if (a == "--warmup" && i + 1 < argc) { warmup = std::atoi(argv[++i]); fixed_step = true; }
        else if (a == "--fixed-step") fixed_step = true;
        else if (a == "--combat-demo") combat_demo = true;
        else if (a == "--collision-probe" && i + 1 < argc) probe = argv[++i];
        else if (a == "--fade-test") fade_test = true;
        else if (a == "--spawn" && i + 2 < argc) {
            spawn_x = std::stof(argv[++i]); spawn_z = std::stof(argv[++i]);
            has_spawn = true;
        }
        else if (a == "--room" && i + 1 < argc) render_room = argv[++i];
        else if (a == "--walk-to" && i + 2 < argc) {
            walk_x = std::stof(argv[++i]); walk_z = std::stof(argv[++i]);
            walk_to = true;
        }
        else if (a == "--help" || a == "-h") {
            std::printf(
                "usage: %s [options]\n"
                "\nWith no options, plays from the default start room.\n"
                "\nPlaying:\n"
                "  --room NAME         start in this room (default %s)\n"
                "  --archive PATH      sk1.mpk (default %s)\n"
                "  --bgm-dir PATH      directory holding bgmNNN*.ogg\n"
                "  --spawn X Z         start at these room-local coordinates\n"
                "\nControls: WASD / arrows to move, Space or Z to attack, Esc to quit.\n"
                "\nTools:\n"
                "  --model NAME [--anim FILE] [--time T]   view one model\n"
                "  --screenshot OUT.png [--warmup N]       render N frames, save, exit\n"
                "  --fixed-step        step at a fixed 30 Hz (implied by --warmup)\n"
                "  --collision-probe ROOM                  walk outward, report walls\n"
                "  --script-census     run every shipping script and tally cmd calls\n"
                "  --room-census       load every room headlessly, report what is missing\n"
                "  --combat-selftest / --audio-selftest    self-tests, non-zero on failure\n"
                "  --auto-attack       swing continuously (headless combat driver)\n"
                "  --walk-to X Z       walk toward a room-local point (headless)\n",
                argv[0], kDefaultRoom, archive.c_str());
            return 0;
        }
        else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option '%s' (try --help)\n", a.c_str());
            return 2;
        }
    }

    // Playing the game is the default. Without this the bare binary dropped
    // into the model viewer and span forever, which is not a game. Any explicit
    // mode wins; `explicit_model` is tracked separately because `model` carries
    // a default value and so cannot be tested for emptiness.
    if (render_room.empty() && room.empty() && probe.empty() && !census &&
        !audio_selftest && !combat_selftest && !explicit_model && !room_census)
        render_room = kDefaultRoom;

    try {
        mcf::Archive ar(archive);
        lucent::info("assets", "opened {} ({} entries)", archive, ar.entries().size());

        if (!probe.empty()) {
            // Walk outward from the room centre in 8 directions and report how
            // far before a wall or a floor edge stops us. A wall system that
            // blocks nothing looks identical to one that is never called, so
            // this prints distances rather than a pass/fail vibe.
            auto cs = std::format("sk1/{}.scol", probe);
            if (!ar.Has(cs)) throw mcf::Error(std::format("no {}", cs));
            auto col = mcf::ParseScol(ar.Read(cs));
            float cx = (col.aabb_lo[0] + col.aabb_hi[0]) * .5f;
            float cz = (col.aabb_lo[2] + col.aabb_hi[2]) * .5f;
            float cy = 0;
            col.GetFloor(cx, cz, mcf::Collision::kFloorMask, &cy);
            lucent::info("probe", "{}: AABB ({:.0f},{:.0f})..({:.0f},{:.0f}), "
                         "centre floor y={:.1f}", probe, col.aabb_lo[0], col.aabb_lo[2],
                         col.aabb_hi[0], col.aabb_hi[2], cy);
            const char* names[8] = {"+X", "+X+Z", "+Z", "-X+Z", "-X", "-X-Z", "-Z", "+X-Z"};
            for (int d = 0; d < 8; ++d) {
                float ang = float(d) * 3.14159265f / 4.f;
                float dx = std::cos(ang), dz = std::sin(ang);
                float x = cx, z = cz, y = cy, dist = 0;
                const char* why = "reached 400";
                for (int step = 0; step < 400; ++step) {
                    float nx = x + dx, nz = z + dz, g;
                    if (col.BlockedXZ(x, z, nx, nz, y, 30.f, mcf::Collision::kWallMask)) {
                        why = "wall"; break;
                    }
                    if (!col.GetFloor(nx, nz, mcf::Collision::kFloorMask, &g)) {
                        why = "no floor"; break;
                    }
                    x = nx; z = nz; y = g; dist += 1;
                }
                lucent::info("probe", "  {:<5} stopped at {:5.0f} units ({})",
                             names[d], dist, why);
            }
            return 0;
        }

        if (combat_selftest) {
            // The detector must be run against BOTH classes. A hit test that
            // never fires and one that always fires look identical in a game
            // where nothing happens to overlap.
            struct Case { const char* name; float a[3]; float ar, arc, yaw; float d[3]; float dr; bool want; };
            const Case cases[] = {
                {"touching spheres, full circle",   {0,0,0}, 35, 360, 0, {0,0,40}, 15, true},
                {"just out of reach",               {0,0,0}, 35, 360, 0, {0,0,51}, 15, false},
                {"exactly at combined radius",      {0,0,0}, 35, 360, 0, {0,0,50}, 15, true},
                {"180 arc, target in front",        {0,0,0}, 35, 180, 0, {0,0,40}, 15, true},
                {"180 arc, target behind",          {0,0,0}, 35, 180, 0, {0,0,-40},15, false},
                {"180 arc, target to the side",     {0,0,0}, 35, 180, 0, {40,0,0}, 15, true},
                {"60 arc, target to the side",      {0,0,0}, 35,  60, 0, {40,0,0}, 15, false},
                {"180 arc rotated to face behind",  {0,0,0}, 35, 180, 3.14159f, {0,0,-40}, 15, true},
                {"vertical separation beyond reach",{0,0,0}, 35, 360, 0, {0,80,0}, 15, false},
            };
            int bad = 0;
            for (const auto& c : cases) {
                bool got = mcf::HitArcSphere(c.a, c.ar, c.arc, c.yaw, c.d, c.dr);
                if (got != c.want) {
                    lucent::error("combat", "SELFTEST FAIL: {} -> {} (want {})",
                                  c.name, got, c.want);
                    ++bad;
                } else {
                    lucent::info("combat", "  ok: {:<34} -> {}", c.name, got);
                }
            }
            lucent::info("combat", "SELFTEST: {} cases, {} failures", 9, bad);
            return bad ? 1 : 0;
        }

        if (audio_selftest) {
            // A decoder that plays nothing is indistinguishable from one that
            // was never called, so this feeds cases that MUST decode and fails
            // loudly if they do not.
            mcf::Audio au;
            if (!au.Init()) { lucent::error("audio", "no device; cannot self-test"); return 1; }
            int bad = 0;
            for (int id : {1, 2, 88, 176}) {
                auto nm = std::format("sk1/SE{:04d}.wav", id);
                if (!ar.Has(nm) || !au.PlaySe(id, ar.Read(nm), false)) {
                    lucent::error("audio", "SELFTEST FAIL: {} did not decode", nm);
                    ++bad;
                }
            }
            for (int id : {1, 30, 101, 130}) {
                std::string path;
                for (const auto& e : std::filesystem::directory_iterator(bgm_dir)) {
                    auto f = e.path().filename().string();
                    if (f.rfind(std::format("bgm{:03d}", id), 0) == 0) { path = e.path().string(); break; }
                }
                if (path.empty() || !au.PlayBgm(id, path, false)) {
                    lucent::error("audio", "SELFTEST FAIL: bgm {} did not decode", id);
                    ++bad;
                }
            }
            lucent::info("audio", "SELFTEST: {} sounds, {} PCM frames, {} failures",
                         au.stat.decoded_sounds, au.stat.decoded_frames, bad);
            if (au.stat.decoded_frames == 0) {
                lucent::error("audio", "SELFTEST FAIL: zero PCM decoded overall");
                return 1;
            }
            return bad ? 1 : 0;
        }

        if (!room.empty()) {
            // Run one room's script against a live actor system and report what
            // it populated. This is the smallest end-to-end proof that scripts
            // are driving engine state rather than just executing.
            mcf::World world;
            mcf::Audio audio;
            audio.Init();          // logs and disables itself if there is no device
            mcf::Script sc;
            sc.world = &world;
            sc.audio = &audio;
            if (!sc.Run("sk1.lua", ar.Read("sk1/sk1.lua")))
                throw mcf::Error(std::format("prelude: {}", sc.last_error()));
            if (!sc.CallFunction("SystemInit"))
                throw mcf::Error(std::format("SystemInit: {}", sc.last_error()));
            // Snapshot BEFORE the room script so its own handlers are the diff.
            auto before = sc.Globals();
            auto script = std::format("sk1/{}.lua", room);
            if (!sc.Run(script, ar.Read(script)))
                throw mcf::Error(std::format("{}: {}", script, sc.last_error()));
            lucent::info("lua", "ran {}", script);
            size_t fired = 0, errs = 0;
            for (const auto& g : sc.Globals()) {
                if (std::binary_search(before.begin(), before.end(), g)) continue;
                ++fired;
                if (!sc.CallFunction(g)) ++errs;
            }
            lucent::info("lua", "fired {} handlers ({} errored)", fired, errs);
            const auto& cm = world.camera;
            lucent::info("world", "camera: angle={:g} distance={:g} rotY={:g} speed={:g} "
                         "target='{}' ({} slots set)",
                         cm.Get(mcf::cam_data::kAngle, 20.f),
                         cm.Get(mcf::cam_data::kDistance, 450.f),
                         cm.Get(mcf::cam_data::kRotateY, 0.f),
                         cm.Get(mcf::cam_data::kSpeed, 0.3f),
                         cm.target_chr, cm.data.size());
            for (const auto& b : world.boxes)
                lucent::info("world", "  box '{}' ({:.0f},{:.0f},{:.0f})..({:.0f},{:.0f},{:.0f})",
                             b.name, b.lo[0], b.lo[1], b.lo[2], b.hi[0], b.hi[1], b.hi[2]);
            lucent::info("world", "--- {} actors ---", world.actors().size());
            for (const auto& a : world.actors()) {
                std::string slots;
                for (const auto& [k, v] : a.data)
                    slots += std::format(" [{}]={:g}", k, v);
                lucent::info("world", "  {:<16} id={:<4} pos=({:.1f},{:.1f},{:.1f}) "
                             "motion={} alive={}{}", a.handle, a.type_id, a.pos[0],
                             a.pos[1], a.pos[2], a.motion, a.alive, slots);
            }
            return 0;
        }

        if (room_census) {
            // Coverage over the WHOLE game, not the handful of rooms used for
            // development. Loads every room the way the real path does -- mesh,
            // collision, .odt objects, and the room script against a live actor
            // system -- and reports what is missing, with denominators.
            //
            // No GL here: models are parsed, not uploaded, so this runs
            // headless and fast. That means it cannot catch upload/render
            // faults, which is stated rather than left implied.
            std::vector<std::string> rooms;
            for (const auto& e : ar.entries()) {
                if (e.name.size() < 10) continue;
                if (e.name.compare(0, 5, "sk1/M") != 0) continue;
                if (e.name.compare(e.name.size() - 5, 5, ".smdl") != 0) continue;
                rooms.push_back(e.name.substr(4, e.name.size() - 9));
            }
            std::sort(rooms.begin(), rooms.end());
            if (rooms.empty()) {
                lucent::error("census", "no sk1/M*.smdl in the archive -- scanned "
                              "NOTHING, which is not a pass");
                return 2;
            }

            mcf::World w;
            mcf::Script sc2;
            sc2.world = &w;
            if (!sc2.Run("sk1.lua", ar.Read("sk1/sk1.lua")))
                throw mcf::Error(std::format("prelude: {}", sc2.last_error()));
            if (!sc2.CallFunction("SystemInit"))
                throw mcf::Error(std::format("SystemInit: {}", sc2.last_error()));

            struct { long rooms = 0, mesh_fail = 0, no_col = 0, no_script = 0,
                          script_fail = 0, actors = 0, boxes = 0, objects = 0,
                          obj_unknown = 0, actor_no_model = 0, with_odt = 0,
                          invisible = 0; } c;
            std::map<std::string, int> missing_models;
            for (const auto& r : rooms) {
                ++c.rooms;
                try { mcf::ParseSmdl(ar.Read(std::format("sk1/{}.smdl", r))); }
                catch (const std::exception&) { ++c.mesh_fail; continue; }

                auto cs = std::format("sk1/{}.scol", r);
                if (!ar.Has(cs)) ++c.no_col;

                w.Reset();
                auto sp = std::format("sk1/{}.lua", r);
                if (!ar.Has(sp)) ++c.no_script;
                else if (!sc2.Run(sp, ar.Read(sp))) ++c.script_fail;

                c.actors += long(w.actors().size());
                c.boxes += long(w.boxes.size());
                for (const auto& a : w.actors()) {
                    auto nm = mcf::ActorModelName(a.kind, a.type_id);
                    if (nm.empty()) { ++c.invisible; continue; }   // eNPC.TRANS
                    if (!ar.Has(std::format("sk1/{}.smdl", nm))) {
                        ++c.actor_no_model;
                        missing_models[nm]++;
                    }
                }

                auto op = std::format("sk1/{}.odt", r);
                if (ar.Has(op)) {
                    ++c.with_odt;
                    for (const auto& o : mcf::ParseOdt(ar.Read(op))) {
                        ++c.objects;
                        if (!mcf::MapObjectModel(o.id)) ++c.obj_unknown;
                    }
                }
            }
            lucent::info("census", "{} rooms: {} mesh parse failures, {} without "
                         "collision, {} without a script, {} script failures",
                         c.rooms, c.mesh_fail, c.no_col, c.no_script, c.script_fail);
            lucent::info("census", "  {} actors spawned, {} with no model ({} distinct), "
                         "{} intentionally invisible (eNPC.TRANS)",
                         c.actors, c.actor_no_model, missing_models.size(), c.invisible);
            lucent::info("census", "  {} event boxes", c.boxes);
            lucent::info("census", "  {} rooms have an .odt; {} objects, {} unresolved ids",
                         c.with_odt, c.objects, c.obj_unknown);
            for (const auto& [nm, n] : missing_models)
                lucent::info("census", "    missing model {} x{}", nm, n);
            return (c.mesh_fail || c.script_fail || c.obj_unknown) ? 1 : 0;
        }

        if (census) {
            // Run every shipping script against the recording bindings. This
            // measures which of the 200 cmd functions the game actually
            // exercises, so engine work can be ordered by evidence.
            std::vector<std::string> scripts;
            for (const auto& e : ar.entries())
                if (e.name.size() > 4 && e.name.compare(e.name.size() - 4, 4, ".lua") == 0)
                    scripts.push_back(e.name);
            std::sort(scripts.begin(), scripts.end());
            auto prelude = ar.Read("sk1/sk1.lua");
            lucent::info("lua", "prelude sk1.lua: {} bytes; {} scripts to run",
                         prelude.size(), scripts.size());

            std::map<std::string, uint64_t> totals;
            size_t ok = 0, failed = 0, handlers = 0, handler_errs = 0;
            std::vector<std::string> errors;
            for (const auto& name : scripts) {
                if (name == "sk1/sk1.lua") continue;
                mcf::Script sc;
                if (!sc.Run("sk1.lua", prelude)) {
                    lucent::error("lua", "prelude failed: {}", sc.last_error());
                    break;
                }
                // sk1.lua's own comment marks SystemInit as startup-only; it
                // establishes the scenario globals (sccnt, scflagNN, ...) that
                // the map scripts read. Skipping it made 102 scripts fail on
                // "attempt to compare nil with number".
                if (!sc.CallFunction("SystemInit")) {
                    lucent::error("lua", "SystemInit failed: {}", sc.last_error());
                    break;
                }
                // Baseline: functions the prelude alone defines. Anything the
                // map script adds is one of ITS event handlers.
                auto before = sc.Globals();
                auto body = ar.Read(name);
                if (!sc.Run(name, body)) {
                    ++failed;
                    if (errors.size() < 12) errors.push_back(
                        std::format("{}: {}", name, sc.last_error()));
                } else {
                    ++ok;
                    // Load-time calls alone undercount badly: the interesting
                    // API surface lives in the event handlers, which only run
                    // when the player trips a box. Fire each one to see it.
                    for (const auto& g : sc.Globals()) {
                        if (std::binary_search(before.begin(), before.end(), g)) continue;
                        ++handlers;
                        if (!sc.CallFunction(g)) ++handler_errs;
                    }
                }
                for (const auto& [fn, rec] : sc.calls) totals[fn] += rec.count;
            }
            lucent::info("lua", "executed {} map scripts, {} failed", ok, failed);
            lucent::info("lua", "invoked {} event handlers, {} raised errors "
                         "(expected: many need live game state)", handlers, handler_errs);
            for (const auto& e : errors) lucent::warn("lua", "  {}", e);

            std::vector<std::pair<std::string, uint64_t>> ranked(totals.begin(), totals.end());
            std::sort(ranked.begin(), ranked.end(),
                      [](auto& a, auto& b) { return a.second > b.second; });
            lucent::info("lua", "--- cmd functions CALLED: {} of {} ---",
                         ranked.size(), mcf::Script::api_size());
            for (const auto& [fn, n] : ranked)
                std::printf("%9llu  %s\n", (unsigned long long)n, fn.c_str());
            return 0;
        }

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

        if (!render_room.empty()) {
            GLuint progFlat = LinkProgram(kVS, kFS);
            GLuint progSkin = LinkProgram(kVSkin, kFS);
            GLuint progFade = LinkProgram(kVFade, kFFade);
            GLuint fadeVbo = 0;
            glGenBuffers(1, &fadeVbo);
            {
                const float quad[] = {-1,-1, 3,-1, -1,3};   // one oversized triangle
                glBindBuffer(GL_ARRAY_BUFFER, fadeVbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
            }
            GLuint white = 0;
            glGenTextures(1, &white);
            glBindTexture(GL_TEXTURE_2D, white);
            const uint8_t kW[4] = {255, 255, 255, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, kW);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // Room-scoped state, rebuilt by loadRoom on every transition.
            mcf::Renderable stage;
            mcf::Collision col;
            bool have_col = false;
            float room_org[3]{0, 0, 0};
            struct Placed { const mcf::Renderable* r; float pos[3]; const mcf::Motion* mo; };
            std::vector<Placed> placed;
            struct PlacedObj { const mcf::Renderable* r; float pos[3]; };
            std::vector<PlacedObj> objects;
            std::map<std::string, mcf::Renderable> cache;   // survives transitions
            std::map<std::string, mcf::Motion> motions;     // survives transitions

            mcf::World world;
            mcf::Audio audio;
            audio.Init();          // logs and disables itself if there is no device
            mcf::Script sc;
            sc.world = &world;
            sc.audio = &audio;
            if (!sc.Run("sk1.lua", ar.Read("sk1/sk1.lua")))
                throw mcf::Error(std::format("prelude: {}", sc.last_error()));
            if (!sc.CallFunction("SystemInit"))
                throw mcf::Error(std::format("SystemInit: {}", sc.last_error()));

            // The game's enemy stat table, keyed by the id AddEnemy/AddBoss pass.
            std::map<int, mcf::EnemyStats> enemy_stats;
            {
                const char* kEnemyDat = "sk1/enemydat.bin";
                auto rows = ar.Has(kEnemyDat)
                                ? mcf::ParseEnemyDat(ar.Read(kEnemyDat))
                                : std::vector<mcf::EnemyStats>{};
                for (const auto& e : rows) enemy_stats[e.id] = e;
                // Say what was loaded AND what was not: silence here would look
                // identical to a table that failed to parse.
                if (rows.empty())
                    lucent::warn("combat", "{}: {} -- enemies will have no stats",
                                 kEnemyDat,
                                 ar.Has(kEnemyDat) ? "did not parse" : "not in archive");
                else
                    lucent::info("combat", "{}: {} enemy records", kEnemyDat, rows.size());
            }
            // Re-seeded on every room load, because world.Reset() drops the
            // actors a previous room's stats were attached to.
            std::function<void()> seedCombat = [] {};

            std::string room_name = render_room;
            auto loadRoom = [&](const std::string& name) -> bool {
                room_name = name;
            stage = mcf::Renderable{};
            if (!mcf::LoadRenderable(ar, room_name, white, &stage)) {
                    lucent::error("world", "no model for room {}", room_name);
                    return false;
                }

            // The Lua state persists ACROSS rooms: scenario flags (sccnt, scflagNN) are
                // globals that must survive a transition, which is why SystemInit runs
                // once at startup and not per room.
                world.Reset();
            
            auto sp = std::format("sk1/{}.lua", room_name);
            if (ar.Has(sp) && !sc.Run(sp, ar.Read(sp)))
                lucent::warn("lua", "{}: {}", sp, sc.last_error());

            // Scripts give actor positions in ROOM-LOCAL coordinates while map
            // models are authored in world space. Rooms tile a fixed
            // 300 x 240 grid indexed by the two numbers in the model name
            // (M<map>_<gx>_<gy>): for the M0000_00_* column, mesh min.z is
            // exactly gy*240 for every room. Mesh bounds and the collision AABB
            // both overhang that grid by varying amounts (walls, and a uniform
            // 15-unit collision margin giving 330x270 boxes), so the FILENAME
            // is the anchor -- not anything measured off the geometry.
            room_org[0] = room_org[1] = room_org[2] = 0.f;
            {
                auto us = room_name.rfind('_');
                auto us2 = room_name.rfind('_', us - 1);
                if (us != std::string::npos && us2 != std::string::npos) {
                    int gx = std::atoi(room_name.substr(us2 + 1, us - us2 - 1).c_str());
                    int gy = std::atoi(room_name.substr(us + 1).c_str());
                    room_org[0] = float(gx) * 300.f;
                    room_org[2] = float(gy) * 240.f;
                    lucent::info("world", "room grid ({},{}) -> origin ({:.0f},0,{:.0f})",
                                 gx, gy, room_org[0], room_org[2]);
                }
            }

            // Ground height comes from the room's collision mesh. The scripts'
            // Y argument is not a world offset -- placing actors at
            // room_origin.y + script_y put them at wall height -- so the floor
            // is queried at the actor's XZ instead.
            have_col = false;
            {
                auto cs = std::format("sk1/{}.scol", room_name);
                if (ar.Has(cs)) { col = mcf::ParseScol(ar.Read(cs)); have_col = true; }
                else lucent::warn("world", "no {}; actors keep their script Y", cs);
            }

            // Engine-chosen NPC placement. The TRIGGER is reversed: AddNPC
            // sets a flag when the script's x and z are both 0, and
            // ModeGame::AddCharacterRandomPos then scans the room's per-chip
            // attribute array for chips passing a mask (and not bit 4) and
            // picks one. A "chip" is 30 units -- confirmed independently, since
            // sk1.lua's EvBoxOneY("in_01",3,3.4,2) produces exactly the box
            // (90,102)..(120,132) this port reads back, i.e. 3*30..4*30.
            //
            // PORT CHOICE: the chip attribute array and the engine's RNG are
            // not reversed, so the actual pick is ours -- a deterministic scan
            // of chip centres for one with walkable floor, nearest the room
            // centre. What is faithful is that the engine places these, not the
            // script; parking them at the literal origin was simply wrong.
            for (auto& a : world.actors_mutable()) {
                if (!a.random_place || !have_col) continue;
                constexpr float kChip = 30.f;
                float best[2]{0, 0};
                float best_d = 1e30f;
                bool found = false;
                float cx = room_org[0] + 150.f, cz = room_org[2] + 120.f;
                for (int gz = 0; gz < 8; ++gz) {
                    for (int gx = 0; gx < 10; ++gx) {
                        float wx = room_org[0] + (float(gx) + 0.5f) * kChip;
                        float wz = room_org[2] + (float(gz) + 0.5f) * kChip;
                        float g;
                        if (!col.GetFloor(wx, wz, mcf::Collision::kFloorMask, &g)) continue;
                        float d = (wx - cx) * (wx - cx) + (wz - cz) * (wz - cz);
                        if (d < best_d) { best_d = d; best[0] = wx; best[1] = wz; found = true; }
                    }
                }
                if (found) {
                    a.pos[0] = best[0] - room_org[0];
                    a.pos[2] = best[1] - room_org[2];
                    lucent::info("world", "{}: engine-placed (script gave 0,0, extent {:.0f}) "
                                 "-> room-local ({:.0f},{:.0f})",
                                 a.handle, a.place_extent, a.pos[0], a.pos[2]);
                } else {
                    lucent::warn("world", "{}: engine-placed, but no walkable chip found "
                                 "in the 10x8 grid; leaving it at the origin", a.handle);
                }
            }

            // One renderable per distinct model, instanced per actor.
            placed.clear();

            for (const auto& a : world.actors()) {
                if (!a.alive) continue;
                auto nm = mcf::ActorModelName(a.kind, a.type_id);
                if (nm.empty()) continue;   // eNPC.TRANS: invisible by design
                if (!cache.count(nm)) {
                    mcf::Renderable r;
                    if (!mcf::LoadRenderable(ar, nm, white, &r)) {
                        lucent::warn("world", "actor {} (kind {} id {}) has no model {}",
                                     a.handle, a.kind, a.type_id, nm);
                        continue;
                    }
                    cache[nm] = std::move(r);
                }
                float wx = a.pos[0] + room_org[0], wz = a.pos[2] + room_org[2];
                float wy = a.pos[1] + room_org[1];
                if (have_col) {
                    float g = 0;
                    if (col.GetFloor(wx, wz, mcf::Collision::kFloorMask, &g)) {
                        lucent::info("world", "  {} floor at ({:.1f},{:.1f}) = {:.2f} "
                                     "(script Y was {:.1f})", a.handle, wx, wz, g, a.pos[1]);
                        wy = g;
                    } else {
                        lucent::warn("world", "  {} at ({:.1f},{:.1f}) is over no floor; "
                                     "keeping script Y {:.1f}", a.handle, wx, wz, a.pos[1]);
                    }
                }
                // Resolve this actor's motion by NUMBER. The label in the
                // filename is per-model and not canonical (137 files disagree
                // with eMOTION), so only the numeric prefix is matched.
                const mcf::Motion* mo = nullptr;
                if (!cache[nm].model.bones.empty()) {
                    auto pre = mcf::World::MotionPrefix(nm, a.motion);
                    auto file = ar.FindByPrefix(pre);
                    if (file.empty()) {
                        lucent::warn("world", "{}: no motion {} for {} (prefix {})",
                                     a.handle, a.motion, nm, pre);
                    } else {
                        if (!motions.count(file))
                            motions[file] = mcf::ParseSmot(ar.Read(file));
                        mo = &motions[file];
                    }
                }
                placed.push_back({&cache[nm], {wx, wy, wz}, mo});
            }
            // Map objects from the room's .odt. Positions there are already
            // world-space, so room_org is NOT added; the id resolves through
            // the table lifted out of ModeGame::LoadMapObject, not through the
            // number in the O####_##.smdl filename (that mapping was refuted --
            // see docs/object-table.md).
            objects.clear();
            {
                auto op = std::format("sk1/{}.odt", room_name);
                int missing_model = 0, missing_id = 0;
                auto objs = ar.Has(op) ? mcf::ParseOdt(ar.Read(op))
                                       : std::vector<mcf::MapObject>{};
                for (const auto& o : objs) {
                    const char* nm = mcf::MapObjectModel(o.id);
                    if (!nm) { ++missing_id; continue; }
                    if (!cache.count(nm)) {
                        mcf::Renderable r;
                        if (!mcf::LoadRenderable(ar, nm, white, &r)) {
                            ++missing_model;
                            continue;
                        }
                        cache[nm] = std::move(r);
                    }
                    objects.push_back({&cache[nm], {o.pos[0], o.pos[1], o.pos[2]}});
                }
                // Report the denominator: "0 objects" from a room that has no
                // .odt and from a room whose table failed to parse would
                // otherwise look identical.
                lucent::info("world",
                             "{}: {} placements -> {} drawn ({} unknown id, "
                             "{} id known but model missing)",
                             ar.Has(op) ? op : op + " (absent)",
                             objs.size(), objects.size(), missing_id, missing_model);
            }

            lucent::info("world", "{} actors, {} placed, {} distinct models",
                         world.actors().size(), placed.size(), cache.size());
            // Event boxes are how the game connects rooms, so a room that
            // registered none is worth seeing -- previously indistinguishable
            // from a room whose script never ran.
            {
                size_t live = 0;
                for (const auto& bx : world.boxes)
                    if (bx.enabled && !bx.no_touch) ++live;
                lucent::info("world", "{} event box(es), {} touchable",
                             world.boxes.size(), live);
                for (const auto& bx : world.boxes)
                    lucent::debug("world", "  box '{}' ({:.0f},{:.0f})..({:.0f},{:.0f})",
                                  bx.name, bx.lo[0], bx.lo[2], bx.hi[0], bx.hi[2]);
            }

                return true;
            };
            if (!loadRoom(render_room))
                throw mcf::Error(std::format("cannot load room {}", render_room));
            float ctr[3], radius = 0;
            for (int k = 0; k < 3; ++k) {
                ctr[k] = (stage.lo[k] + stage.hi[k]) * .5f;
                radius = std::max(radius, stage.hi[k] - stage.lo[k]);
            }
            Mat4 vp;   // rebuilt each frame from the camera slots

            glEnable(GL_DEPTH_TEST);
            glClearColor(0.10f, 0.11f, 0.14f, 1.f);
            glViewport(0, 0, W, H);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            float origin_zero[3]{0, 0, 0};
            auto drawOne = [&](const mcf::Renderable& r, const float t[3],
                               const mcf::Motion* mo, float yaw = 0.f) {
                GLuint pr = r.skinned() ? progSkin : progFlat;
                glUseProgram(pr);
                Mat4 m = Mat4::Identity();
                float cs = std::cos(yaw), sn = std::sin(yaw);
                m.m[0] = cs; m.m[2] = -sn; m.m[8] = sn; m.m[10] = cs;
                m.m[12] = t[0]; m.m[13] = t[1]; m.m[14] = t[2];
                Mat4 mvp = vp * m;
                glUniformMatrix4fv(glGetUniformLocation(pr, "mVP"), 1, GL_FALSE, mvp.m);
                glUniform1i(glGetUniformLocation(pr, "texture0"), 0);
                if (r.skinned()) {
                    std::vector<float> j;
                    mcf::BuildJointPalette(r.model, mo, anim_t, &j);
                    glUniform4fv(glGetUniformLocation(pr, "vJoint"), 80 * 3, j.data());
                }
                glActiveTexture(GL_TEXTURE0);
                glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r.ibo);
                GLsizei st = GLsizei(r.model.vertex_stride);
                const auto* pa = r.model.Find(mcf::VertexUsage::kPosition);
                const auto* ca = r.model.Find(mcf::VertexUsage::kColor);
                const auto* ta = r.model.Find(mcf::VertexUsage::kTexcoord0);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)(uintptr_t)pa->offset);
                if (ca) { glEnableVertexAttribArray(1);
                    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)(uintptr_t)ca->offset); }
                if (ta) { glEnableVertexAttribArray(2);
                    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, st, (void*)(uintptr_t)ta->offset); }
                if (r.skinned()) {
                    const auto* wa = r.model.Find(mcf::VertexUsage::kWeight);
                    const auto* ia = r.model.Find(mcf::VertexUsage::kIncidence);
                    glEnableVertexAttribArray(3);
                    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void*)(uintptr_t)wa->offset);
                    glEnableVertexAttribArray(4);
                    glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_FALSE, st, (void*)(uintptr_t)ia->offset);
                } else {
                    glDisableVertexAttribArray(3);
                    glDisableVertexAttribArray(4);
                }
                GLenum it = r.model.index_size == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
                // Two passes: opaque ranges first, then the blended ones, so a
                // shadow plane composites over the ground it lies on instead of
                // depth-fighting it. Blended geometry still tests depth but does
                // not write it, which is the standard ordering-independent
                // treatment for flat decals like these.
                auto blended = [&](size_t i) {
                    uint32_t mi = r.model.draws[i].material;
                    return mi < r.model.materials.size() && r.model.materials[mi].blend;
                };
                for (int pass = 0; pass < 2; ++pass) {
                    if (pass == 1) {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glDepthMask(GL_FALSE);
                    }
                    for (size_t i = 0; i < r.model.draws.size(); ++i) {
                        if (blended(i) != (pass == 1)) continue;
                        glBindTexture(GL_TEXTURE_2D, r.draw_tex[i]);
                        glDrawElements(GL_TRIANGLES, GLsizei(r.model.draws[i].index_count), it,
                                       (void*)(uintptr_t)r.model.draws[i].byte_offset);
                    }
                    if (pass == 1) { glDepthMask(GL_TRUE); glDisable(GL_BLEND); }
                }
            };

            // The player. Scripts address it as "MainPlayer" (_plName in
            // sk1.lua) and it uses the C0000_00 character model.
            // The player's model must also live in `cache`: the combat test
            // resolves an attacker's model through that map, so loading it only
            // into `hero` made the player invisible to its own hit test.
            bool have_hero = cache.count("C0000_00") ||
                             mcf::LoadRenderable(ar, "C0000_00", white, &cache["C0000_00"]);
            if (!have_hero) cache.erase("C0000_00");
            mcf::Renderable& hero = cache["C0000_00"];
            std::map<int, mcf::Motion> hero_motions;
            auto heroMotion = [&](int id) -> const mcf::Motion* {
                auto it = hero_motions.find(id);
                if (it != hero_motions.end()) return &it->second;
                auto f = ar.FindByPrefix(mcf::World::MotionPrefix("C0000_00", id));
                if (f.empty()) return nullptr;
                hero_motions[id] = mcf::ParseSmot(ar.Read(f));
                return &hero_motions[id];
            };
            float px = ctr[0], pz = ctr[2], py = 0, pdeg = 0;
            if (has_spawn) { px = spawn_x + room_org[0]; pz = spawn_z + room_org[2]; }
            // A spawn with no floor under it silently dropped the player to
            // y=0, i.e. under a terrain whose floor is at y=60 -- the player
            // simply did not appear, with nothing said. Say it.
            if (have_col && !col.GetFloor(px, pz, mcf::Collision::kFloorMask, &py)) {
                py = 0;
                lucent::warn("world",
                             "spawn ({:.0f},{:.0f}) has no floor under it; placing "
                             "the player at y=0, where the terrain will hide them. "
                             "Use --collision-probe {} to find walkable ground.",
                             px, pz, room_name);
            }
            world.Spawn("MainPlayer", 0, px, py, pz).kind = 'C';

            // Service whatever the room script asked for. BGM lives in the APK
            // assets, not the MPK, so it is loaded from a directory on disk.
            auto serviceAudio = [&]() {
                if (sc.stop_all_se) { audio.StopAllSe(); sc.stop_all_se = false; }
                for (int id : sc.pending_se_stop) audio.StopSe(id);
                sc.pending_se_stop.clear();
                for (auto& r : sc.pending_se) {
                    auto nm = std::format("sk1/SE{:04d}.wav", r.id);
                    if (!ar.Has(nm)) { lucent::warn("audio", "no {}", nm); continue; }
                    audio.PlaySe(r.id, ar.Read(nm), r.loop);
                }
                sc.pending_se.clear();
                if (sc.pending_bgm >= 0) {
                    int id = sc.pending_bgm;
                    sc.pending_bgm = -1;
                    std::string path;
                    for (const auto& e : std::filesystem::directory_iterator(bgm_dir)) {
                        auto f = e.path().filename().string();
                        if (f.rfind(std::format("bgm{:03d}", id), 0) == 0 &&
                            f.size() > 4 && f.compare(f.size() - 4, 4, ".ogg") == 0) {
                            path = e.path().string();
                            break;
                        }
                    }
                    if (path.empty())
                        lucent::warn("audio", "no bgm{:03d}*.ogg under {}", id, bgm_dir);
                    else if (audio.PlayBgm(id, path, true))
                        sc.current_bgm = id;
                }
            };
            serviceAudio();

            if (fade_test) {   // half-covered black fade, to prove the overlay draws
                world.fade.kind = 1;
                world.fade.duration_ms = 1000;
                world.fade.remaining_ms = 500;
                world.fade.colour[0] = world.fade.colour[1] = world.fade.colour[2] = 0;
            }
            // Give the player a sword arc and every enemy a body sphere, matching
            // how the scripts configure them (attack 35@180 on a bone; damage 15
            // on y_ang). Scripts normally do this from handlers that need combat
            // state we do not have yet, so the host seeds it.
            constexpr int kMotionWait = 0, kMotionWalk = 1, kMotionAttack = 23;
            constexpr float kAttackFrames = 24.f;   // ~0.8 s at 30 fps
            float attack_left = 0.f;
            // The player's attack comes from the game's own weapon table.
            // What is NOT modelled: which weapon is equipped (no inventory or
            // save system, so this is the id a new game starts with) and any
            // level-up bonus (tblLevelup is a 4-entry growth cycle, not
            // decoded). So the NUMBER is real; the SELECTION is an assumption.
            int player_attack = 0;
            if (const auto* w = mcf::FindWeapon(mcf::kStartingWeaponId)) {
                player_attack = w->atk_hi;
                lucent::info("combat", "player weapon {}: attack {}-{}, using {}",
                             mcf::kStartingWeaponId, w->atk_lo, w->atk_hi, player_attack);
            } else {
                lucent::warn("combat", "weapon {} not in tblWeapon; no damage "
                             "will be applied", mcf::kStartingWeaponId);
            }
            seedCombat = [&] {
                if (auto* pl = world.Find("MainPlayer")) {
                    auto& av = pl->attack[0];
                    av.bone = "cog"; av.radius = 45.f; av.arc_deg = 180.f;
                    av.valid = false;
                }
                int with_stats = 0, without = 0;
                for (auto& a : world.actors_mutable()) {
                    if (a.kind != 'E' && a.kind != 'B') continue;
                    auto& dv = a.damage[0];
                    dv.bone = "y_ang"; dv.radius = 15.f; dv.valid = true;
                    auto it = enemy_stats.find(a.type_id);
                    if (it == enemy_stats.end()) { ++without; continue; }
                    a.max_hp = it->second.max_hp;
                    a.hp = a.max_hp;
                    a.defence = it->second.defence;
                    a.exp = it->second.exp;
                    a.money = it->second.money;
                    ++with_stats;
                }
                if (with_stats || without)
                    lucent::info("combat", "enemy stats: {} from enemydat.bin, "
                                 "{} with no table entry", with_stats, without);
            };
            seedCombat();

            float eye_cur[3]{};
            bool cam_init = false;
            bool running = true;
            uint64_t prev = SDL_GetTicks();
            float t = anim_t;
            int frames = 0;
            const float kWalk = 60.f;   // units/sec; rooms are 300x240
            // A silent combat loop is ambiguous: nothing in range, or the test
            // never ran at all. These make the negative say which.
            struct CombatStats {
                long swing_frames = 0;   // frames an attack volume was live
                long pairs = 0;          // attack/damage volume pairs tested
                long hits = 0;           // per-frame overlaps
                long landed = 0;         // hits that counted (one per swing/target)
                long kills = 0;
                long atk_no_model = 0, def_no_model = 0, def_no_bone = 0;
                float closest = 1e30f;   // nearest volume separation seen
            } cs;
            while (running) {
                uint64_t now = SDL_GetTicks();
                float dt = float(now - prev) / 1000.f;
                prev = now;
                if (dt > 0.1f) dt = 0.1f;
                // A frame COUNT is only a duration if the step is fixed. The
                // loop is uncapped, so --warmup 400 on this machine was ~0.5 s,
                // not the 13 s the number suggests -- enemies closing at 30
                // units/s covered 15 of the 98 they start at and combat looked
                // dead. Headless runs therefore step at a fixed 30 Hz, which
                // also makes them reproducible across machines.
                if (fixed_step) dt = 1.f / 30.f;

                SDL_Event ev;
                while (SDL_PollEvent(&ev)) {
                    if (ev.type == SDL_EVENT_QUIT) running = false;
                    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE)
                        running = false;
                }
                int nk = 0;
                const bool* key = SDL_GetKeyboardState(&nk);
                float mx = 0, mz = 0;
                if (key) {
                    if (key[SDL_SCANCODE_LEFT]  || key[SDL_SCANCODE_A]) mx -= 1;
                    if (key[SDL_SCANCODE_RIGHT] || key[SDL_SCANCODE_D]) mx += 1;
                    if (key[SDL_SCANCODE_UP]    || key[SDL_SCANCODE_W]) mz -= 1;
                    if (key[SDL_SCANCODE_DOWN]  || key[SDL_SCANCODE_S]) mz += 1;
                }
                // Headless driver: steer toward a room-local target so the
                // walk-into-an-event-box path (which is how the game connects
                // rooms) can be exercised without a human at the keyboard.
                if (walk_to) {
                    float tx = walk_x + room_org[0], tz = walk_z + room_org[2];
                    float dx = tx - px, dz = tz - pz;
                    if (dx * dx + dz * dz > 4.f) { mx = dx; mz = dz; }
                }
                bool attacking = attack_left > 0.f;
                if (combat_demo && !attacking && frames > 30) {
                    attack_left = kAttackFrames;   // swing continuously, for testing
                    attacking = true;
                }
                if (auto_attack && !attacking) { attack_left = kAttackFrames; attacking = true; }
                if (key && (key[SDL_SCANCODE_SPACE] || key[SDL_SCANCODE_Z]) && !attacking) {
                    attack_left = kAttackFrames;
                    attacking = true;
                }
                bool moving = (mx != 0 || mz != 0) && !attacking;   // no moving mid-swing
                if (moving) {
                    float ox = px, oz = pz;
                    float len = std::sqrt(mx * mx + mz * mz);
                    px += mx / len * kWalk * dt;
                    pz += mz / len * kWalk * dt;
                    pdeg = std::atan2(mx, mz);
                    // Refuse to walk off the collision mesh rather than
                    // silently floating: revert the step if there is no floor.
                    float g;
                    bool blocked = have_col &&
                        (col.BlockedXZ(ox, oz, px, pz, py, 30.f,
                                       mcf::Collision::kWallMask) ||
                         !col.GetFloor(px, pz, mcf::Collision::kFloorMask, &g));
                    if (blocked) { px = ox; pz = oz; }
                    else if (have_col) py = g;
                }
                // Camera from the game's own slots. Defaults are sk1.lua's where
                // it states them (NEAR 40, FAR 5000, SPEED 0.3) and the values
                // the scripts most often set otherwise (ANGLE 20, DISTANCE 450).
                const auto& cam = world.camera;
                float look[3]{px, py + 20.f, pz};
                if (!cam.target_chr.empty())
                    if (const auto* ta = world.Find(cam.target_chr))
                        { look[0] = ta->pos[0] + room_org[0];
                          look[1] = ta->pos[1] + 20.f;
                          look[2] = ta->pos[2] + room_org[2]; }
                float fov  = cam.Get(mcf::cam_data::kAngle, 20.f);
                float dist = cam.Get(mcf::cam_data::kDistance, 450.f);
                float yaw  = cam.Get(mcf::cam_data::kRotateY, 0.f);
                float pit  = cam.Get(mcf::cam_data::kRotateX, cam.pitch_default);
                float zn   = cam.Get(mcf::cam_data::kNear, 40.f);
                float zf   = cam.Get(mcf::cam_data::kFar, 5000.f);
                float speed = cam.Get(mcf::cam_data::kSpeed, 0.3f);

                const float kDeg = float(std::numbers::pi) / 180.f;
                float want[3]{
                    look[0] + std::sin(yaw * kDeg) * std::cos(pit * kDeg) * dist,
                    look[1] + std::sin(pit * kDeg) * dist,
                    look[2] + std::cos(yaw * kDeg) * std::cos(pit * kDeg) * dist};
                if (!cam_init) { for (int k = 0; k < 3; ++k) eye_cur[k] = want[k]; cam_init = true; }
                // SPEED is a per-frame lerp in the original; scale by dt so the
                // result does not depend on our (uncapped) frame rate.
                float a = 1.f - std::pow(1.f - std::min(speed, 0.99f), dt * 30.f);
                for (int k = 0; k < 3; ++k) eye_cur[k] += (want[k] - eye_cur[k]) * a;
                float up[3]{0, 1, 0};
                vp = Perspective(fov * 2.f * kDeg, float(W) / H, zn, zf) *
                     LookAt(eye_cur, look, up);

                // Drive the player's attack volume from the swing, and keep the
                // world actor in sync so the shared hit test sees it.
                if (attack_left > 0.f) attack_left -= dt * 30.f;
                if (auto* pl = world.Find("MainPlayer")) {
                    pl->pos[0] = px - room_org[0];
                    pl->pos[1] = py - room_org[1];
                    pl->pos[2] = pz - room_org[2];
                    pl->rot_y = pdeg;
                    pl->motion = attacking ? kMotionAttack : (moving ? kMotionWalk : kMotionWait);
                    // Only the middle of the swing connects, so a held key does
                    // not produce a continuous damage beam.
                    auto it = pl->attack.find(0);
                    if (it != pl->attack.end()) {
                        bool was = it->second.valid;
                        it->second.valid = attack_left > kAttackFrames * 0.25f &&
                                           attack_left < kAttackFrames * 0.75f;
                        // A new swing starts the moment the volume goes live.
                        if (it->second.valid && !was) ++pl->swing_id;
                    }
                }

                // Enemies close on the player. Deliberately minimal -- the real
                // AI lives in native code that is not reversed.
                for (auto& a : world.actors_mutable()) {
                    if (!a.alive || (a.kind != 'E' && a.kind != 'B')) continue;
                    float ax = a.pos[0] + room_org[0], az = a.pos[2] + room_org[2];
                    float dx = px - ax, dz = pz - az;
                    float d2 = dx * dx + dz * dz;
                    if (d2 < 1.f || d2 > 200.f * 200.f) continue;
                    float d = std::sqrt(d2);
                    if (d < 40.f) { a.motion = kMotionWait; continue; }
                    float step = 30.f * dt;
                    a.pos[0] += dx / d * step;
                    a.pos[2] += dz / d * step;
                    a.rot_y = std::atan2(dx, dz);
                    a.motion = kMotionWalk;
                }

                t += dt * 30.f;      // motions are keyed in frames at 30fps
                if (!fade_test) world.fade.Tick(dt * 1000.f);
                sc.ResumeCoroutines();
                serviceAudio();
                audio.Update();

                // Event boxes are edge-triggered: entering fires the handler
                // once. Firing every frame would re-enter the same transition
                // forever.
                for (auto& bx : world.boxes) {
                    bool in = px >= bx.lo[0] + room_org[0] && px <= bx.hi[0] + room_org[0] &&
                              pz >= bx.lo[2] + room_org[2] && pz <= bx.hi[2] + room_org[2];
                    if (in && !bx.inside && bx.enabled && !bx.no_touch) {
                        lucent::info("world", "entered event box '{}'", bx.name);
                        if (!sc.StartCoroutine(bx.name))
                            lucent::warn("lua", "{}: {}", bx.name, sc.last_error());
                    }
                    bx.inside = in;
                }

                // Combat: every valid attack volume tested against every valid
                // damage volume on a different actor. Attack volumes are arcs
                // (radius + degrees) and damage volumes are spheres, per how the
                // scripts configure them.
                //
                // Indices, not references: a hit mutates the defender's HP, and
                // that cannot be done through the const actor list.
                {
                    auto& acts = world.actors_mutable();
                    for (size_t aidx = 0; aidx < acts.size(); ++aidx) {
                        if (!acts[aidx].alive || acts[aidx].attack.empty()) continue;
                        auto an = mcf::ActorModelName(acts[aidx].kind, acts[aidx].type_id);
                        auto ait = cache.find(an);
                        if (ait == cache.end()) { ++cs.atk_no_model; continue; }
                        for (const auto& [ai, av] : acts[aidx].attack) {
                            if (!av.valid || av.bone.empty()) continue;
                            ++cs.swing_frames;
                            float ap[3];
                            if (!mcf::BoneLocalPos(ait->second.model, nullptr, t, av.bone, ap))
                                continue;
                            for (int k = 0; k < 3; ++k)
                                ap[k] += acts[aidx].pos[k] + room_org[k] + av.offset[k];

                            for (size_t didx = 0; didx < acts.size(); ++didx) {
                                if (didx == aidx || !acts[didx].alive ||
                                    acts[didx].damage.empty()) continue;
                                auto dn = mcf::ActorModelName(acts[didx].kind,
                                                              acts[didx].type_id);
                                auto dit = cache.find(dn);
                                if (dit == cache.end()) { ++cs.def_no_model; continue; }
                                for (const auto& [di, dv] : acts[didx].damage) {
                                    if (!dv.valid || dv.bone.empty()) continue;
                                    float dp[3];
                                    if (!mcf::BoneLocalPos(dit->second.model, nullptr, t,
                                                           dv.bone, dp))
                                        { ++cs.def_no_bone; continue; }
                                    for (int k = 0; k < 3; ++k)
                                        dp[k] += acts[didx].pos[k] + room_org[k] + dv.offset[k];
                                    ++cs.pairs;
                                    float sx = dp[0] - ap[0], sy = dp[1] - ap[1],
                                          sz = dp[2] - ap[2];
                                    cs.closest = std::min(cs.closest,
                                        std::sqrt(sx * sx + sy * sy + sz * sz));
                                    if (!mcf::HitArcSphere(ap, av.radius, av.arc_deg,
                                                           acts[aidx].rot_y, dp, dv.radius))
                                        continue;
                                    ++cs.hits;

                                    // One hit per swing per target. The volume is
                                    // live for several frames, and without this a
                                    // single swing applied its damage every frame.
                                    auto& atkA = acts[aidx];
                                    auto key = std::make_pair(atkA.swing_id,
                                                              acts[didx].handle);
                                    if (std::find(atkA.hit_this_swing.begin(),
                                                  atkA.hit_this_swing.end(), key) !=
                                        atkA.hit_this_swing.end())
                                        continue;
                                    atkA.hit_this_swing.push_back(key);
                                    ++cs.landed;

                                    auto& d = acts[didx];
                                    if (d.max_hp <= 0) {
                                        // No table entry: log the contact but do
                                        // NOT invent a health pool for it.
                                        lucent::info("combat",
                                            "{} hits {} (no stats; no damage applied)",
                                            atkA.handle, d.handle);
                                        continue;
                                    }
                                    // The engine's formula: attack - defence.
                                    // AppCharacterEnemy::Damage does
                                    // `sub w22, w28, w27` with w27 read from the
                                    // record's +0x0C. Floored at 1 so a tough
                                    // enemy is slow, not immortal.
                                    int dmg = std::max(1, player_attack - d.defence);
                                    d.hp -= dmg;
                                    if (d.hp > 0) {
                                        lucent::info("combat",
                                            "{} hits {} for {} ({} - {} def) -> {}/{} HP",
                                            atkA.handle, d.handle, dmg,
                                            player_attack, d.defence, d.hp, d.max_hp);
                                    } else {
                                        d.hp = 0;
                                        d.alive = false;
                                        ++cs.kills;
                                        lucent::info("combat",
                                            "{} killed {} (+{} EXP, +{} money)",
                                            atkA.handle, d.handle, d.exp, d.money);
                                    }
                                }
                            }
                        }
                    }
                }

                if (sc.has_jump) {
                    sc.has_jump = false;
                    auto& j = sc.jump;
                    auto dest = std::format("M{:04d}_{:02d}_{:02d}", j.map, j.gx, j.gy);
                    lucent::info("world", "mapjump -> {} at ({:.0f},{:.0f},{:.0f}) arrow {}",
                                 dest, j.x, j.y, j.z, j.arrow);
                    if (loadRoom(dest)) {
                        seedCombat();
                        px = j.x + room_org[0];
                        pz = j.z + room_org[2];
                        py = j.y + room_org[1];
                        if (have_col) {
                            float g;
                            if (col.GetFloor(px, pz, mcf::Collision::kFloorMask, &g)) py = g;
                        }
                        // eArrow: UP=0 RI=1 DN=2 LF=3.
                        pdeg = float(j.arrow) * (float(std::numbers::pi) / 2.f);
                        cam_init = false;
                        serviceAudio();
                    } else {
                        lucent::error("world", "mapjump to {} failed; staying put", dest);
                    }
                }

                glViewport(0, 0, W, H);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                anim_t = t;
                drawOne(stage, origin_zero, nullptr);
                for (const auto& o : objects) drawOne(*o.r, o.pos, nullptr);
                // Draw from LIVE actor state: `placed` was a load-time snapshot,
                // so enemies that move would have rendered at their spawn point.
                for (const auto& a : world.actors()) {
                    if (!a.alive || a.handle == "MainPlayer") continue;
                    auto nm = mcf::ActorModelName(a.kind, a.type_id);
                    if (nm.empty()) continue;   // eNPC.TRANS: invisible by design
                    auto it = cache.find(nm);
                    if (it == cache.end()) continue;
                    const mcf::Motion* mo = nullptr;
                    if (!it->second.model.bones.empty()) {
                        auto f = ar.FindByPrefix(mcf::World::MotionPrefix(nm, a.motion));
                        if (!f.empty()) {
                            auto mit = motions.find(f);
                            if (mit == motions.end()) mit = motions.emplace(f, mcf::ParseSmot(ar.Read(f))).first;
                            mo = &mit->second;
                        }
                    }
                    float wp[3]{a.pos[0] + room_org[0], a.pos[1] + room_org[1],
                                a.pos[2] + room_org[2]};
                    drawOne(it->second, wp, mo, a.rot_y);
                }
                if (have_hero) {
                    float hp[3]{px, py, pz};
                    drawOne(hero, hp, heroMotion(moving ? 1 : 0), pdeg);
                }
                ++frames;

                // Fade overlay, drawn last so it covers everything.
                if (world.fade.Coverage() > 0.001f) {
                    glUseProgram(progFade);
                    glDisable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    const auto& fc = world.fade.colour;
                    glUniform4f(glGetUniformLocation(progFade, "vColor"),
                                fc[0] / 255.f, fc[1] / 255.f, fc[2] / 255.f,
                                world.fade.Coverage());
                    glBindBuffer(GL_ARRAY_BUFFER, fadeVbo);
                    glEnableVertexAttribArray(0);
                    glDisableVertexAttribArray(1);
                    glDisableVertexAttribArray(2);
                    glDisableVertexAttribArray(3);
                    glDisableVertexAttribArray(4);
                    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                    glDrawArrays(GL_TRIANGLES, 0, 3);
                    glDisable(GL_BLEND);
                    glEnable(GL_DEPTH_TEST);
                }

                // Capture BEFORE presenting: SDL_GL_SwapWindow may discard the
                // back buffer, so reading after it returns an undefined (here,
                // black) image.
                if (!shot.empty() && frames >= warmup) {
                    std::vector<uint8_t> px(size_t(W) * H * 4);
                    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                    std::vector<uint8_t> fl(px.size());
                    for (int y = 0; y < H; ++y)
                        std::memcpy(&fl[size_t(y) * W * 4],
                                    &px[size_t(H - 1 - y) * W * 4], size_t(W) * 4);
                    WritePng(shot, W, H, fl);
                    lucent::info("host", "wrote {}", shot);
                    running = false;
                }
                SDL_GL_SwapWindow(win);
            }
            lucent::info("host", "{} frames; audio decoded {} sounds / {} frames, bgm={}",
                         frames, audio.stat.decoded_sounds, audio.stat.decoded_frames,
                         audio.bgm_id());
            lucent::info("combat",
                         "{} frame-overlaps -> {} landed hits -> {} kills; "
                         "{} volume pairs over {} live-swing frames; "
                         "closest approach {} units; skipped {} attackers / {} "
                         "defenders with no loaded model, {} with no such bone",
                         cs.hits, cs.landed, cs.kills, cs.pairs, cs.swing_frames,
                         cs.pairs ? std::format("{:.1f}", cs.closest) : "n/a",
                         cs.atk_no_model, cs.def_no_model, cs.def_no_bone);
            SDL_GL_DestroyContext(ctx);
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 0;
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
