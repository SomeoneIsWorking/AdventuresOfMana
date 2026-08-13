// Desktop host: opens sk1.mpk, uploads a model + its texture, and draws it with
// the game's OWN GLES2 shaders (lifted verbatim from libmcfandroid.so .rodata).
//
// --screenshot renders a single frame and writes a PNG, so correctness can be
// checked without a display. That is the acceptance test for this stage.
#include <set>
#include <SDL3/SDL.h>
#include <GLES2/gl2.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <numbers>
#include <filesystem>
#include <random>
#include <map>
#include <string>
#include <vector>

#include <lucent/config.h>
#include <lucent/log.h>

#include "engine/script.h"
#include "engine/audio.h"
#include "engine/world.h"
#include "host/render.h"
#include "engine/mode.h"
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

// 2D overlay: screen-space quads, an 8-bit coverage atlas in the red channel,
// tinted. Used for the message window and its text.
constexpr const char* kVText =
    "attribute vec2 position; attribute vec2 texcoord; varying vec2 uv; "
    "void main() { uv = texcoord; gl_Position = vec4(position, 0.0, 1.0); }";
constexpr const char* kFText =
    "precision highp float; varying vec2 uv; uniform sampler2D tex; "
    "uniform vec4 tint; uniform float useTex; void main() { "
    "float a = mix(1.0, texture2D(tex, uv).r, useTex); "
    "gl_FragColor = vec4(tint.rgb, tint.a * a); }";

// Full RGBA, for the boot art. The text shader samples only .r, because it
// serves an 8-bit luminance font atlas; the logos are RGBA and need all four.
constexpr const char* kFSprite =
    "precision highp float; varying vec2 uv; uniform sampler2D tex; "
    "uniform vec4 tint; void main() { gl_FragColor = texture2D(tex, uv) * tint; }";

constexpr const char* kFS =
    "precision highp float; uniform sampler2D texture0; varying vec4 colorVarying; "
    "varying vec2 texcoordVarying; void main() { vec4 color = texture2D( texture0 , "
    "texcoordVarying ); gl_FragColor = colorVarying * color; }";

// The chip grid's two engine constants. CheckAddPos @ 0x2dcd20 writes
// `baseY + 10000.0f` into the height map where the probe found no floor, and
// _MakeRouteTable @ 0x2a7c5c rejects a chip whose |height| exceeds 9999. The
// port always probes with baseY = 0, as MakeRandomChrPosTbl does.
constexpr float kChipNoFloor = 10000.f;
// _MakeRouteTable's height gate: |height[chip] - height[GOAL]| >= 5 rejects.
// It is measured against the fixed goal cell, not the previous step -- the
// per-step reading was refuted; see docs/re-frontier.md.
constexpr float kChipStepLimit = 5.f;

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

    // The engine's own starting room, no longer a port choice. ModeGame's
    // constructor keeps the current cell in three int32s at ModeGame+0x9b18:
    // {col, row, world} (x26 = this + 0x9b18 @ 0x2d232c). It branches on
    // oG[0x28c0], the pending save slot, and the new-game arm @ 0x2d2b8c does
    //
    //     str w22, [x26, #0x8]     // world = 1  (w22 is `mov w22, #1` @ 0x2d2ac4,
    //     str xzr, [x26]           //             not reassigned in between)
    //                              // col = 0, row = 0 -- one 64-bit zero store
    //
    // so a new game starts at world 1, cell (0,0). The world table names that
    // cell sk1/M0001_00_00. The port previously guessed M0000_00_00, which is a
    // real room but not the one the game starts in.
    static constexpr const char* kDefaultRoom = "M0001_00_00";
    // The game's assets are not in the repo and never can be. They come from a
    // directory holding the APK's own `assets/` tree, given by $MANA_ASSETS or,
    // failing that, dropped in at scratch/raw/assets -- both, so neither a
    // shared checkout nor a one-off run needs the other set up.
    const char* assets_env = std::getenv("MANA_ASSETS");
    std::string assets = assets_env && *assets_env ? assets_env : "scratch/raw/assets";
    std::string archive = assets + "/sk1/sk1.mpk";
    std::string model = "B0000_00";
    std::string shot, anim;
    float anim_t = 0.f;
    bool census = false;
    bool room_census = false;
    std::string string_id, show_string;
    std::string room, render_room;
    std::string bgm_dir = assets;
    bool audio_selftest = false;
    std::string probe;
    float spawn_x = 0, spawn_z = 0;
    bool has_spawn = false;
    bool fade_test = false;
    bool combat_selftest = false;
    bool text_selftest = false;
    bool player_selftest = false;
    bool inventory_selftest = false;
    bool ai_selftest = false;
    bool eventbox_selftest = false;
    bool name_selftest = false;
    bool boot_chain = false;
    std::string title_phase;   // TEST HOOK: attract | menu | crawl | names
    int shot_delay = 0;        // frames to wait inside --shot-mode
    bool mode_selftest = false;
    bool png_selftest = false;
    std::string shot_mode;
    bool show_hud = true;
    int auto_levelup = -1;
    int grant_exp = 0;
    bool auto_attack = false;
    int warmup = 0;
    bool fixed_step = false;
    bool combat_demo = false;
    bool explicit_model = false;
    std::string lang = "en";
    bool auto_advance = false;
    bool auto_talk = false;
    bool force_window = false;
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
        else if (a == "--string" && i + 1 < argc) string_id = argv[++i];
        else if (a == "--show-string" && i + 1 < argc) show_string = argv[++i];
        else if (a == "--auto-advance") auto_advance = true;
        else if (a == "--auto-talk") auto_talk = true;
        else if (a == "--window") force_window = true;
        else if (a == "--run-room" && i + 1 < argc) room = argv[++i];
        else if (a == "--render-room" && i + 1 < argc) render_room = argv[++i];
        else if (a == "--bgm-dir" && i + 1 < argc) bgm_dir = argv[++i];
        else if (a == "--audio-selftest") audio_selftest = true;
        else if (a == "--combat-selftest") combat_selftest = true;
        else if (a == "--text-selftest") text_selftest = true;
        else if (a == "--player-selftest") player_selftest = true;
        else if (a == "--inventory-selftest") inventory_selftest = true;
        else if (a == "--ai-selftest") ai_selftest = true;
        else if (a == "--eventbox-selftest") eventbox_selftest = true;
        else if (a == "--nameentry-selftest") name_selftest = true;
        else if (a == "--boot") boot_chain = true;
        else if (a == "--title-phase" && i + 1 < argc) title_phase = argv[++i];
        else if (a == "--shot-delay" && i + 1 < argc) shot_delay = std::atoi(argv[++i]);
        else if (a == "--mode-selftest") mode_selftest = true;
        else if (a == "--png-selftest") png_selftest = true;
        else if (a == "--shot-mode" && i + 1 < argc) shot_mode = argv[++i];
        else if (a == "--no-hud") show_hud = false;
        else if (a == "--auto-levelup" && i + 1 < argc) auto_levelup = std::atoi(argv[++i]);
        else if (a == "--grant-exp" && i + 1 < argc) grant_exp = std::atoi(argv[++i]);
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
        else if (a == "--lang" && i + 1 < argc) lang = argv[++i];
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
                "  --archive PATH      sk1.mpk (default %s; see $MANA_ASSETS)\n"
                "  --bgm-dir PATH      directory holding bgmNNN*.ogg\n"
                "  --lang en|ja        dialogue language (default en)\n"
                "  --spawn X Z         start at these room-local coordinates\n"
                "\nControls: WASD / arrows to move, Space or Z to attack, Esc to quit.\n"
                "\nTools:\n"
                "  --model NAME [--anim FILE] [--time T]   view one model\n"
                "  --screenshot OUT.png [--warmup N]       render N frames, save, exit\n"
                "  --window            open a real window during a --screenshot run\n"
                "  --fixed-step        step at a fixed 30 Hz (implied by --warmup)\n"
                "  --collision-probe ROOM                  walk outward, report walls\n"
                "  --script-census     run every shipping script and tally cmd calls\n"
                "  --room-census       load every room headlessly, report what is missing\n"
                "  --string ID         resolve a dialogue id in every language\n"
                "  --show-string ID    open the message window on that line\n"
                "  --combat-selftest / --audio-selftest / --text-selftest / --player-selftest\n"
                "  --inventory-selftest / --ai-selftest / --eventbox-selftest / --nameentry-selftest\n"
                "  --boot              boot through the engine's real mode chain\n"
                "  --title-phase P     TEST HOOK: attract|menu|crawl|names\n"
                "  --shot-delay N      wait N frames inside --shot-mode before capturing\n"
                "                                         self-tests, non-zero on failure\n"
                "  --auto-attack       swing continuously (headless combat driver)\n"
                "  --walk-to X Z       walk toward a room-local point (headless)\n"
                "  --auto-advance      dismiss dialogue automatically (headless)\n"
                "  --auto-talk         talk to any NPC in reach (headless)\n"
                "  --no-hud            hide the status readout\n"
                "  --auto-levelup N    take regimen N (0 Warrior .. 3 Sage) headlessly\n"
                "  --grant-exp N       TEST HOOK: credit N EXP at startup\n",
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
        !audio_selftest && !combat_selftest && !text_selftest && !player_selftest && !explicit_model && !room_census &&
        string_id.empty())
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
            // The faction filter, both directions. A filter that rejects
            // everything and one that rejects nothing look identical if only
            // the rejections are tested.
            struct FCase { const char* name; const char* ah; char ak;
                           const char* dh; char dk; bool want; };
            const FCase fcases[] = {
                {"player -> enemy", "MainPlayer", 'N', "e", 'E', true},
                {"player -> boss",  "MainPlayer", 'N', "b", 'B', true},
                {"party  -> enemy", "p", 'C',          "e", 'E', true},
                {"enemy  -> player","e", 'E',  "MainPlayer", 'N', true},
                {"enemy  -> enemy", "e1", 'E',        "e2", 'E', false},
                {"boss   -> enemy", "b", 'B',          "e", 'E', false},
                {"player -> npc",   "MainPlayer", 'N', "n", 'N', false},
                {"player -> party", "MainPlayer", 'N', "p", 'C', false},
                {"npc    -> player","n", 'N',  "MainPlayer", 'N', false},
            };
            for (const auto& f : fcases) {
                mcf::Actor a, d;
                a.handle = f.ah; a.kind = f.ak;
                d.handle = f.dh; d.kind = f.dk;
                bool got = mcf::CanDamage(a, d);
                if (got != f.want) {
                    lucent::error("combat", "SELFTEST FAIL: {} -> {} (want {})",
                                  f.name, got, f.want);
                    ++bad;
                } else {
                    lucent::info("combat", "  ok: {:<18} -> {}", f.name, got);
                }
            }
            // The damage formula. Every term must be able to MOVE the result,
            // or a simpler formula would pass the same cases.
            struct DCase { const char* name; mcf::DamageInput in; int32_t want; };
            const DCase dcases[] = {
                {"plain attack - defence",   {20, 5, 0, 0.f, false, 0}, 15},
                {"magic is added",           {20, 5, 4, 0.f, false, 0}, 19},
                {"a full gauge doubles it",  {20, 5, 0, 16000.f, false, 0}, 30},
                {"half a gauge is 1.5x",     {20, 5, 0, 8000.f, false, 0}, 22},
                {"weakness quarters defence",{20, 8, 0, 0.f, true, 0}, 18},
                {"a 24% roll adds 24%",      {100, 0, 0, 0.f, false, 24}, 124},
                {"outmatched is floored at 1",{6, 15, 2, 0.f, false, 0}, 1},
                {"a huge defence still lands 1",{1, 9999, 0, 0.f, false, 24}, 1},
            };
            for (const auto& c : dcases) {
                int32_t got = mcf::ComputeDamage(c.in);
                if (got != c.want) {
                    lucent::error("combat", "SELFTEST FAIL: {} -> {} (want {})",
                                  c.name, got, c.want);
                    ++bad;
                } else {
                    lucent::info("combat", "  ok: {:<30} -> {}", c.name, got);
                }
            }
            for (auto [base, roll, want] : {std::tuple{48, 0, 48}, {48, 10, 52},
                                            {40, 5, 42}, {0, 10, 0}}) {
                int32_t got = mcf::RewardWithBonus(base, roll);
                if (got != want) {
                    lucent::error("combat", "SELFTEST FAIL: reward {} +{}% -> {} "
                                  "(want {})", base, roll, got, want);
                    ++bad;
                }
            }
            lucent::info("combat", "  ok: kill rewards carry a 0..10%% bonus");
            lucent::info("combat", "SELFTEST: {} cases, {} failures", 30, bad);
            return bad ? 1 : 0;
        }

        if (player_selftest) {
            // The formulas are derived from Update's arithmetic, so the test
            // that matters is whether they REPRODUCE Init's own constants --
            // numbers written by a different function, which the derivation
            // did not get to see.
            mcf::PlayerStats p;
            struct Case { const char* what; int32_t got, want; };
            const Case cases[] = {
                // Init writes these four literally; Update derives two of them.
                {"max HP at stamina 2 == Init's 19", p.max_hp(), 19},
                {"max MP at wisdom 2 == Init's 6",   p.max_mp(), 6},
                {"starting HP",                       p.hp, 19},
                {"starting money",                    p.money, 50},
                // The equipment Init grants, and what it is worth.
                {"attack = power + weapon 101 low",   p.attack(), 2 + 4},
                {"defence = stamina + 1 + 2 + 2",     p.defence(), 7},
                {"EXP to level 2",                    p.next_exp(), 16},
            };
            int bad = 0;
            for (const auto& c : cases) {
                if (c.got != c.want) {
                    lucent::error("player", "SELFTEST FAIL: {} -> {} (want {})",
                                  c.what, c.got, c.want);
                    ++bad;
                } else {
                    lucent::info("player", "  ok: {:<34} -> {}", c.what, c.got);
                }
            }
            // The other class: the formulas must MOVE, or a pair of constants
            // would pass every case above.
            mcf::PlayerStats q;
            q.stamina = 40; q.wisdom = 40; q.power = 40; q.level = 10;
            struct Case2 { const char* what; int32_t got, want; };
            const Case2 grown[] = {
                {"max HP at stamina 40 (40*40/10 + 19)", q.max_hp(), 179},
                {"max MP at wisdom 40 (40*94/100 + 5)",  q.max_mp(), 42},
                {"max HP at stamina 100 is capped",
                 [] { mcf::PlayerStats r; r.stamina = 100; return r.max_hp(); }(), 999},
                {"stats above 99 are capped",
                 [] { mcf::PlayerStats r; r.stamina = 500; return r.max_hp(); }(), 999},
                {"EXP to level 11",                      q.next_exp(),
                 12 * 10 + 3 * 100 + 103 * 1000 / 100},
            };
            for (const auto& c : grown) {
                if (c.got != c.want) {
                    lucent::error("player", "SELFTEST FAIL: {} -> {} (want {})",
                                  c.what, c.got, c.want);
                    ++bad;
                } else {
                    lucent::info("player", "  ok: {:<38} -> {}", c.what, c.got);
                }
            }
            // tblLevelup's rows are added through a lane swap, so which
            // regimen raises which stat is exactly the thing that could be off
            // by one. The game's own help text is an INDEPENDENT source for the
            // answer, and it is asserted here rather than admired:
            //   SYS_HELP_LEVELUP_FIGHTER  "improving physical ATK"
            //   SYS_HELP_LEVELUP_MONK     "improving DEF and increasing HP"
            //   SYS_HELP_LEVELUP_WIZARD   "increasing magical ATK and MP"
            //   SYS_HELP_LEVELUP_WISEMAN  "increasing limit gauge build speed"
            // Each regimen must move the stat its own text names, and must move
            // it MORE than the other three do.
            struct Reg { int r; const char* claim; int32_t mcf::PlayerStats::* stat; };
            const Reg regimens[] = {
                {mcf::PlayerStats::kWarrior, "physical ATK -> power",
                 &mcf::PlayerStats::power},
                {mcf::PlayerStats::kMonk,    "DEF and HP   -> stamina",
                 &mcf::PlayerStats::stamina},
                {mcf::PlayerStats::kMage,    "magic and MP -> wisdom",
                 &mcf::PlayerStats::wisdom},
                {mcf::PlayerStats::kSage,    "limit gauge  -> will",
                 &mcf::PlayerStats::will},
            };
            for (const auto& reg : regimens) {
                int32_t best = -1, mine = -1;
                for (const auto& other : regimens) {
                    mcf::PlayerStats r;
                    int32_t before = r.*(reg.stat);
                    r.LevelUp(other.r);
                    int32_t gain = r.*(reg.stat) - before;
                    if (other.r == reg.r) mine = gain;
                    else best = std::max(best, gain);
                }
                if (mine <= best) {
                    lucent::error("player", "SELFTEST FAIL: {} raises its stat by "
                                  "{}, but another regimen raises it by {}",
                                  mcf::PlayerStats::RegimenName(reg.r), mine, best);
                    ++bad;
                } else {
                    lucent::info("player", "  ok: {:<8} {} +{} (best rival +{})",
                                 mcf::PlayerStats::RegimenName(reg.r), reg.claim,
                                 mine, best);
                }
            }
            {   // A level-up is also a full heal, and it advances the level.
                mcf::PlayerStats r;
                r.hp = 1; r.mp = 0;
                r.LevelUp(mcf::PlayerStats::kMonk);
                if (r.level != 2 || r.hp != r.max_hp() || r.mp != r.max_mp()) {
                    lucent::error("player", "SELFTEST FAIL: level {} HP {}/{} MP "
                                  "{}/{} after a level-up", r.level, r.hp,
                                  r.max_hp(), r.mp, r.max_mp());
                    ++bad;
                } else {
                    lucent::info("player", "  ok: level-up refills -> level {} "
                                 "HP {}/{} MP {}/{}", r.level, r.hp, r.max_hp(),
                                 r.mp, r.max_mp());
                }
            }
            {   // CheckLevelUp's own conditions, each one able to veto.
                struct L { const char* what; int hp, exp, level; bool want; };
                const L ls[] = {
                    {"enough EXP and alive",      19, 16, 1, true},
                    {"one EXP short",             19, 15, 1, false},
                    {"exactly at the threshold",  19, 16, 1, true},
                    {"down at 0 HP",               0, 999, 1, false},
                    {"already level 99",          19, 999999, 99, false},
                };
                for (const auto& c : ls) {
                    mcf::PlayerStats r;
                    r.hp = c.hp; r.exp = c.exp; r.level = c.level;
                    if (r.level_up_due() != c.want) {
                        lucent::error("player", "SELFTEST FAIL: level_up_due for {} "
                                      "-> {} (want {})", c.what, !c.want, c.want);
                        ++bad;
                    } else {
                        lucent::info("player", "  ok: level-up due? {:<24} -> {}",
                                     c.what, c.want);
                    }
                }
            }
            lucent::info("player", "SELFTEST: {} cases, {} failures", 22, bad);
            return bad ? 1 : 0;
        }

        if (png_selftest) {
            // Decodes the real boot-path art and prints a checksum per image.
            // The cross-check is tools/asset/png_check.py, which decodes the
            // same files through Python's zlib -- an INDEPENDENT implementation
            // -- and compares. A decoder validated only against itself proves
            // nothing, and this one was written from scratch precisely because
            // the project has no zlib.
            const char* files[] = {"sk1/sqex.png", "sk1/titlelogo_en_color.png",
                                   "sk1/titlelogo_ja_color.png", "sk1/title_000.png"};
            int bad = 0, done = 0;
            for (const char* f : files) {
                if (!ar.Has(f)) {
                    lucent::error("png", "SELFTEST FAIL: {} not in the archive", f);
                    ++bad; continue;
                }
                auto blob = ar.Read(f);
                int w = 0, h = 0; std::vector<uint8_t> rgba;
                if (!mcf::DecodePng(blob, &w, &h, &rgba)) {
                    lucent::error("png", "SELFTEST FAIL: {} did not decode", f);
                    ++bad; continue;
                }
                if (rgba.size() != size_t(w) * size_t(h) * 4) {
                    lucent::error("png", "SELFTEST FAIL: {} gave {} bytes for {}x{}",
                                  f, rgba.size(), w, h);
                    ++bad; continue;
                }
                // A decode that returned all-zero would satisfy every check
                // above, so the content is what gets summarised.
                uint64_t sum = 0; size_t opaque = 0;
                for (size_t i = 0; i < rgba.size(); ++i) sum = sum * 131 + rgba[i];
                for (size_t i = 3; i < rgba.size(); i += 4) opaque += rgba[i] > 0;
                if (opaque == 0) {
                    lucent::error("png", "SELFTEST FAIL: {} is fully transparent "
                                  "-- a decode that produced nothing", f);
                    ++bad; continue;
                }
                lucent::info("png", "  {:<30} {}x{} hash {:016x} opaque {}/{}",
                             f, w, h, sum, opaque, size_t(w) * size_t(h));
                ++done;
            }
            // The other class. A decoder that never says no would "succeed"
            // on anything, so each of these MUST be refused.
            {
                auto blob = ar.Read("sk1/sqex.png");
                int w = 0, h = 0; std::vector<uint8_t> px;
                struct Neg { const char* what; std::vector<uint8_t> data; };
                std::vector<Neg> negs;
                negs.push_back({"empty input", {}});
                negs.push_back({"signature mismatch",
                                std::vector<uint8_t>(64, 0x41)});
                { auto t = blob; t.resize(t.size() / 2);
                  negs.push_back({"truncated mid-IDAT", t}); }
                { auto t = blob; t[25] = 3;      // colour type -> palette
                  negs.push_back({"palette colour type", t}); }
                { auto t = blob; t[28] = 1;      // interlace on
                  negs.push_back({"interlaced", t}); }
                { auto t = blob; t[24] = 16;     // bit depth 16
                  negs.push_back({"16-bit depth", t}); }
                for (auto& n : negs) {
                    if (mcf::DecodePng(n.data, &w, &h, &px)) {
                        lucent::error("png", "SELFTEST FAIL: accepted {}", n.what);
                        ++bad;
                    } else {
                        lucent::info("png", "  ok: refused {}", n.what);
                    }
                }
            }
            lucent::info("png", "SELFTEST: {} images decoded, {} failures", done, bad);
            return bad ? 1 : 0;
        }

        if (mode_selftest) {
            int bad = 0;
            auto ck=[&](const char* what, long got, long want){
                if (got!=want){ lucent::error("mode","SELFTEST FAIL: {} -> {} (want {})",
                    what,got,want); ++bad; }
                else lucent::info("mode","  ok: {:<46} -> {}",what,got);
            };
            // The enum values are ProcessMain's switch cases, not ours.
            ck("EMODE ModeInit",      (long)mcf::Mode::kInit, 2);
            ck("EMODE ModeCESA",      (long)mcf::Mode::kCESA, 3);
            ck("EMODE ModeMakerLogo", (long)mcf::Mode::kMakerLogo, 4);
            ck("EMODE ModeTitle",     (long)mcf::Mode::kTitle, 5);
            ck("EMODE ModeGame",      (long)mcf::Mode::kGame, 6);
            // The chain, driven for real rather than asserted as a list.
            mcf::ModeMachine m;
            std::vector<mcf::Mode> seen;
            for (int i = 0; i < 4000 && seen.size() < 5; ++i) {
                if (m.Step(1.f)) seen.push_back(m.current);
                if (m.current == mcf::Mode::kTitle) m.next = mcf::Mode::kGame;
            }
            const mcf::Mode want[] = {mcf::Mode::kInit, mcf::Mode::kCESA,
                                      mcf::Mode::kMakerLogo, mcf::Mode::kTitle,
                                      mcf::Mode::kGame};
            ck("the chain reaches all 5 modes", (long)seen.size(), 5);
            for (size_t i = 0; i < seen.size() && i < 5; ++i)
                ck(mcf::ModeName(want[i]), (long)seen[i], (long)want[i]);
            // The negative: a splash mode must NOT advance early. This checks
            // the MECHANISM -- that a gate exists at all -- and NOT the value
            // 0xb1, because the check reads the same constant the gate does.
            // Editing kMakerLogoFrames was tried as a sabotage and produced
            // zero failures, so the number itself is verified only by the
            // disassembly (ModeMakerLogo::Process, `cmp w8, #0xb1`), never by
            // this test. Said here so the passing line is not mistaken for
            // proof of the duration.
            mcf::ModeMachine e; e.Step(1.f);              // -> kInit
            e.next = mcf::Mode::kCESA; e.Step(1.f);       // -> kCESA
            for (int i = 0; i < mcf::kMakerLogoFrames - 2; ++i) e.Step(1.f);
            ck("ModeCESA still held one frame short of 0xb1",
               (long)(e.current == mcf::Mode::kCESA), 1);
            lucent::info("mode", "SELFTEST: {} failures", bad);
            return bad ? 1 : 0;
        }

        if (name_selftest) {
            // Both classes, on the game's OWN character set: names that must be
            // accepted and names that must be refused, with the exact error.
            // A validator only ever run on valid input proves nothing.
            const char* kStr = "sk1/str_en.bin";
            mcf::StringTable st;
            if (!ar.Has(kStr) || !st.Load(ar.Read(kStr))) {
                lucent::error("name", "{} missing or unparseable; NOTHING was "
                              "tested", kStr);
                return 1;
            }
            const std::string* use = st.Find("SYS_NAMEENTRY_USE");
            const std::string* nul = st.Find("SYS_COMMON_NULLSPACE");
            if (!use || use->empty()) {
                lucent::error("name", "SYS_NAMEENTRY_USE is not in the table; "
                              "NOTHING was tested");
                return 1;
            }
            const std::string ns = nul ? *nul : std::string();
            lucent::info("name", "SYS_NAMEENTRY_USE has {} code points; "
                         "SYS_COMMON_NULLSPACE is {}",
                         mcf::NameEntry::CodePoints(*use),
                         nul ? "present" : "ABSENT (that gate is untested)");
            struct Case { const char* name; bool ja; int want; };
            const Case cases[] = {
                // Accepted.
                {"Sumo",     false, mcf::NameEntry::kOk},
                {"A",        false, mcf::NameEntry::kOk},
                {"12345678", false, mcf::NameEntry::kOk},
                {"Zz09",     true,  mcf::NameEntry::kOk},
                // Refused, one per error code, in both languages.
                {"",         false, mcf::NameEntry::kEmpty},
                {"",         true,  mcf::NameEntry::kEmpty},
                {"123456789",false, mcf::NameEntry::kTooLong},
                {"12345",    true,  mcf::NameEntry::kTooLong},
                {"ab cd",    false, mcf::NameEntry::kProhibited},  // space
                {"a\"b",     false, mcf::NameEntry::kProhibited},  // quote
                {"a#b",      false, mcf::NameEntry::kProhibited},
                // The length gate must OUTRANK the character gate, because the
                // engine's csel chain applies it last.
                {"aaaa#aaaaa", false, mcf::NameEntry::kTooLong},
            };
            int bad = 0, n = 0;
            for (const auto& c : cases) {
                ++n;
                int got = mcf::NameEntry::Validate(c.name, *use, ns, c.ja);
                const char* id = mcf::NameEntry::ErrorId(got);
                if (got != c.want) {
                    ++bad;
                    lucent::error("name", "'{}' ({}) -> {} ({}), want {}",
                                  c.name, c.ja ? "ja" : "en", got,
                                  id ? id : "ok", c.want);
                } else {
                    lucent::info("name", "'{}' ({}) -> {}", c.name,
                                 c.ja ? "ja" : "en", id ? id : "accepted");
                }
            }
            // The 4-vs-8 split is the whole point of the language branch, so
            // check it directly rather than trusting the cases above.
            bool split = mcf::NameEntry::Validate("abcde", *use, ns, true)
                             == mcf::NameEntry::kTooLong &&
                         mcf::NameEntry::Validate("abcde", *use, ns, false)
                             == mcf::NameEntry::kOk;
            if (!split) { ++bad; lucent::error("name", "the ja/en length split "
                                               "(4 vs 8) does not hold"); }
            lucent::info("name", "{} cases, {} failures; length split {}",
                         n, bad, split ? "holds" : "BROKEN");
            return bad ? 1 : 0;
        }
        if (eventbox_selftest) {
            int bad = 0;
            auto ck = [&](const char* what, bool got, bool want) {
                if (got != want) {
                    ++bad;
                    lucent::error("eventbox", "SELFTEST FAIL: {} -> {} (want {})",
                                  what, got, want);
                } else {
                    lucent::info("eventbox", "  ok: {} -> {}", what, got);
                }
            };
            mcf::EventBox b;
            b.lo[0] = 10; b.lo[1] = 20; b.lo[2] = 30;
            b.hi[0] = 20; b.hi[1] = 40; b.hi[2] = 50;
            constexpr float ox = 100, oz = 200;
            ck("strictly inside", b.IsHit(115, 30, 240, ox, oz), true);
            ck("lower X boundary is outside", b.IsHit(110, 30, 240, ox, oz), false);
            ck("upper Z boundary is outside", b.IsHit(115, 30, 250, ox, oz), false);
            ck("below the Y range is outside", b.IsHit(115, 19.9f, 240, ox, oz), false);
            b.enabled = false;
            ck("disabled box cannot hit", b.IsHit(115, 30, 240, ox, oz), false);
            b.enabled = true; b.no_touch = true;
            ck("no-touch box cannot hit", b.IsHit(115, 30, 240, ox, oz), false);
            b.no_touch = false; b.flags = 0x01; b.flags |= 0x40;
            ck("flag set preserves existing bits", b.flags == 0x41, true);
            b.flags &= ~uint32_t(0x01);
            ck("flag clear preserves other bits", b.flags == 0x40, true);
            b.floor_y = true; b.lo[1] = -1; b.hi[1] = 30;
            b.ResolveFloorY(60);
            ck("floor sentinel moves lower bound", b.lo[1] == 60, true);
            ck("floor sentinel translates upper bound", b.hi[1] == 90, true);
            lucent::info("eventbox", "SELFTEST: 10 cases, {} failures", bad);
            return bad ? 1 : 0;
        }
        if (ai_selftest) {
            const char* kEnemyDat = "sk1/enemydat.bin";
            if (!ar.Has(kEnemyDat)) {
                lucent::error("ai", "{} is not in the archive; NOTHING was "
                              "tested", kEnemyDat);
                return 1;
            }
            auto rows = mcf::ParseEnemyDat(ar.Read(kEnemyDat));
            if (rows.empty()) {
                lucent::error("ai", "{} did not parse; NOTHING was tested",
                              kEnemyDat);
                return 1;
            }
            int bad = 0;
            // The defining property of weighted roulette: sweeping every roll
            // in [0, sum) must select state i exactly weight[i] times. This is
            // checked against every shipping machine rather than a toy one, and
            // it fails if any arm of the subtract-chain is wrong -- a swapped
            // pair, an off-by-one bound, or a `<=` for a `<`.
            int checked = 0, terminal = 0;
            for (const auto& e : rows) {
                for (int r = 0; r < 2; ++r) {
                    for (int s = 0; s < 4; ++s) {
                        const auto& st = e.ai[r].state[s];
                        int32_t sum = st.weight_sum();
                        if (sum < 1) {
                            // Terminal: no roll may move it, including rolls
                            // that would be valid for a non-terminal state.
                            ++terminal;
                            for (int32_t roll = 0; roll < 8; ++roll)
                                if (mcf::NextAiState(e.ai[r], s, roll) != s) {
                                    lucent::error("ai", "SELFTEST FAIL: enemy {} "
                                                  "rec{} state {} has weight sum 0 "
                                                  "but roll {} moved it",
                                                  e.id, r, s, roll);
                                    ++bad;
                                    break;
                                }
                            continue;
                        }
                        int hits[4] = {0, 0, 0, 0};
                        int stayed = 0;
                        for (int32_t roll = 0; roll < sum; ++roll) {
                            int next = mcf::NextAiState(e.ai[r], s, roll);
                            if (next < 0 || next > 3) { ++bad; break; }
                            ++hits[next];
                            if (next == s) ++stayed;
                        }
                        (void)stayed;
                        for (int i = 0; i < 4; ++i) {
                            // hits[i] counts landing on i, which for i == s also
                            // includes the weight-driven self-transition, so the
                            // comparison is against the weight either way.
                            if (hits[i] != st.weight[i]) {
                                lucent::error("ai", "SELFTEST FAIL: enemy {} rec{} "
                                              "state {} -> {} selected {} times "
                                              "over {} rolls, weight is {}",
                                              e.id, r, s, i, hits[i], sum,
                                              st.weight[i]);
                                ++bad;
                            }
                        }
                        ++checked;
                    }
                }
            }
            // NOTE on the terminal count: it is a census, not coverage. The
            // subtract-chain returns the state unchanged for an all-zero
            // descriptor even without the explicit early-out, so this arm passes
            // whether or not that guard exists -- confirmed by deleting it and
            // seeing 0 failures. It is reported so the number is not mistaken
            // for a test of the guard.
            lucent::info("ai", "  ok: {} rolling descriptors sweep-checked; "
                         "{} terminal ones held (census, not coverage -- see the "
                         "note at this call site)", checked, terminal);
            // The other class -- cases that MUST behave a particular way, built
            // by hand so the test does not depend only on shipping data.
            struct C { int32_t w[4]; int state; int32_t roll; int want; const char* what; };
            const C cases[] = {
                {{1, 0, 0, 0}, 0, 0, 0, "all weight on 0 stays at 0"},
                {{0, 1, 0, 0}, 0, 0, 1, "all weight on 1 moves to 1"},
                {{0, 0, 0, 1}, 0, 0, 3, "all weight on 3 reaches the last arm"},
                {{0, 0, 1, 0}, 3, 0, 2, "from state 3, weight on 2 moves to 2"},
                {{2, 3, 0, 0}, 0, 1, 0, "roll 1 of 5 falls in the first weight"},
                {{2, 3, 0, 0}, 0, 2, 1, "roll 2 of 5 falls in the second"},
                {{2, 3, 0, 0}, 0, 4, 1, "roll 4 of 5 is still the second"},
                {{0, 0, 0, 0}, 1, 0, 1, "a terminal state never moves"},
            };
            for (const auto& c : cases) {
                mcf::EnemyStats::AiMachine m{};
                for (int i = 0; i < 4; ++i) m.state[c.state].weight[i] = c.w[i];
                int got = mcf::NextAiState(m, c.state, c.roll);
                if (got != c.want) {
                    lucent::error("ai", "SELFTEST FAIL: {} -> {} (want {})",
                                  c.what, got, c.want);
                    ++bad;
                } else {
                    lucent::info("ai", "  ok: {:<44} -> {}", c.what, got);
                }
            }
            lucent::info("ai", "SELFTEST: {} machines, {} hand cases, {} failures",
                         checked + terminal, std::size(cases), bad);
            return bad ? 1 : 0;
        }

        if (inventory_selftest) {
            int bad = 0, ran = 0;
            auto check = [&](const char* what, long got, long want) {
                ++ran;
                if (got != want) {
                    lucent::error("inv", "SELFTEST FAIL: {} -> {} (want {})",
                                  what, got, want);
                    ++bad;
                } else {
                    lucent::info("inv", "  ok: {:<44} -> {}", what, got);
                }
            };

            // DataTableGetIdType's six ranges, each tested on BOTH sides of
            // both edges -- a range check that is only ever fed values inside
            // the range cannot report a wrong bound.
            struct T { int32_t id; int type; };
            const T types[] = {
                {0, 0}, {1, 1}, {37, 1}, {38, 0},
                {100, 0}, {101, 2}, {118, 2}, {119, 0},
                {200, 0}, {201, 4}, {206, 4}, {207, 0},
                {300, 0}, {301, 5}, {309, 5}, {310, 0},
                {400, 0}, {401, 6}, {409, 6}, {410, 0},
                {500, 0}, {501, 7}, {508, 7}, {509, 0},
            };
            for (const auto& t : types) {
                ++ran;
                if (mcf::Inventory::IdType(t.id) != t.type) {
                    lucent::error("inv", "SELFTEST FAIL: IdType({}) -> {} (want {})",
                                  t.id, mcf::Inventory::IdType(t.id), t.type);
                    ++bad;
                }
            }
            lucent::info("inv", "  ok: {} id-type cases, both sides of every "
                         "range edge", std::size(types));

            {   // Init's four ids, and the bags they land in.
                mcf::Inventory inv;
                inv.NewGame();
                check("new game holds weapon 101",  inv.Count(101), 1);
                check("new game holds helm 201",    inv.Count(201), 1);
                check("new game holds armour 301",  inv.Count(301), 1);
                check("new game holds accessory 401", inv.Count(401), 1);
                // The negative: it must NOT hold things it was never granted.
                check("new game holds no item 1",   inv.Count(1), 0);
                check("new game holds no magic 501", inv.Count(501), 0);
                check("new game holds no weapon 102", inv.Count(102), 0);
                // 201, 301 and 401 are types 4, 5 and 6 -- one SHARED bag, so
                // three of NewGame's four grants occupy the same 16 slots.
                check("helm/armour/accessory share a bag",
                      mcf::Inventory::BagOf(201) == mcf::Inventory::BagOf(301) &&
                      mcf::Inventory::BagOf(301) == mcf::Inventory::BagOf(401), 1);
                check("weapons are a different bag",
                      mcf::Inventory::BagOf(101) != mcf::Inventory::BagOf(201), 1);
            }
            {   // Nothing stacks: adding the same id twice takes two slots.
                mcf::Inventory inv;
                inv.Add(1); inv.Add(1); inv.Add(1);
                check("three of item 1 occupy three slots", inv.Count(1), 3);
                check("and the sequence keys are distinct",
                      inv.items[0].seq != inv.items[1].seq &&
                      inv.items[1].seq != inv.items[2].seq, 1);
                check("sequence keys ascend with acquisition",
                      inv.items[2].seq > inv.items[0].seq, 1);
            }
            {   // The bag fills and then REFUSES -- the failing case, which is
                // the one a first-free-slot scan can get wrong.
                mcf::Inventory inv;
                int placed = 0;
                for (int i = 0; i < 40; ++i) if (inv.Add(2)) ++placed;
                check("item bag accepts exactly 16", placed, mcf::Inventory::kSlots);
                check("and reports full afterwards", inv.Add(2), 0);
                check("a free slot is reusable after a delete",
                      inv.Del(2) && inv.Add(2), 1);
            }
            {   // Magic is direct-indexed, not searched, so an id maps to one
                // fixed slot and cannot be held twice.
                mcf::Inventory inv;
                check("magic 505 is not held to begin with", inv.Count(505), 0);
                check("granting magic 505 succeeds", inv.Add(505), 1);
                check("magic 505 is now held", inv.Count(505), 1);
                check("granting it twice fails", inv.Add(505), 0);
                check("it landed on slot id-501", inv.magic[505 - 501].id, 505);
                check("and disturbed no neighbour", inv.magic[3].id + inv.magic[5].id, 0);
            }
            {   // An id in no table has no bag, and must be refused rather than
                // silently dropped into bag 0.
                mcf::Inventory inv;
                check("id 9999 is refused", inv.Add(9999), 0);
                check("and nothing was stored", inv.items[0].id, 0);
            }
            lucent::info("inv", "SELFTEST: {} cases, {} failures", ran, bad);
            return bad ? 1 : 0;
        }

        if (text_selftest) {
            // Runs the control-code expander against BOTH classes -- strings
            // that must change and strings that must not -- and then over the
            // whole shipping table, because a corpus sweep with no denominator
            // proves nothing.
            mcf::StringTable tbl;
            auto sp = std::format("sk1/str_{}.bin", lang);
            if (!ar.Has(sp) || !tbl.Load(ar.Read(sp))) {
                lucent::error("text", "{} missing or malformed; NOTHING was tested", sp);
                return 1;
            }
            mcf::FormatParams p;
            p.hero = "Sumo"; p.girl = "Fuji";
            p.prm[0] = "P0"; p.prm[1] = "P1"; p.prm[2] = "P2"; p.prm[3] = "P3";
            // The two @N cases resolve through the table, so what they must
            // produce depends on which language is loaded. Hard-coding the
            // English answer would make a --lang ja run fail for the wrong
            // reason -- and hide a real failure behind an expected one.
            bool ja = lang == "ja";
            struct Case { const char* in; const char* want; };
            const Case cases[] = {
                {"@N(36):", ja ? "囚人:" : "Prisoner:"},   // the code that started this
                {"@N(1)",   ja ? "旅の男" : "Mysterious Traveler"},
                {"@H",      "Sumo"},
                {"@h",      "Sumo"},
                {"@G",      "Fuji"},
                {"@P/@i/@I/@S", "P0/P1/P2/P3"},
                {"@@",      "@"},                  // escape, not a code
                {"@Z",      "Z"},                  // unknown: '@' dropped, letter kept
                {"plain",   "plain"},              // must NOT be rewritten
                {"e@mail",  "email"},              // ditto for the engine's own rule
                {"@N(9999)", "CHARACTER_NAME_9999"},  // a miss stays visible
                {"日本語@H", "日本語Sumo"},         // multi-byte text survives
            };
            int bad = 0;
            for (const auto& c : cases) {
                auto got = mcf::CnvFormatString(c.in, &tbl, p);
                if (got != c.want) {
                    lucent::error("text", "SELFTEST FAIL: \"{}\" -> \"{}\" (want \"{}\")",
                                  c.in, got, c.want);
                    ++bad;
                } else {
                    lucent::info("text", "  ok: {:<14} -> {}", c.in, got);
                }
            }
            // The sweep. Every '@' the expander leaves behind is a code it does
            // not know, so the residue is the honest measure of coverage.
            size_t total = 0, with_code = 0, residue = 0;
            std::string first_residue;
            for (const auto& id : tbl.ids()) {
                const std::string* t = tbl.Find(id);
                ++total;
                if (t->find('@') == std::string::npos) continue;
                ++with_code;
                auto out = mcf::CnvFormatString(*t, &tbl, p);
                if (out.find('@') != std::string::npos) {
                    ++residue;
                    if (first_residue.empty()) first_residue = id;
                }
            }
            lucent::info("text", "SELFTEST: {} cases, {} failures", 12, bad);
            lucent::info("text", "sweep [{}]: {} strings, {} carry a control code, "
                         "{} still contain '@' after expansion{}", lang, total,
                         with_code, residue,
                         first_residue.empty() ? "" : std::format(" (first: {})",
                                                                  first_residue));
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
            // The game's string table. Both languages ship; en is the default
            // and --lang ja selects the original Japanese.
            mcf::StringTable strings;
            {
                auto sp = std::format("sk1/str_{}.bin", lang);
                if (!ar.Has(sp)) {
                    lucent::warn("text", "{} not in the archive; dialogue ids will "
                                 "echo instead of resolving", sp);
                } else if (!strings.Load(ar.Read(sp))) {
                    lucent::error("text", "{} failed to parse; dialogue ids will "
                                  "echo instead of resolving", sp);
                } else {
                    lucent::info("text", "{}: {} strings", sp, strings.size());
                    sc.SetStrings(&strings);
                    if (!show_string.empty()) {
                        if (const std::string* t = strings.Find(show_string))
                            sc.last_message = sc.FormatText(*t);
                        else
                            lucent::warn("text", "--show-string {}: not in the "
                                         "table ({} ids)", show_string, strings.size());
                    }
                }
            }
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

        if (!string_id.empty()) {
            // Resolve one id in every shipping language. Also the quickest way
            // to confirm the table is wired to the same code the game uses.
            int found = 0;
            for (const char* l : {"en", "ja"}) {
                auto sp = std::format("sk1/str_{}.bin", l);
                mcf::StringTable t;
                if (!ar.Has(sp) || !t.Load(ar.Read(sp))) {
                    lucent::error("text", "{} missing or malformed", sp);
                    continue;
                }
                const std::string* v = t.Find(string_id);
                if (v) {
                    ++found;
                    // Escape newlines: the game's lines are multi-line and a raw
                    // dump silently shows only the first line.
                    std::string shown;
                    for (char c : *v) { if (c == '\n') shown += "\\n"; else shown += c; }
                    lucent::info("text", "{} [{}] = \"{}\"", string_id, l, shown);
                }
                else lucent::warn("text", "{} [{}] is not in the table ({} ids)",
                                  string_id, l, t.size());
            }
            return found ? 0 : 1;
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
            // Only M####_##_## is a room. Three M*.smdl (M0001, M0002,
            // M0020) carry no grid suffix and are not rooms at all; counting
            // them inflated the total and made them look like rooms missing
            // their collision mesh.
            auto is_room = [](const std::string& n) {
                if (n.size() != 11 || n[0] != 'M' || n[5] != '_' || n[8] != '_')
                    return false;
                for (size_t i : {1u, 2u, 3u, 4u, 6u, 7u, 9u, 10u})
                    if (n[i] < '0' || n[i] > '9') return false;
                return true;
            };
            std::vector<std::string> rooms, non_rooms;
            for (const auto& e : ar.entries()) {
                if (e.name.size() < 10) continue;
                if (e.name.compare(0, 5, "sk1/M") != 0) continue;
                if (e.name.compare(e.name.size() - 5, 5, ".smdl") != 0) continue;
                auto n = e.name.substr(4, e.name.size() - 9);
                (is_room(n) ? rooms : non_rooms).push_back(n);
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
                          invisible = 0, col_fail = 0, on_floor = 0,
                          off_floor = 0, engine_placed = 0, on_floor_aabb = 0;
                     long size_src[4] = {0, 0, 0, 0}; } c;
            std::map<std::string, int> missing_models;
            for (const auto& r : rooms) {
                ++c.rooms;
                try { mcf::ParseSmdl(ar.Read(std::format("sk1/{}.smdl", r))); }
                catch (const std::exception&) { ++c.mesh_fail; continue; }

                auto cs = std::format("sk1/{}.scol", r);
                mcf::Collision rc;
                bool has_col = ar.Has(cs);
                if (!has_col) ++c.no_col;
                else { try { rc = mcf::ParseScol(ar.Read(cs)); }
                       catch (const std::exception&) { has_col = false; ++c.col_fail; } }

                w.Reset();
                auto sp = std::format("sk1/{}.lua", r);
                if (!ar.Has(sp)) ++c.no_script;
                else if (!sc2.Run(sp, ar.Read(sp))) ++c.script_fail;

                c.actors += long(w.actors().size());
                c.boxes += long(w.boxes.size());
                // Room origin. The engine's rule is size.w * grid_x (see
                // mcf::FindRoomSize); the fixed 300x240 it replaced is kept as
                // the control, because a new rule that is not compared with the
                // old one is not measured.
                float gx = float(std::atoi(r.substr(6, 2).c_str()));
                float gy = float(std::atoi(r.substr(9, 2).c_str()));
                mcf::RoomSize rsz = mcf::FindRoomSize(ar, r);
                ++c.size_src[rsz.source];
                float ox = gx * rsz.w, oz = gy * rsz.h;
                float bx = gx * 300.f, bz = gy * 240.f;
                for (const auto& a : w.actors()) {
                    // Every spawned actor should stand on floor. An actor over
                    // nothing keeps its script Y and floats -- the bug that hid
                    // enemy attacks for a whole session.
                    if (has_col) {
                        float g;
                        float wx = a.pos[0] + ox, wz = a.pos[2] + oz;
                        bool onfloor = rc.GetFloor(wx, wz, mcf::Collision::kFloorMask, &g);
                        bool onfloor_aabb = rc.GetFloor(a.pos[0] + bx, a.pos[2] + bz,
                                                        mcf::Collision::kFloorMask, &g);
                        // A script-spawned NPC at (0,0) is placed by the engine,
                        // so its script position is not expected to be walkable.
                        if (a.random_place) ++c.engine_placed;
                        else if (onfloor) { ++c.on_floor; if (onfloor_aabb) ++c.on_floor_aabb; }
                        else {
                            ++c.off_floor;
                            if (onfloor_aabb) ++c.on_floor_aabb;
                            lucent::debug("census",
                                "  {} {} '{}' kind {} id {} at room-local "
                                "({:.0f},{:.0f},{:.0f}) has no floor",
                                r, a.random_place ? "rand" : "fixed", a.handle,
                                a.kind, a.type_id, a.pos[0], a.pos[1], a.pos[2]);
                        }
                    }
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
            lucent::info("census", "{} rooms ({} non-room M*.smdl skipped): {} mesh "
                         "parse failures, {} without collision, {} script failures",
                         c.rooms, non_rooms.size(), c.mesh_fail, c.no_col, c.script_fail);
            // Most overworld tiles genuinely have no script -- they are entered
            // by walking, not by a scripted transition -- so this is reported
            // as a fact, not as a fault.
            lucent::info("census", "  {} rooms have no script (expected: overworld "
                         "tiles are entered by walking)", c.no_script);
            lucent::info("census", "  {} actors spawned, {} with no model ({} distinct), "
                         "{} intentionally invisible (eNPC.TRANS)",
                         c.actors, c.actor_no_model, missing_models.size(), c.invisible);
            lucent::info("census", "  {} event boxes", c.boxes);
            lucent::info("census", "  actor placement: {} stand on floor, {} do NOT "
                         "(they would float at their script Y), {} are engine-placed "
                         "so their script position is not expected to be walkable; "
                         "{} collision meshes failed to parse",
                         c.on_floor, c.off_floor, c.engine_placed, c.col_fail);
            lucent::info("census", "  origin rule comparison: per-room size puts {} "
                         "actors on floor; a fixed 300x240 puts {} (of {} tested)",
                         c.on_floor, c.on_floor_aabb, c.on_floor + c.off_floor);
            lucent::info("census", "  room size source: {} from the engine's world "
                         "table, {} from .gdt, {} from the collision AABB (inferred), "
                         "{} defaulted to 300x240",
                         c.size_src[mcf::RoomSize::kTable],
                         c.size_src[mcf::RoomSize::kGdt],
                         c.size_src[mcf::RoomSize::kAabb],
                         c.size_src[mcf::RoomSize::kDefault]);
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
            // Dialogue coverage. This census fires each script's own event
            // handlers, which is where msgId actually runs, so it is the only
            // place the number is meaningful.
            mcf::StringTable cstr;
            bool have_strings = false;
            {
                auto sp = std::format("sk1/str_{}.bin", lang);
                have_strings = ar.Has(sp) && cstr.Load(ar.Read(sp));
                if (!have_strings)
                    lucent::warn("lua", "{} missing or malformed; dialogue coverage "
                                 "will NOT be measured", sp);
            }
            long msgs = 0, msg_missing = 0;

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
                if (have_strings) sc.SetStrings(&cstr);
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
                        // A handler that showed a line leaves the script waiting
                        // for the player; clear it so the next handler can run.
                        if (sc.message_pending) {
                            sc.message_pending = false;
                            sc.last_message.clear();
                        }
                    }
                }
                for (const auto& [fn, rec] : sc.calls) totals[fn] += rec.count;
                msgs += sc.messages_shown;
                msg_missing += sc.message_ids_missing;
            }
            lucent::info("lua", "executed {} map scripts, {} failed", ok, failed);
            lucent::info("lua", "invoked {} event handlers, {} raised errors "
                         "(expected: many need live game state)", handlers, handler_errs);
            if (have_strings)
                lucent::info("lua", "dialogue: {} lines shown, {} of them used an id "
                             "the string table does not have", msgs, msg_missing);
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

        bool have_display = SDL_getenv("DISPLAY") || SDL_getenv("WAYLAND_DISPLAY");
        if (shot.empty() && !have_display)
            lucent::warn("host", "no display detected; use --screenshot for headless");
        // A --screenshot run is headless by nature: it renders N frames, writes
        // a PNG and exits. Opening a real window for that steals focus and
        // flashes on the user's desktop for no benefit, so it goes through
        // SDL's offscreen driver whether or not a display exists. --window
        // opts back in for the rare case of watching a capture run live.
        if (!shot.empty() && !force_window)
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
        // Say which driver we got. "windowless" is easy to believe and hard to
        // notice when it silently stops being true.
        lucent::info("host", "video driver: {}", SDL_GetCurrentVideoDriver());

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
            // The game's own bitmap font, for the message window.
            mcf::Font font;
            GLuint fontTex = 0, textVbo = 0;
            {
                // The engine draws UI text with sk1/font_<lang>.bin, not with
                // BasicFont: FontFileLoad @ 0x2c2608 builds the FTData from it.
                // BasicFont covers ASCII 32..126 only, which is why the
                // copyright sign, the em dash and every kana were dropped;
                // font_en covers 543 characters and font_ja 1209. BasicFont
                // stays as the fallback rather than being deleted, because it
                // is a real shipping font and a missing font_*.bin should
                // degrade to worse text, not to no text.
                std::string fp = std::string("sk1/font_") + (lang == "ja" ? "ja" : "en") + ".bin";
                bool ok = ar.Has(fp) && font.LoadFontBin(ar.Read(fp));
                if (!ok) {
                    lucent::warn("text", "{} missing or malformed; falling back "
                                 "to BasicFont (ASCII 32..126 only)", fp);
                    fp = "sk1/BasicFont.sfont";
                    ok = ar.Has(fp) && font.Load(ar.Read(fp));
                }
                if (!ok) {
                    lucent::warn("text", "{} missing or malformed; dialogue will "
                                 "be logged but not drawn", fp);
                } else {
                    lucent::info("text", "{}: {}x{} atlas, {} glyphs", fp,
                                 font.width(), font.height(), font.glyphs());
                    glGenTextures(1, &fontTex);
                    glBindTexture(GL_TEXTURE_2D, fontTex);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    // 8-bit coverage. GL_LUMINANCE replicates into rgb, so the
                    // shader can read it from .r on both GLES2 and desktop GL.
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                                 GLsizei(font.width()), GLsizei(font.height()), 0,
                                 GL_LUMINANCE, GL_UNSIGNED_BYTE, font.atlas().data());
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                }
                glGenBuffers(1, &textVbo);
            }
            // Every UI site here was laid out against BasicFont, whose tallest
            // glyph reaches 17px below the line origin. A font_*.bin line is
            // 28px, so text drawn at the same `scale` would be 1.6x too big and
            // would overflow the boxes. Normalising by the ratio keeps the
            // layout fixed and leaves BasicFont at exactly 1.0.
            const float kDesignLine = 17.f;
            const float font_scale =
                font.line_height() ? kDesignLine / float(font.line_height()) : 1.f;
            GLuint progText = LinkProgram(kVText, kFText);
            GLuint fadeVbo = 0;
            glGenBuffers(1, &fadeVbo);
            {
                const float quad[] = {-1,-1, 3,-1, -1,3};   // one oversized triangle
                glBindBuffer(GL_ARRAY_BUFFER, fadeVbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
            }
            // Boot art. Names come from the modes' own string literals:
            // ModeMakerLogo formats "sk1/sqex%s.png", ModeTitle uses the
            // per-language title logo. Loaded lazily and only when --boot is
            // on, so a normal run pays nothing for them.
            GLuint progSprite = LinkProgram(kVText, kFSprite);
            GLuint spriteVbo = 0;
            glGenBuffers(1, &spriteVbo);
            struct Sprite { GLuint tex = 0; int w = 0, h = 0; };
            auto loadSprite = [&](const char* name) -> Sprite {
                Sprite sp;
                if (!ar.Has(name)) {
                    lucent::warn("boot", "{} is not in the archive; that screen "
                                 "will be blank", name);
                    return sp;
                }
                int w = 0, h = 0; std::vector<uint8_t> rgba;
                if (!mcf::DecodePng(ar.Read(name), &w, &h, &rgba)) {
                    lucent::warn("boot", "{} did not decode", name);
                    return sp;
                }
                glGenTextures(1, &sp.tex);
                glBindTexture(GL_TEXTURE_2D, sp.tex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, rgba.data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                sp.w = w; sp.h = h;
                lucent::info("boot", "{}: {}x{}", name, w, h);
                return sp;
            };
            Sprite sprMaker, sprTitle;
            if (boot_chain) {
                sprMaker = loadSprite("sk1/sqex.png");
                sprTitle = loadSprite(lang == "ja" ? "sk1/titlelogo_ja_color.png"
                                                   : "sk1/titlelogo_en_color.png");
            }
            // Aspect-fit, because the art is authored at 960x544 and the window
            // is whatever the user gave us. Letterboxing preserves the logo's
            // proportions; stretching would not.
            auto drawSprite = [&](const Sprite& sp) {
                if (!sp.tex) return;
                int vw = 0, vh = 0;
                SDL_GetWindowSizeInPixels(win, &vw, &vh);
                if (vw <= 0 || vh <= 0) { vw = W; vh = H; }
                float sa = float(sp.w) / float(sp.h), va = float(vw) / float(vh);
                float ex = sa > va ? 1.f : sa / va;
                float ey = sa > va ? va / sa : 1.f;
                const float q[] = {
                    -ex,-ey, 0,1,   ex,-ey, 1,1,   ex, ey, 1,0,
                    -ex,-ey, 0,1,   ex, ey, 1,0,  -ex, ey, 0,0};
                glUseProgram(progSprite);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_DEPTH_TEST);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sp.tex);
                glUniform1i(glGetUniformLocation(progSprite, "tex"), 0);
                glUniform4f(glGetUniformLocation(progSprite, "tint"), 1, 1, 1, 1);
                glBindBuffer(GL_ARRAY_BUFFER, spriteVbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof q, q, GL_STREAM_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16,
                                      (const void*)(sizeof(float) * 2));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            };

            // Screen-space text, in the game's own font. The HUD, the message
            // window and the level-up panel each build their quads inline
            // because they also draw boxes and bars; this is the plain-text
            // path the title screen needs, factored so the title does not
            // become a fourth copy of the same glyph loop.
            //
            // x is the LEFT edge unless `centre`, in which case it is the
            // centre. Returns the advance width, so a caller can lay out
            // without measuring twice.
            auto drawUiText = [&](const std::string& s, float x, float y,
                                  float scale, bool centre,
                                  float r, float g, float b, float a) -> float {
                float wpx = 0.f;
                for (uint32_t ch : mcf::Utf8Codepoints(s))
                    if (const mcf::Glyph* gl = font.Find(ch))
                        wpx += float(gl->Advance()) * scale;
                if (s.empty() || !font.height()) return wpx;
                float tx = centre ? x - wpx * 0.5f : x;
                const float aw = float(font.width()), ah = float(font.height());
                std::vector<float> verts;
                auto sx = [&](float px) { return px / float(W) * 2.f - 1.f; };
                auto sy = [&](float py) { return 1.f - py / float(H) * 2.f; };
                for (uint32_t ch : mcf::Utf8Codepoints(s)) {
                    const mcf::Glyph* gl = font.Find(ch);
                    if (!gl) {
                        // A dropped byte is a hole in the text, so say so once
                        // per byte value rather than rendering a shorter string
                        // and looking correct. The atlas is ASCII 32..126, so
                        // anything above that (the copyright sign in
                        // SYS_TITLE_COPYRIGHT_1, and all CJK) has no glyph --
                        // the original draws those with the Android system
                        // font, which is not in the archive.
                        static std::set<unsigned char> warned;
                        if (warned.insert(ch).second)
                            lucent::warn("text", "no glyph for byte 0x{:02x}; the "
                                         "atlas is ASCII 32..126", ch);
                        continue;
                    }
                    if (gl->w && gl->h) {
                        float gx = tx + float(gl->left) * scale;
                        float gy = y + float(gl->top) * scale;
                        float x1 = gx + float(gl->w) * scale;
                        float y1 = gy + float(gl->h) * scale;
                        float u0 = float(gl->x) / aw, v0 = float(gl->y) / ah;
                        float u1 = float(gl->x + gl->w) / aw;
                        float v1 = float(gl->y + gl->h) / ah;
                        float q[6][4] = {{sx(gx), sy(gy), u0, v0}, {sx(x1), sy(gy), u1, v0},
                                         {sx(x1), sy(y1), u1, v1}, {sx(gx), sy(gy), u0, v0},
                                         {sx(x1), sy(y1), u1, v1}, {sx(gx), sy(y1), u0, v1}};
                        for (auto& v : q) verts.insert(verts.end(), v, v + 4);
                    }
                    tx += float(gl->Advance()) * scale;
                }
                if (verts.empty()) return wpx;
                glUseProgram(progText);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_DEPTH_TEST);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, fontTex);
                glUniform1i(glGetUniformLocation(progText, "tex"), 0);
                glUniform4f(glGetUniformLocation(progText, "tint"), r, g, b, a);
                glUniform1f(glGetUniformLocation(progText, "useTex"), 1.f);
                glBindBuffer(GL_ARRAY_BUFFER, textVbo);
                glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * 4),
                             verts.data(), GL_STREAM_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)(uintptr_t)8);
                glDrawArrays(GL_TRIANGLES, 0, GLsizei(verts.size() / 4));
                glDisableVertexAttribArray(1);
                return wpx;
            };

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
            // Chip-resolution pathing state. `chip_walk` is per room;
            // `chip_dist` is rebuilt each frame from the player's chip and
            // shared by every chaser, as one flood fill rather than one per
            // enemy.
            int chip_w = 0, chip_h = 0;
            std::vector<uint8_t> chip_walk;
            std::vector<float>   chip_height;   // ModeGame + 0x9bb0's counterpart
            std::vector<int32_t> chip_dist;
            bool chip_fill_logged = false;
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
            // The game's string table. Both languages ship; en is the default
            // and --lang ja selects the original Japanese.
            mcf::StringTable strings;
            {
                auto sp = std::format("sk1/str_{}.bin", lang);
                if (!ar.Has(sp)) {
                    lucent::warn("text", "{} not in the archive; dialogue ids will "
                                 "echo instead of resolving", sp);
                } else if (!strings.Load(ar.Read(sp))) {
                    lucent::error("text", "{} failed to parse; dialogue ids will "
                                  "echo instead of resolving", sp);
                } else {
                    lucent::info("text", "{}: {} strings", sp, strings.size());
                    sc.SetStrings(&strings);
                    if (!show_string.empty()) {
                        if (const std::string* t = strings.Find(show_string))
                            sc.last_message = sc.FormatText(*t);
                        else
                            lucent::warn("text", "--show-string {}: not in the "
                                         "table ({} ids)", show_string, strings.size());
                    }
                }
            }
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
            mcf::RoomSize room_size;
            {
                auto us = room_name.rfind('_');
                auto us2 = room_name.rfind('_', us - 1);
                if (us != std::string::npos && us2 != std::string::npos) {
                    int gx = std::atoi(room_name.substr(us2 + 1, us - us2 - 1).c_str());
                    int gy = std::atoi(room_name.substr(us + 1).c_str());
                    room_size = mcf::FindRoomSize(ar, room_name);
                    room_org[0] = float(gx) * room_size.w;
                    room_org[2] = float(gy) * room_size.h;
                    lucent::info("world", "room grid ({},{}), size {:.0f}x{:.0f} "
                                 "from {} -> origin ({:.0f},0,{:.0f})", gx, gy,
                                 room_size.w, room_size.h, room_size.source_name(),
                                 room_org[0], room_org[2]);
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
            // NOT refined from the collision AABB. That was tried and
            // FALSIFIED: the AABB's lo corner differs from the grid position by
            // multiples of 30 (the chip size) in 659 of 992 rooms and its size
            // varies (330x270, 300x240, 300x300, 300x180...), because it is a
            // tight bound on the collision geometry rather than the room's
            // extent. It scored 116/116 on the actor-on-floor test only because
            // floors are broad enough to absorb the error. See docs/assets.md.

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
            // The chip grids, built once per room. This is the engine's own
            // construction, not a substitute for it: ModeGame::MakeRandomChrPosTbl
            // @ 0x2dd0a0 sizes the grid `fcvtzs(roomSize / 30)` -- TRUNCATE --
            // and fills it by probing the collision mesh once per chip, at the
            // chip CENTRE, through CheckAddPos @ 0x2dcd20. It stores an
            // attribute byte at ModeGame+0x9ba8 and the ground height at
            // +0x9bb0, writing the sentinel baseY + 10000 where there is no
            // floor. Two departures, both named in docs/re-frontier.md: the
            // port has no character/AppObject occupancy bits (the engine's bit
            // 6 and bit 7, via a radius-12 push-back sphere), and it does not
            // separate the two floor-mask classes in bits 0-1.
            chip_w = int(room_size.w / 30.f);
            chip_h = int(room_size.h / 30.f);
            chip_walk.assign(size_t(chip_w) * size_t(chip_h), 0);
            chip_height.assign(size_t(chip_w) * size_t(chip_h), kChipNoFloor);
            chip_fill_logged = false;
            if (have_col) {
                for (int gz = 0; gz < chip_h; ++gz)
                    for (int gx = 0; gx < chip_w; ++gx) {
                        float g;
                        if (col.GetFloor(room_org[0] + (float(gx) + 0.5f) * 30.f,
                                         room_org[2] + (float(gz) + 0.5f) * 30.f,
                                         mcf::Collision::kFloorMask, &g)) {
                            chip_walk[size_t(gz) * size_t(chip_w) + size_t(gx)] = 1;
                            chip_height[size_t(gz) * size_t(chip_w) + size_t(gx)] = g;
                        }
                    }
            }
            {
                long w = 0;
                for (uint8_t v : chip_walk) w += v;
                lucent::info("ai", "chip grid {}x{}: {} of {} walkable",
                             chip_w, chip_h, w, chip_walk.size());
                // The height map itself, row by row, because the route table's
                // gate is entirely a function of it and a wrong floor probe
                // would otherwise look like a wall.
                for (int gz = 0; gz < chip_h; ++gz) {
                    lucent::Line l;
                    l.add("row {:2d}:", gz);
                    for (int gx = 0; gx < chip_w; ++gx) {
                        float hgt = chip_height[size_t(gz) * size_t(chip_w) + size_t(gx)];
                        if (hgt >= kChipNoFloor) l.add("     .");
                        else l.add(" {:5.0f}", hgt);
                    }
                    l.flush_debug("chip");
                }
            }

            std::vector<std::pair<int, int>> taken_chips;
            for (auto& a : world.actors_mutable()) {
                if (!a.random_place || !have_col) continue;
                constexpr float kChip = 30.f;
                float best[2]{0, 0};
                std::pair<int, int> best_chip{-1, -1};
                float best_d = 1e30f;
                bool found = false;
                float cx = room_org[0] + room_size.w * 0.5f;
                float cz = room_org[2] + room_size.h * 0.5f;
                // The scan covers the ROOM, whose size is not a constant --
                // a 330x270 room is 11x9 chips, not 10x8.
                const int chips_x = int(room_size.w / kChip);
                const int chips_z = int(room_size.h / kChip);
                for (int gz = 0; gz < chips_z; ++gz) {
                    for (int gx = 0; gx < chips_x; ++gx) {
                        float wx = room_org[0] + (float(gx) + 0.5f) * kChip;
                        float wz = room_org[2] + (float(gz) + 0.5f) * kChip;
                        float g;
                        if (!col.GetFloor(wx, wz, mcf::Collision::kFloorMask, &g)) continue;
                        // One actor per chip, or a room's NPCs all stack on the
                        // single chip nearest the centre.
                        if (std::find(taken_chips.begin(), taken_chips.end(),
                                      std::make_pair(gx, gz)) != taken_chips.end())
                            continue;
                        float d = (wx - cx) * (wx - cx) + (wz - cz) * (wz - cz);
                        if (d < best_d) {
                            best_d = d; best[0] = wx; best[1] = wz; found = true;
                            best_chip = {gx, gz};
                        }
                    }
                }
                if (found) {
                    taken_chips.push_back(best_chip);
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

            for (auto& a : world.actors_mutable()) {
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
                        // Write it BACK. This correction used to live only in
                        // the `placed` snapshot, so the actor itself kept the
                        // script's Y -- 30 units above the floor for these
                        // enemies -- and every consumer of a.pos (the live draw
                        // loop, and every combat volume) used the wrong height.
                        a.pos[1] = g - room_org[1];
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

            // AddEventBox's y0 == -1 is not a literal world height: the
            // engine probes the collision floor at registration and anchors
            // the box there. This must live in loadRoom, not its initial
            // caller, because mapjump loads another room through this lambda.
            if (have_col) {
                for (auto& bx : world.boxes) {
                    if (!bx.floor_y) continue;
                    const float x = (bx.lo[0] + bx.hi[0]) * .5f + room_org[0];
                    const float z = (bx.lo[2] + bx.hi[2]) * .5f + room_org[2];
                    float y = 0.f;
                    if (col.GetFloor(x, z, mcf::Collision::kFloorMask, &y))
                        bx.ResolveFloorY(y);
                }
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
            // The player's numbers are the engine's own: GameParameter::Init
            // sets a new game's state and ::Update derives the rest from it.
            // What is NOT modelled is the SAVE -- there is no load path, so
            // this is always a new game's level 1 and its granted equipment.
            mcf::PlayerStats ps;
            lucent::info("player", "level {}  HP {}/{}  MP {}/{}  {} GP  "
                         "power {} stamina {} wisdom {} will {}",
                         ps.level, ps.hp, ps.max_hp(), ps.mp, ps.max_mp(),
                         ps.money, ps.power, ps.stamina, ps.wisdom, ps.will);
            lucent::info("player", "attack {} (power {} + weapon {}), "
                         "defence {} (stamina {} + 1 + helm {} + armor {}), "
                         "{} EXP to level {}",
                         ps.attack(), mcf::PlayerStats::Cap(ps.power), ps.weapon,
                         ps.defence(), mcf::PlayerStats::Cap(ps.stamina),
                         mcf::EquipDefence(ps.helm), mcf::EquipDefence(ps.armor),
                         ps.next_exp(), ps.level + 1);
            // TEST HOOK, not a game mechanic: every enemy in the shipping
            // rooms can one-shot a level-1 player, so there is no honest way to
            // reach a level-up in a headless run. This credits EXP through the
            // engine's own AddEXP so the progression path can be exercised.
            if (grant_exp) {
                ps.AddExp(grant_exp);
                lucent::warn("player", "--grant-exp {}: {} EXP (test hook, not a "
                             "game mechanic)", grant_exp, ps.exp);
            }
            const int player_defence = ps.defence();
            // AppCharacterPlayer::DamageProcess refuses damage while an
            // invulnerability timer at +0x1ff0 is positive and reloads it from
            // +0x3aa0 after each hit. The MECHANISM is reversed; the duration
            // is a per-character field the port cannot source, so it is named.
            constexpr float kPlayerIFramesStopgap = 30.f;   // frames
            float player_iframes = 0.f;
            long player_damage_taken = 0, player_hits = 0;
            bool player_dead = false;
            // ModeGame::Process_GameOver's own three steps: 1 show the message,
            // 2 fade out over 800 ms once it is dismissed, 3 leave the mode.
            int game_over_state = 0;
            bool level_up_announced = false;
            // GameRandom @ 0x3da480 is NOT reversed, so this is a stand-in with
            // the same range contract, seeded fixed so a headless run is
            // reproducible. The SHAPE of the roll is the engine's; the sequence
            // is not.
            std::mt19937 rng(12345);
            auto game_random = [&rng](int n) {
                return std::uniform_int_distribution<int>(0, n - 1)(rng);
            };
            int player_attack = ps.attack();
            if (!mcf::FindWeapon(ps.weapon))
                lucent::warn("combat", "weapon {} not in tblWeapon; the player's "
                             "attack is the power stat alone", ps.weapon);
            seedCombat = [&] {
                if (auto* pl = world.Find("MainPlayer")) {
                    auto& av = pl->attack[0];
                    av.bone = "cog"; av.radius = 45.f; av.arc_deg = 180.f;
                    av.valid = false;
                    // The player must also be a TARGET, or enemy attack volumes
                    // have nothing to hit and combat stays one-sided.
                    auto& pdv = pl->damage[0];
                    pdv.bone = "y_ang"; pdv.radius = 15.f; pdv.valid = true;
                }
                int with_stats = 0, without = 0;
                for (auto& a : world.actors_mutable()) {
                    if (a.kind != 'E' && a.kind != 'B') continue;
                    auto& dv = a.damage[0];
                    dv.bone = "y_ang"; dv.radius = 15.f; dv.valid = true;
                    auto it = enemy_stats.find(a.type_id);
                    if (it == enemy_stats.end()) { ++without; continue; }
                    // Enemies attack too. The volume is always live because
                    // enemy attack timing is native code that is not reversed;
                    // the player's i-frame window is what keeps this from
                    // being a continuous damage stream, and that IS reversed.
                    // Radius/arc are an ATTESTED script configuration rather
                    // than a made-up number: ChrAttackBoneSize(my, 1, 20, 360)
                    // is the most common enemy setup in the shipping scripts.
                    auto& eav = a.attack[0];
                    eav.bone = "y_ang"; eav.radius = 20.f; eav.arc_deg = 360.f;
                    eav.valid = true;
                    a.attack_power = it->second.attack;
                    a.max_hp = it->second.max_hp;
                    a.hp = a.max_hp;
                    a.defence = it->second.defence;
                    a.exp = it->second.exp;
                    a.move_speed = it->second.move_speed;
                    a.ai_type = it->second.ai_type;
                    a.ai[0] = it->second.ai[0];
                    a.ai[1] = it->second.ai[1];
                    a.has_ai = true;
                    a.money = it->second.money;
                    ++with_stats;
                }
                if (with_stats || without)
                    lucent::info("combat", "enemy stats: {} from enemydat.bin, "
                                 "{} with no table entry", with_stats, without);
            };
            seedCombat();

            std::string last_warned_message;
            bool confirm_prev = false;
            bool menu_up_prev = false, menu_down_prev = false;
            bool level_up_open = false;
            int level_up_choice = 0;
            float eye_cur[3]{};
            bool cam_init = false;
            bool running = true;
            // The engine's mode machine. The port reaches gameplay through the
            // same chain the binary does -- ModeInit -> ModeCESA ->
            // ModeMakerLogo -> ModeTitle -> ModeGame -- rather than starting in
            // a room. It is OPT-IN (--boot) for now because the two splash
            // modes draw nothing: the logo artwork has not been located in the
            // archive, so running them by default would replace six seconds of
            // gameplay with six seconds of black and call it progress.
            mcf::ModeMachine modes;
            bool title_bgm = false;
            mcf::TitleMenu title;
            if (title_phase == "menu" || title_phase == "names" ||
                title_phase == "crawl")
                title.phase = mcf::TitleMenu::Phase::kMenu;
            bool title_press = false;   // "any button" on the attract screen
            // Which menu items this port can actually act on. Continue and Load
            // Game both need a save file, and the save format past the
            // inventory is not reversed, so there is nothing to load: they are
            // listed and dimmed rather than offered and then ignored.
            auto titleEnabled = [](int item) { return item == 0; };
            // Name entry: the engine asks for two, hero then heroine
            // (SYS_NAMEENTRY_HERO_NAME_TITLE / _GIRL_NAME_TITLE).
            // The crawl runs BEFORE name entry: New Game's setup @ 0x306df0
            // loads the lines, then SetNextSubMode(10) @ 0x307bc8 plays them.
            mcf::OpeningCrawl crawl;
            std::vector<std::string> crawl_lines;
            bool crawling = false;
            // Only --title-phase crawl (or the normal New Game path) plays it.
            bool crawl_done = !title_phase.empty() && title_phase != "crawl";
            bool naming = title_phase == "names";
            bool naming_done = false;
            if (naming || title_phase == "crawl")
                title.chosen = true;               // as New Game would
            int  name_field = 0;              // 0 = hero, 1 = heroine
            std::string names[2];
            int  name_err = mcf::NameEntry::kOk;
            if (!boot_chain) modes = {mcf::Mode::kGame, mcf::Mode::kNone, 0};
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
                // Overlaps the engine's faction filter rejects. Reported, not
                // silently dropped: if this were the whole hit count, combat
                // would look "working" while nothing could ever land.
                long blocked_by_faction = 0;
                long atk_no_model = 0, def_no_model = 0, def_no_bone = 0;
                long pairs_vs_player = 0;   // enemy attack volume vs the player
                float closest_vs_player = 1e30f, closest_xz = 0, closest_y = 0;
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
                //
                // This is clamped HERE, before the boot chain, not after it:
                // the title's opening crawl scrolls in real milliseconds, and
                // with a raw uncapped dt it advanced ~0.03 units a frame and
                // never came on screen. The same bug the comment above
                // describes, one scope higher.
                if (fixed_step) dt = 1.f / 30.f;
                if (boot_chain) {
                    // 60fps is the engine's own rate --
                    // MainProcess::Initialize calls SetFrameParSecond(60).
                    modes.Step(dt * 60.f);
                    if (modes.current == mcf::Mode::kTitle && !title_bgm) {
                        // ModeTitle's state 3 @0x3070d4 calls GameBgmPlay(1),
                        // so the title theme is track 1 -- the engine's number,
                        // not a guess.
                        title_bgm = true;
                        sc.pending_bgm = 1;
                        serviceAudio();
                    }
                    if (modes.current == mcf::Mode::kTitle) {
                        // ModeTitle::Process @ 0x3070bc advances on the
                        // player's CHOICE, not a timer, so the port waits for
                        // one too. The attract screen comes first; any button
                        // opens the three-item menu that ModeTitle::Render
                        // builds from the id table at 0xbd354.
                        if (title.phase == mcf::TitleMenu::Phase::kAttract) {
                            // A headless run has nobody to press a button, but
                            // the attract screen is a screen the player really
                            // sees, so hold it for the engine's own 1000ms
                            // prompt period rather than blinking past it.
                            bool headless_done =
                                (warmup > 0 || auto_advance) && modes.frames >= 60;
                            if (title_press || headless_done) {
                                title.phase = mcf::TitleMenu::Phase::kMenu;
                                title_press = false;
                                lucent::info("title", "menu: {} items",
                                             mcf::TitleMenu::kItemCount);
                            }
                        } else if (title.chosen && !crawl_done) {
                            if (!crawling) {
                                crawling = true;
                                // Load the ids the engine loads, and stop where
                                // it stops: at the first EMPTY string. The
                                // count is data, not a constant.
                                for (int i = 0; i < mcf::OpeningCrawl::kIdCount; ++i) {
                                    const std::string* l = strings.Find(
                                        std::format("SYS_TITLE_OPENING_{}", i));
                                    if (!l || l->empty()) break;
                                    crawl_lines.push_back(*l);
                                }
                                lucent::info("title", "opening crawl: {} of {} ids "
                                             "are non-empty", crawl_lines.size(),
                                             mcf::OpeningCrawl::kIdCount);
                                if (crawl_lines.empty()) {
                                    lucent::warn("title", "no opening lines "
                                                 "resolved; skipping the crawl");
                                    crawl_done = true;
                                }
                            }
                            if (crawling && !crawl_done) {
                                const bool* ks = SDL_GetKeyboardState(nullptr);
                                // Headless skips at the engine's own fast
                                // rate rather than teleporting past the crawl,
                                // so the screen is still observable.
                                crawl.skipping = (ks && ks[SDL_SCANCODE_LSHIFT]) ||
                                                 ((warmup > 0 || auto_advance) &&
                                                  title_phase != "crawl");
                                if (crawl.Step(dt * 1000.f, int(crawl_lines.size())))
                                    crawl_done = true;
                            }
                        } else if (title.chosen && !naming_done) {
                            // New Game asks for the two names before it starts.
                            // Only New Game is reachable -- see titleEnabled.
                            // oG[0x28c0] initialises to -1 (ApplicationGlobal's
                            // ctor @ 0x2c07e8) and -1 IS "new game", so this
                            // matches the engine's own default.
                            if (!naming) {
                                naming = true;
                                lucent::info("name", "entry: max {} code points",
                                             lang == "ja" ? mcf::NameEntry::kMaxJa
                                                          : mcf::NameEntry::kMaxOther);
                            }
                            if ((warmup > 0 || auto_advance) &&
                                title_phase != "names") {
                                // Headless: no keyboard. Take the defaults so
                                // the boot path still reaches the game, and say
                                // that the names were not typed.
                                // The engine's defaults, copied into
                                // GameParameter+0x8 and +0x88 @ 0x306e54.
                                const std::string* dh =
                                    strings.Find("SYS_DEFAULTNAME_HERO");
                                const std::string* dg =
                                    strings.Find("SYS_DEFAULTNAME_GIRL");
                                names[0] = dh ? *dh : std::string("Sumo");
                                names[1] = dg ? *dg : std::string("Fuji");
                                lucent::info("name", "headless: using defaults "
                                             "'{}' and '{}' (nobody typed one)",
                                             names[0], names[1]);
                                naming_done = true;
                            }
                        } else if (title.chosen) {
                            modes.next = mcf::Mode::kGame;
                        } else if (warmup > 0 || auto_advance) {
                            // Headless: take the engine's default immediately.
                            title.Confirm(titleEnabled);
                        }
                    }
                    if (modes.current != mcf::Mode::kGame) {
                        SDL_Event ev;
                        while (SDL_PollEvent(&ev)) {
                            if (ev.type == SDL_EVENT_QUIT) running = false;
                            if (ev.type != SDL_EVENT_KEY_DOWN || ev.key.repeat) continue;
                            // Edge-triggered: a menu must not scroll once per
                            // frame while a key is held.
                            SDL_Keycode k = ev.key.key;
                            if (naming && !naming_done) {
                                // Typed with the real keyboard; the engine's
                                // own character set decides what is legal, so
                                // nothing is filtered here -- an illegal key
                                // produces the engine's own error message
                                // instead of being silently swallowed.
                                if (k == SDLK_BACKSPACE) {
                                    if (!names[name_field].empty())
                                        names[name_field].pop_back();
                                    name_err = mcf::NameEntry::kOk;
                                } else if (k == SDLK_RETURN) {
                                    const std::string* use =
                                        strings.Find("SYS_NAMEENTRY_USE");
                                    const std::string* nul =
                                        strings.Find("SYS_COMMON_NULLSPACE");
                                    name_err = use
                                        ? mcf::NameEntry::Validate(
                                              names[name_field], *use,
                                              nul ? *nul : std::string(),
                                              lang == "ja")
                                        : mcf::NameEntry::kOk;
                                    if (name_err == mcf::NameEntry::kOk) {
                                        if (name_field == 0) name_field = 1;
                                        else naming_done = true;
                                    }
                                } else if (k == SDLK_TAB) {
                                    name_field ^= 1;
                                    name_err = mcf::NameEntry::kOk;
                                } else if (k >= 32 && k < 127) {
                                    names[name_field].push_back(char(k));
                                    name_err = mcf::NameEntry::kOk;
                                }
                                if (k == SDLK_ESCAPE) running = false;
                                continue;
                            }
                            if (modes.current == mcf::Mode::kTitle &&
                                title.phase == mcf::TitleMenu::Phase::kMenu) {
                                if (k == SDLK_DOWN || k == SDLK_S) title.Down();
                                else if (k == SDLK_UP || k == SDLK_W) title.Up();
                                else if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_Z)
                                    title.Confirm(titleEnabled);
                            } else if (k != SDLK_ESCAPE) {
                                title_press = true;   // "any button"
                            }
                            if (k == SDLK_ESCAPE) running = false;
                        }
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        // ModeCESA's art (cesa.png) is NOT in this archive --
                        // 0 hits in all 9886 entries, and no PNG in the assets
                        // root either. Its screen stays black on purpose, and
                        // the log says so once rather than pretending.
                        if (modes.current == mcf::Mode::kMakerLogo)
                            drawSprite(sprMaker);
                        else if (modes.current == mcf::Mode::kTitle) {
                            // Name entry is its own screen, not an overlay on
                            // the logo.
                            // Name entry and the crawl are their own
                            // screens, not overlays on the logo.
                            if ((!naming || naming_done) &&
                                (!crawling || crawl_done)) drawSprite(sprTitle);
                            // Every word here is the game's own, resolved
                            // through the shipping string table.
                            auto say = [&](const char* id) {
                                // Echo the id when it is missing, so a wrong id
                                // shows on screen instead of drawing nothing.
                                const std::string* s = strings.Find(id);
                                return s ? *s : std::string(id);
                            };
                            const float cx = float(W) * 0.5f;
                            const float kScale = 2.f;                // layout, as the HUD uses
                            const float kGlyph = kScale * font_scale;
                            const float sc = kGlyph * 1.4f;
                            // The copyright belongs to the title screen
                            // proper, not to the crawl or the name screen.
                            if ((!crawling || crawl_done) && (!naming || naming_done))
                                drawUiText(say(mcf::TitleMenu::kCopyrightId),
                                           cx, float(H) - 22.f * kScale, kGlyph,
                                           true, 1.f, 1.f, 1.f, 0.75f);
                            if (crawling && !crawl_done) {
                                // App space is 544 tall; scale to the window so
                                // the engine's own offsets stay meaningful.
                                const float k = float(H) / 544.f;
                                for (size_t i = 0; i < crawl_lines.size(); ++i) {
                                    float ay = crawl.scroll +
                                               mcf::OpeningCrawl::kFirstY +
                                               mcf::OpeningCrawl::kLineStep * float(i);
                                    float a = mcf::OpeningCrawl::Alpha(ay);
                                    if (a <= 0.f) continue;
                                    float y = ay * k;
                                    // Shadow then body, as Render does (0x40
                                    // then 0xf0).
                                    drawUiText(crawl_lines[i], cx + 2.f, y + 2.f,
                                               sc, true, 0.25f, 0.25f, 0.25f, a);
                                    drawUiText(crawl_lines[i], cx, y, sc, true,
                                               0.94f, 0.94f, 0.94f, a);
                                }
                                drawUiText(say(mcf::OpeningCrawl::kSkipId) +
                                               "  [Shift]",
                                           cx, float(H) - 40.f,
                                           kGlyph, true, 1, 1, 1, 0.7f);
                            } else if (naming && !naming_done) {
                                // Every word here is the game's own.
                                drawUiText(say("SYS_NAMEENTRY_INFO_1"),
                                           cx, float(H) * 0.30f, kGlyph, true,
                                           1, 1, 1, 1);
                                drawUiText(say("SYS_NAMEENTRY_INFO_2"),
                                           cx, float(H) * 0.35f, kGlyph, true,
                                           1, 1, 1, 0.8f);
                                const char* lbl[2] = {
                                    "SYS_NAMEENTRY_HERO_NAME_TITLE",
                                    "SYS_NAMEENTRY_GIRL_NAME_TITLE"};
                                float y = float(H) * 0.50f;
                                for (int f = 0; f < 2; ++f) {
                                    bool sel = f == name_field;
                                    drawUiText(say(lbl[f]) + ":",
                                               cx - 120.f * kScale, y, sc, false,
                                               1, 1, sel ? 0.55f : 1.f, 1);
                                    // A caret on the field being typed, so an
                                    // empty field still shows where input goes.
                                    std::string shown = names[f];
                                    if (sel && (modes.frames / 20) % 2) shown += "_";
                                    drawUiText(shown, cx + 10.f * kScale, y, sc,
                                               false, 1, 1, 1, 1);
                                    y += 26.f * kScale;
                                }
                                if (const char* eid =
                                        mcf::NameEntry::ErrorId(name_err))
                                    drawUiText(say(eid), cx, float(H) * 0.68f,
                                               kGlyph, true, 1.f, 0.45f, 0.4f, 1.f);
                                drawUiText(say("SYS_NAMEENTRY_BUTTON_DECIDE") +
                                               "  [Enter]   Tab: switch",
                                           cx, float(H) * 0.78f, kGlyph, true,
                                           1, 1, 1, 0.7f);
                            } else if (title.phase == mcf::TitleMenu::Phase::kAttract) {
                                // The engine fades this prompt (LerpL over
                                // 1000ms @ 0x3084c8); the port pulses it on the
                                // same 1s period rather than inventing a rate.
                                float t = float(modes.frames % 60) / 60.f;
                                float a = 0.35f + 0.65f * std::fabs(1.f - 2.f * t);
                                drawUiText(say(mcf::TitleMenu::kStartId),
                                           cx, float(H) * 0.74f, sc, true, 1, 1, 1, a);
                            } else {
                                float y = float(H) * 0.64f;
                                for (int i = 0; i < mcf::TitleMenu::kItemCount; ++i) {
                                    bool on = titleEnabled(i);
                                    bool sel = i == title.cursor;
                                    // Unavailable items are still SHOWN -- the
                                    // engine lists them -- but dimmed, so the
                                    // menu does not lie about what it has.
                                    float v = on ? 1.f : 0.4f;
                                    if (sel && on) v = 1.f;
                                    drawUiText(say(mcf::TitleMenu::kItemId[i]),
                                               cx, y, sc, true,
                                               v, v, sel ? 0.55f * v : v, on ? 1.f : 0.7f);
                                    if (sel)
                                        drawUiText(">", cx - 90.f * kScale, y, sc, false,
                                                   1.f, 0.85f, 0.35f, 1.f);
                                    y += 20.f * kScale;
                                }
                            }
                        }
                        // --shot-mode NAME captures this screen and exits, so
                        // "the logo draws" can be checked on real pixels rather
                        // than asserted. Without it the splash is invisible to
                        // --screenshot, which counts gameplay frames only.
                        if (!shot.empty() && shot_mode == mcf::ModeName(modes.current) &&
                            modes.frames >= shot_delay) {
                            std::vector<uint8_t> px(size_t(W) * H * 4);
                            glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                            std::vector<uint8_t> fl(px.size());
                            for (int y = 0; y < H; ++y)
                                std::memcpy(&fl[size_t(y) * W * 4],
                                            &px[size_t(H - 1 - y) * W * 4], size_t(W) * 4);
                            WritePng(shot, W, H, fl);
                            lucent::info("host", "wrote {} during {}", shot,
                                         mcf::ModeName(modes.current));
                            running = false;
                        }
                        SDL_GL_SwapWindow(win);
                        continue;
                    }
                }
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
                // Dismiss a message with the same key that attacks; while one
                // is up that key must NOT also swing, or every conversation
                // would be punctuated by sword strokes.
                // The level-up screen. Everything behind it is the engine's --
                // CheckLevelUp's conditions, tblLevelup's four rows, the full
                // heal -- and the one thing the game hands to the PLAYER is the
                // choice, so the port asks rather than picking.
                if (!level_up_open && ps.level_up_due() && !sc.message_pending) {
                    level_up_open = true;
                    level_up_choice = 0;
                    lucent::info("player", "level {} available: choose a regimen",
                                 ps.level + 1);
                }
                bool confirm = key && (key[SDL_SCANCODE_SPACE] ||
                                       key[SDL_SCANCODE_Z] ||
                                       key[SDL_SCANCODE_RETURN]);
                bool confirm_edge = confirm && !confirm_prev;
                confirm_prev = confirm;
                if (level_up_open) {
                    bool up = key && (key[SDL_SCANCODE_UP] || key[SDL_SCANCODE_W]);
                    bool down = key && (key[SDL_SCANCODE_DOWN] || key[SDL_SCANCODE_S]);
                    if (up && !menu_up_prev) level_up_choice = (level_up_choice + 3) % 4;
                    if (down && !menu_down_prev) level_up_choice = (level_up_choice + 1) % 4;
                    menu_up_prev = up; menu_down_prev = down;
                    // The screen stops the world: no walking, no swinging.
                    mx = mz = 0;
                    if (confirm_edge || auto_levelup >= 0) {
                        int pick = auto_levelup >= 0 ? auto_levelup : level_up_choice;
                        int before_hp = ps.hp;
                        ps.LevelUp(pick);
                        level_up_open = false;
                        lucent::info("player", "{} -> level {}, HP {}->{}/{}, "
                                     "power {} stamina {} wisdom {} will {}, "
                                     "attack {} defence {}",
                                     mcf::PlayerStats::RegimenName(pick), ps.level,
                                     before_hp, ps.hp, ps.max_hp(), ps.power,
                                     ps.stamina, ps.wisdom, ps.will,
                                     ps.attack(), ps.defence());
                    }
                } else if (sc.message_pending) {
                    if (confirm_edge || auto_advance) {
                        sc.message_pending = false;
                        sc.last_message.clear();
                    }
                } else if (confirm_edge || (auto_talk && frames % 30 == 0)) {
                    // Talking. A room script spawns an NPC with a handle and
                    // defines a global of the SAME NAME as its conversation:
                    //   npc_rand("BATTLEMAN", eNPC.BATTLEMAN, 6)
                    //   function BATTLEMAN() talk_on(...) msgId(...) ... end
                    // So "talk to the nearest NPC" is "start the coroutine named
                    // after it". StartCoroutine already refuses a name that is
                    // not a global function, which is what keeps NPCs with no
                    // conversation silent.
                    //
                    // PORT CHOICE: the reach. The engine's own talk trigger is
                    // not reversed, so this uses one chip (30 units), the game's
                    // fundamental spatial unit, rather than an invented number.
                    constexpr float kTalkReach = 30.f;
                    const mcf::Actor* best = nullptr;
                    float best_d = kTalkReach * kTalkReach;
                    for (const auto& a : world.actors()) {
                        if (!a.alive || a.kind != 'N' || a.handle == "MainPlayer") continue;
                        float dx = a.pos[0] + room_org[0] - px;
                        float dz = a.pos[2] + room_org[2] - pz;
                        float d2 = dx * dx + dz * dz;
                        if (d2 < best_d) { best_d = d2; best = &a; }
                    }
                    if (best) {
                        if (sc.StartCoroutine(best->handle))
                            lucent::info("world", "talking to '{}' ({:.0f} units away)",
                                         best->handle, std::sqrt(best_d));
                        else
                            lucent::debug("world", "'{}' has no conversation: {}",
                                          best->handle, sc.last_error());
                    }
                }
                bool attacking = attack_left > 0.f;
                if (combat_demo && !attacking && frames > 30) {
                    attack_left = kAttackFrames;   // swing continuously, for testing
                    attacking = true;
                }
                if (auto_attack && !attacking && !level_up_open) {
                    attack_left = kAttackFrames; attacking = true;
                }
                if (confirm && !sc.message_pending && !level_up_open && !attacking) {
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
                if (player_iframes > 0.f) player_iframes -= dt * 30.f;
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

                // The chip distance field, rebuilt once per frame from the
                // player's chip and shared by every chaser. MakeRouteTable
                // @ 0x2a7ef4 allocates chips_w * chips_h int32s, memsets them
                // to ZERO, and treats a still-zero cell as unreached -- so the
                // field is 1-based and needs no separate visited map. Same here.
                if (chip_w > 0 && chip_h > 0) {
                    chip_dist.assign(size_t(chip_w) * size_t(chip_h), 0);
                    int pcx = int(std::floor((px - room_org[0]) / 30.f));
                    int pcz = int(std::floor((pz - room_org[2]) / 30.f));
                    if (pcx >= 0 && pcz >= 0 && pcx < chip_w && pcz < chip_h) {
                        // The engine's height gate, now that it is read
                        // correctly: |height[chip] - height[GOAL]| >= 5
                        // rejects, where the goal is the fixed cell the fill
                        // was started for -- here the player's chip. So a
                        // ledge five units off the player's floor is not
                        // reachable however many chips lead to it.
                        const float goal_h =
                            chip_height[size_t(pcz) * size_t(chip_w) + size_t(pcx)];
                        long band_reject = 0;   // chips the height gate refused
                        std::vector<int> q;
                        q.push_back(pcz * chip_w + pcx);
                        chip_dist[size_t(q[0])] = 1;
                        for (size_t h = 0; h < q.size(); ++h) {
                            int c = q[h], cx2 = c % chip_w, cz2 = c / chip_w;
                            int nd = chip_dist[size_t(c)] + 1;
                            static const int kDx[4] = {1, -1, 0, 0};
                            static const int kDz[4] = {0, 0, 1, -1};
                            for (int k = 0; k < 4; ++k) {
                                int nx = cx2 + kDx[k], nz = cz2 + kDz[k];
                                if (nx < 0 || nz < 0 || nx >= chip_w || nz >= chip_h) continue;
                                size_t ni = size_t(nz) * size_t(chip_w) + size_t(nx);
                                if (!chip_walk[ni] || chip_dist[ni]) continue;
                                if (std::fabs(chip_height[ni]) > kChipNoFloor - 1.f)
                                    continue;
                                if (std::fabs(chip_height[ni] - goal_h) >= kChipStepLimit) {
                                    ++band_reject;
                                    continue;
                                }
                                chip_dist[ni] = nd;
                                q.push_back(int(ni));
                            }
                        }
                        // Reported once per room, with its denominators, so a
                        // zero here says "the gate ran and refused nothing"
                        // rather than "the gate never ran".
                        if (!chip_fill_logged) {
                            chip_fill_logged = true;
                            long walk = 0;
                            for (uint8_t v : chip_walk) walk += v;
                            lucent::info("ai",
                                "chip fill from ({},{}) h={:.1f}: reached {} of "
                                "{} walkable / {} chips, height gate refused {}",
                                pcx, pcz, goal_h, q.size(), walk,
                                chip_walk.size(), band_reject);
                        }
                    }
                }

                // Enemies close on the player.
                for (auto& a : world.actors_mutable()) {
                    if (!a.alive || (a.kind != 'E' && a.kind != 'B')) continue;
                    float ax = a.pos[0] + room_org[0], az = a.pos[2] + room_org[2];
                    float dx = px - ax, dz = pz - az;
                    float d2 = dx * dx + dz * dz;
                    if (d2 < 1.f || d2 > 200.f * 200.f) continue;
                    float d = std::sqrt(d2);
                    // Stop inside the enemy's own reach instead of at a magic
                    // constant. The previous 40 predated enemies being able to
                    // attack at all and left them permanently 49 units from the
                    // player -- outside a 20+15 reach, so they closed in and
                    // then stood there harmlessly.
                    float reach = 15.f;   // the player's damage sphere
                    if (auto it = a.attack.find(0); it != a.attack.end())
                        reach += it->second.radius;
                    if (d < reach * 0.7f) { a.motion = kMotionWait; continue; }
                    // The enemy's OWN speed, from enemydat +0x68 -- 12 and 24
                    // units/s dominate, against the invented 30 this replaces.
                    // Nine of the 107 enemies have 0 and simply do not move.
                    if (a.move_speed <= 0.f) { a.motion = kMotionWait; continue; }

                    // --- The AI state machine ---------------------------------
                    // RE-VERIFIED: the countdown, the weighted transition and
                    // the per-state durations are the engine's, from this
                    // enemy's own enemydat record. UpdateAI decrements the timer
                    // once per call and rolls a new state when it expires.
                    if (a.has_ai) {
                        a.ai_timer -= dt * 30.f;          // frames, at 30fps
                        if (a.ai_timer <= 0.f) {
                            const auto& m = a.ai[a.ai_record];
                            const int prev_state = a.ai_state;
                            const auto& cur = m.state[a.ai_state];
                            int32_t sum = cur.weight_sum();
                            if (sum >= 1)
                                a.ai_state = mcf::NextAiState(m, a.ai_state,
                                                              game_random(sum));
                            if (a.ai_state != prev_state) {
                                lucent::debug("ai", "{} (id {}) state {} -> {} "
                                              "after its timer ran out",
                                              a.handle, a.type_id, prev_state,
                                              a.ai_state);
                                a.has_wander = false;   // re-pick on re-entry
                            }
                            const auto& now = m.state[a.ai_state];
                            a.ai_timer = float(now.base) +
                                         (now.range > 0 ? float(game_random(now.range)) : 0.f);
                            // A machine with no durations at all would spin here
                            // every frame; the corpus has none, but a zero timer
                            // is the one value that must not be trusted silently.
                            if (a.ai_timer <= 0.f) {
                                a.ai_timer = 1.f;
                                lucent::debug("ai", "enemy {} state {} has a zero "
                                              "duration; holding one frame",
                                              a.handle, a.ai_state);
                            }
                        }
                        // STATE 2 is the pursue state, and this is the engine's
                        // answer rather than a port choice now. The state
                        // dispatch @ 0x2a96d8 sends state 2 to 0x2a9748, which
                        // calls vtable[0x428] = UpdateAI_TargetChr() and then
                        // walks the route tables at actor +0x3768 / +0x3770.
                        // UpdateAI_TargetChr @ 0x2a8348 tests the caller's own
                        // GetType(), and for type 4 (AppCharacterEnemy) it
                        // SearchNears type 1 (player) and type 2 (party) and
                        // takes the nearer. This replaces the previous choice of
                        // state 0, which was reasoned from state 0 being the
                        // reset state. That reasoning was wrong.
                        //
                        // What the other three states do, from the same
                        // dispatch:
                        //   state 0 @ 0x2a9f28  IDLE. It zeroes the movement
                        //     vector (`movi v0.2d, #0`) and forces the idle
                        //     motion -- 0 normally, 15 on floor type 1. The
                        //     kMotionWait below matches it.
                        //   state 1 @ 0x2a9ef0  WANDERS -- implemented below.
                        //   state 3 @ 0x2a9700  calls UpdateAI_TargetPos, which
                        //     SearchNears type 4 -- other ENEMIES -- so it is
                        //     spacing, not chase. Not implemented; idles.
                        if (a.ai_state == 1) {
                            // The wander destination picker, from 0x2aa624.
                            // The engine works in CHIPS: it takes the actor's
                            // chip, offers a candidate in [-4, +4] columns and
                            // [-3, +3] rows, and retries up to 126 times until
                            // one is close enough and reachable. The accepted
                            // chip's CENTRE -- (chip + 0.5) * 30 -- becomes the
                            // target.
                            //
                            // The engine's distance gate is `dist <= s10`, a
                            // per-mode radius of 4, 5 or 6 chips. This port has
                            // no mode word, but the gate is mostly inert: the
                            // widest candidate in a +-4 x +-3 window is
                            // sqrt(16+9) = 5 chips, so it can only ever reject
                            // anything when s10 is 4, and then only the far
                            // corners. It is left out rather than guessed at,
                            // and this comment is the record of that.
                            //
                            // Reachability: the engine calls _MakeRouteTable and
                            // requires a route. The port has no route table, so
                            // it requires the destination to be on the floor --
                            // weaker (it cannot see a wall between here and
                            // there), and marked as the approximation it is.
                            if (!a.has_wander) {
                                constexpr float kChip = 30.f;   // the game's spatial unit
                                const float cc = std::floor(a.pos[0] / kChip);
                                const float cr = std::floor(a.pos[2] / kChip);
                                for (int tries = 0; tries < 126; ++tries) {
                                    float tc = cc - 4.f + float(game_random(9));
                                    float tr = cr - 3.f + float(game_random(7));
                                    float lx = (tc + 0.5f) * kChip;
                                    float lz = (tr + 0.5f) * kChip;
                                    float g = 0.f;
                                    if (have_col &&
                                        !col.GetFloor(lx + room_org[0], lz + room_org[2],
                                                      mcf::Collision::kFloorMask, &g))
                                        continue;
                                    a.wander[0] = lx;
                                    a.wander[1] = lz;
                                    a.has_wander = true;
                                    break;
                                }
                                if (!a.has_wander)
                                    lucent::debug("ai", "{} found no reachable "
                                                  "wander chip in 126 tries",
                                                  a.handle);
                            }
                            if (a.has_wander) {
                                float wx = a.wander[0] - a.pos[0];
                                float wz = a.wander[1] - a.pos[2];
                                float wd = std::sqrt(wx * wx + wz * wz);
                                if (wd < 2.f) { a.has_wander = false; }
                                else {
                                    float st = a.move_speed * dt;
                                    a.pos[0] += wx / wd * st;
                                    a.pos[2] += wz / wd * st;
                                    a.rot_y = std::atan2(wx, wz);
                                    a.motion = kMotionWalk;
                                }
                            }
                            continue;
                        }
                        if (a.ai_state != 2) { a.motion = kMotionWait; continue; }
                    }
                    // Chase along the distance field when there is one, exactly
                    // as state 2 does @ 0x2a9d44: find my chip, step to the
                    // neighbour whose distance is one LESS. Falls back to the
                    // straight line when the field has no route -- off-grid, or
                    // a room with no collision.
                    float tx = dx, tz = dz;
                    if (!chip_dist.empty()) {
                        int mcx = int(std::floor(a.pos[0] / 30.f));
                        int mcz = int(std::floor(a.pos[2] / 30.f));
                        if (mcx >= 0 && mcz >= 0 && mcx < chip_w && mcz < chip_h) {
                            int32_t here = chip_dist[size_t(mcz) * size_t(chip_w) + size_t(mcx)];
                            if (here > 1) {
                                static const int kDx[4] = {1, -1, 0, 0};
                                static const int kDz[4] = {0, 0, 1, -1};
                                for (int k = 0; k < 4; ++k) {
                                    int nx = mcx + kDx[k], nz = mcz + kDz[k];
                                    if (nx < 0 || nz < 0 || nx >= chip_w || nz >= chip_h) continue;
                                    if (chip_dist[size_t(nz) * size_t(chip_w) + size_t(nx)]
                                        != here - 1) continue;
                                    tx = (float(nx) + 0.5f) * 30.f - a.pos[0];
                                    tz = (float(nz) + 0.5f) * 30.f - a.pos[2];
                                    break;
                                }
                            }
                        }
                    }
                    float tl = std::sqrt(tx * tx + tz * tz);
                    if (tl < 1e-4f) { tx = dx; tz = dz; tl = d; }
                    float step = a.move_speed * dt;
                    a.pos[0] += tx / tl * step;
                    a.pos[2] += tz / tl * step;
                    a.rot_y = std::atan2(tx, tz);
                    a.motion = kMotionWalk;
                }

                // Game over, steps 2 and 3. The engine fades for 800 ms
                // (SetDispFade(1, 0x320, 0, 0, 0)) once the message is gone,
                // then SetNextMode(5). The port has no mode system and no title
                // screen, so step 3 ends the run and says exactly that instead
                // of inventing a screen.
                if (game_over_state == 1 && !sc.message_pending) {
                    game_over_state = 2;
                    world.fade.kind = 1;
                    world.fade.duration_ms = 800;
                    world.fade.remaining_ms = 800;
                    world.fade.colour[0] = world.fade.colour[1] = world.fade.colour[2] = 0;
                } else if (game_over_state == 2 && world.fade.remaining_ms <= 0.f) {
                    game_over_state = 3;
                    // ModeGame::Process_GameOver @ 0x2dea58 calls
                    // SetNextMode(5), and mode 5 is ModeTitle -- so the engine
                    // returns to the TITLE here, it does not end the program.
                    // An earlier note called 5 "a mode this port does not
                    // have"; it is named now, and the port follows the engine
                    // by handing back to the mode machine.
                    modes.next = mcf::Mode::kTitle;
                    lucent::info("world", "game over: SetNextMode(ModeTitle), "
                                 "as the engine does");
                    running = false;
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
                    bool in = bx.IsHit(px, py, pz, room_org[0], room_org[2]);
                    if (in && !bx.inside) {
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
                                    float sep = std::sqrt(sx * sx + sy * sy + sz * sz);
                                    cs.closest = std::min(cs.closest, sep);
                                    if (acts[didx].handle == "MainPlayer") {
                                        ++cs.pairs_vs_player;
                                        if (sep < cs.closest_vs_player) {
                                            cs.closest_vs_player = sep;
                                            cs.closest_xz = std::sqrt(sx * sx + sz * sz);
                                            cs.closest_y = std::fabs(sy);
                                        }
                                    }
                                    if (!mcf::HitArcSphere(ap, av.radius, av.arc_deg,
                                                           acts[aidx].rot_y, dp, dv.radius))
                                        continue;
                                    ++cs.hits;

                                    auto& atkA = acts[aidx];
                                    // Both Damage overrides test the ATTACKER's
                                    // GetType before anything else, so a hit
                                    // between two actors of the same side is
                                    // never damage. Without this, enemies fought
                                    // each other -- and did it with the PLAYER's
                                    // attack value, because the branch below
                                    // assumed any non-player defender had been
                                    // hit by the player.
                                    if (!mcf::CanDamage(atkA, acts[didx])) {
                                        ++cs.blocked_by_faction;
                                        continue;
                                    }
                                    // The player is gated by the engine's OWN
                                    // rule instead: AppCharacterPlayer::Damage
                                    // Process refuses while the i-frame timer
                                    // is positive. The per-swing dedup below is
                                    // for discrete swings, which enemies do not
                                    // have here (no attack animation is driven),
                                    // so applying it to them let each enemy hit
                                    // the player exactly once, ever.
                                    if (acts[didx].handle == "MainPlayer") {
                                        if (player_iframes > 0.f) continue;
                                        int dmg = std::max(1, atkA.attack_power -
                                                              player_defence);
                                        player_damage_taken += dmg;
                                        ++player_hits;
                                        ++cs.landed;
                                        player_iframes = kPlayerIFramesStopgap;
                                        // A real pool now: GameParameter::Update
                                        // clamps HP into [0, max_hp], which is
                                        // what this mirrors.
                                        ps.hp = std::max(0, ps.hp - dmg);
                                        lucent::info("combat",
                                            "{} hits MainPlayer for {} ({} atk - {} def); "
                                            "HP {}/{} after {} hits",
                                            atkA.handle, dmg, atkA.attack_power,
                                            player_defence, ps.hp, ps.max_hp(),
                                            player_hits);
                                        if (ps.hp == 0 && !player_dead) {
                                            player_dead = true;
                                            // ModeGame::Process_GameOver @
                                            // 0x2de964, step 1 of 3: the
                                            // game-over BGM, then the game's own
                                            // SYS_GAMEOVER_MSG in the message
                                            // window. In Japanese that line is
                                            // "@H は 力尽きた…", so it needs the
                                            // control-code expansion to name the
                                            // player at all.
                                            game_over_state = 1;
                                            sc.pending_bgm = 3;
                                            const std::string* t =
                                                sc.strings ? sc.strings->Find("SYS_GAMEOVER_MSG")
                                                           : nullptr;
                                            sc.last_message = sc.FormatText(
                                                t ? *t : std::string("SYS_GAMEOVER_MSG"));
                                            sc.message_pending = true;
                                            std::string shown;
                                            for (char c : sc.last_message)
                                                { if (c == '\n') shown += "\\n"; else shown += c; }
                                            lucent::info("combat", "MainPlayer is down "
                                                "(0 HP after {} damage): \"{}\"",
                                                player_damage_taken, shown);
                                        }
                                        continue;
                                    }
                                    // One hit per swing per target. The volume is
                                    // live for several frames, and without this a
                                    // single swing applied its damage every frame.
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
                                    // AppCharacterEnemy::Damage's real formula
                                    // (mcf::ComputeDamage), not a subtraction:
                                    // the magical attack is added, the charge
                                    // meter scales the result, and a 0..24%
                                    // roll is added on top. The floor at 1 is
                                    // the ENGINE's, not a port choice.
                                    // The attacker's OWN attack power. Only the
                                    // player's comes from GameParameter; a party
                                    // member carries its own.
                                    int atk = mcf::CharType(atkA) == mcf::Actor::kPlayer
                                                  ? player_attack : atkA.attack_power;
                                    mcf::DamageInput dmg_in;
                                    dmg_in.attack = atk;
                                    dmg_in.defence = d.defence;
                                    // Only the player's magical attack is
                                    // known: GameParameter's wisdom, which
                                    // SetCollisionAttackParam passes alongside
                                    // the physical one.
                                    if (mcf::CharType(atkA) == mcf::Actor::kPlayer)
                                        dmg_in.magic = mcf::PlayerStats::Cap(ps.wisdom);
                                    // gauge stays 0: the charge meter is
                                    // understood but not driven yet, and 0 is
                                    // the value Init gives a new game.
                                    dmg_in.roll = int32_t(game_random(25));
                                    // weak stays false: the enemy weakness bytes
                                    // at record +0xa5c..+0xa5f are located but
                                    // their attack-type ids are not decoded.
                                    int dmg = mcf::ComputeDamage(dmg_in);
                                    d.hp -= dmg;
                                    if (d.hp > 0) {
                                        lucent::info("combat",
                                            "{} hits {} for {} ({} - {} def) -> {}/{} HP",
                                            atkA.handle, d.handle, dmg,
                                            atk, d.defence, d.hp, d.max_hp);
                                    } else {
                                        d.hp = 0;
                                        d.alive = false;
                                        ++cs.kills;
                                        // The rewards are now CREDITED, through
                                        // the engine's own AddEXP / AddRC caps,
                                        // instead of only being printed.
                                        // The kill rewards carry the same
                                        // shape of bonus, with a 0..10% roll.
                                        int gain_exp = mcf::RewardWithBonus(
                                            d.exp, int32_t(game_random(11)));
                                        int gain_gp = mcf::RewardWithBonus(
                                            d.money, int32_t(game_random(11)));
                                        ps.AddExp(gain_exp);
                                        ps.AddMoney(gain_gp);
                                        lucent::info("combat",
                                            "{} killed {} (+{} EXP, +{} GP) -> "
                                            "{}/{} EXP, {} GP",
                                            atkA.handle, d.handle, gain_exp, gain_gp,
                                            ps.exp, ps.next_exp(), ps.money);
                                        if (ps.level_up_due() && !level_up_announced) {
                                            level_up_announced = true;
                                            // NOT applied: Process_LevelUp runs
                                            // when the player picks one of four
                                            // training regimens, and choosing
                                            // for them would be inventing a
                                            // decision the game hands over.
                                            lucent::info("player", "level {} is "
                                                "earned ({} of {} EXP) -- the "
                                                "regimen screen is not built, so "
                                                "no level is taken",
                                                ps.level + 1, ps.exp, ps.next_exp());
                                        }
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

                // Message window. Drawn before the fade so a transition covers
                // it, and only when a script has actually set a line.
                // A message the font cannot draw renders as an empty panel,
                // which looks like a bug in the window rather than a missing
                // glyph range. BasicFont holds only ASCII 32..126 -- the game
                // draws CJK with the Android system font, which is not in the
                // archive -- so say it once per distinct line.
                if (fontTex && !sc.last_message.empty() &&
                    sc.last_message != last_warned_message) {
                    last_warned_message = sc.last_message;
                    size_t drawable = 0, printable = 0;
                    for (unsigned char c : sc.last_message) {
                        if (c == '\n') continue;
                        ++printable;
                        if (font.Find(c)) ++drawable;
                    }
                    if (drawable < printable)
                        lucent::warn("text", "{} of {} characters have no glyph in "
                                     "BasicFont (ASCII 32..126 only); the panel will "
                                     "be blank or partial", printable - drawable,
                                     printable);
                }
                // Status HUD. The player has HP, MP, GP and EXP now, and no
                // way to see any of them. Labels come from the game's own
                // string table (SYS_COMMON_STATUS_LABEL_*), so this reads in
                // whichever language is loaded rather than in invented English.
                // PORT CHOICE: the layout. ModeGame::Draw_StatusData draws the
                // real one and is not reversed, so this is a plain corner
                // readout, not a fake of the game's UI.
                if (fontTex && show_hud) {
                    const float kScale = 2.f;                 // line pitch and boxes
                    // Glyph metrics scale separately: the two fonts have
                    // different cell heights and font_scale equalises them.
                    const float kGlyph = kScale * font_scale;
                    const float kMargin = 16.f;
                    auto label = [&](const char* id, const char* fallback) {
                        const std::string* t = strings.Find(id);
                        return t ? *t : std::string(fallback);
                    };
                    std::vector<std::string> rows{
                        std::format("{} {:>3}/{:<3}  {} {:>2}/{:<2}",
                                    label("SYS_COMMON_STATUS_LABEL_4", "HP"),
                                    ps.hp, ps.max_hp(),
                                    label("SYS_COMMON_STATUS_LABEL_5", "MP"),
                                    ps.mp, ps.max_mp()),
                        std::format("{} {:<5}  {} {}/{}",
                                    label("SYS_COMMON_STATUS_LABEL_6", "GP"), ps.money,
                                    label("SYS_COMMON_STATUS_LABEL_7", "EXP"),
                                    ps.exp, ps.next_exp()),
                        std::format("{} {:<3} {} {:<3} {} {}",
                                    label("SYS_COMMON_STATUS_LABEL_8", "ATK"), ps.attack(),
                                    label("SYS_COMMON_STATUS_LABEL_9", "DEF"), ps.defence(),
                                    label("SYS_COMMON_STATUS_LABEL_1", "Lv"),
                                    ps.level_up_due()
                                        ? std::format("{} {}", ps.level,
                                                      label("SYS_COMMON_BUTTON_LEVELUP",
                                                            "Lv Up!"))
                                        : std::format("{}", ps.level)),
                    };
                    std::vector<float> verts;
                    auto push = [&](float x0, float y0, float x1, float y1,
                                    float u0, float v0, float u1, float v1) {
                        auto sx = [&](float px) { return px / float(W) * 2.f - 1.f; };
                        auto sy = [&](float py) { return 1.f - py / float(H) * 2.f; };
                        float q[6][4] = {{sx(x0), sy(y0), u0, v0}, {sx(x1), sy(y0), u1, v0},
                                         {sx(x1), sy(y1), u1, v1}, {sx(x0), sy(y0), u0, v0},
                                         {sx(x1), sy(y1), u1, v1}, {sx(x0), sy(y1), u0, v1}};
                        for (auto& v : q) verts.insert(verts.end(), v, v + 4);
                    };
                    glUseProgram(progText);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDisable(GL_DEPTH_TEST);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, fontTex);
                    glUniform1i(glGetUniformLocation(progText, "tex"), 0);
                    GLint uTint = glGetUniformLocation(progText, "tint");
                    GLint uUse = glGetUniformLocation(progText, "useTex");
                    auto flush = [&](float r, float g, float b, float a, float useTex) {
                        if (verts.empty()) return;
                        glUniform4f(uTint, r, g, b, a);
                        glUniform1f(uUse, useTex);
                        glBindBuffer(GL_ARRAY_BUFFER, textVbo);
                        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * 4),
                                     verts.data(), GL_STREAM_DRAW);
                        glEnableVertexAttribArray(0);
                        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
                        glEnableVertexAttribArray(1);
                        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16,
                                              (void*)(uintptr_t)8);
                        glDrawArrays(GL_TRIANGLES, 0, GLsizei(verts.size() / 4));
                        glDisableVertexAttribArray(1);
                        verts.clear();
                    };
                    float lineH = 11.f * kScale;
                    float widest = 0.f;
                    for (const auto& r : rows) {
                        float w = 0.f;
                        for (uint32_t ch : mcf::Utf8Codepoints(r))
                            if (const mcf::Glyph* g = font.Find(ch))
                                w += float(g->Advance()) * kGlyph;
                        widest = std::max(widest, w);
                    }
                    float boxW = widest + 16.f, boxH = lineH * float(rows.size()) + 12.f;
                    push(kMargin, kMargin, kMargin + boxW, kMargin + boxH, 0, 0, 1, 1);
                    flush(0.05f, 0.07f, 0.18f, 0.72f, 0.f);
                    // A red bar behind the HP row when the pool is low, so the
                    // one number that can end the run is not just small text.
                    if (ps.max_hp() > 0 && ps.hp * 4 <= ps.max_hp()) {
                        push(kMargin, kMargin + 4.f, kMargin + boxW, kMargin + 4.f + lineH,
                             0, 0, 1, 1);
                        flush(0.6f, 0.1f, 0.1f, 0.8f, 0.f);
                    }
                    float aw = float(font.width()), ah = float(font.height());
                    float ty = kMargin + 6.f;
                    for (const auto& row : rows) {
                        float tx = kMargin + 8.f;
                        for (uint32_t ch : mcf::Utf8Codepoints(row)) {
                            const mcf::Glyph* g = font.Find(ch);
                            if (!g) continue;
                            if (g->w && g->h) {
                                float gx = tx + float(g->left) * kGlyph;
                                float gy = ty + float(g->top) * kGlyph;
                                push(gx, gy, gx + float(g->w) * kGlyph,
                                     gy + float(g->h) * kGlyph,
                                     float(g->x) / aw, float(g->y) / ah,
                                     float(g->x + g->w) / aw, float(g->y + g->h) / ah);
                            }
                            tx += float(g->Advance()) * kGlyph;
                        }
                        ty += lineH;
                    }
                    flush(1.f, 1.f, 1.f, 1.f, 1.f);
                    glDisableVertexAttribArray(0);
                    glEnable(GL_DEPTH_TEST);
                    glDisable(GL_BLEND);
                }

                // The level-up screen. PORT CHOICE: the layout, again --
                // ModeGame::Draw_StatusData draws the real one and is not
                // reversed. Every WORD in it is the game's: the four regimen
                // names are SYS_LEVELUP_TYPE_1..4 and the descriptions are the
                // matching SYS_HELP_LEVELUP_* strings, so what the player is
                // told about each choice is what the game tells them.
                if (fontTex && level_up_open) {
                    const float kScale = 2.f;                 // line pitch and boxes
                    // Glyph metrics scale separately: the two fonts have
                    // different cell heights and font_scale equalises them.
                    const float kGlyph = kScale * font_scale;
                    const float kMargin = 40.f;
                    auto str = [&](const char* id, const char* fallback) {
                        const std::string* t = strings.Find(id);
                        return t ? *t : std::string(fallback);
                    };
                    static const char* kTypeIds[4] = {
                        "SYS_LEVELUP_TYPE_1", "SYS_LEVELUP_TYPE_2",
                        "SYS_LEVELUP_TYPE_3", "SYS_LEVELUP_TYPE_4"};
                    static const char* kHelpIds[4] = {
                        "SYS_HELP_LEVELUP_FIGHTER", "SYS_HELP_LEVELUP_MONK",
                        "SYS_HELP_LEVELUP_WIZARD", "SYS_HELP_LEVELUP_WISEMAN"};
                    static const char* kFallback[4] = {"Warrior", "Monk", "Mage", "Sage"};
                    std::vector<std::string> rows;
                    rows.push_back(str("SYS_HELP_LEVELUP_START_TOUCH",
                                       "Select a training regimen."));
                    rows.push_back("");
                    for (int i = 0; i < 4; ++i)
                        rows.push_back(std::format("{} {}",
                                                   i == level_up_choice ? ">" : " ",
                                                   str(kTypeIds[i], kFallback[i])));
                    rows.push_back("");
                    // The chosen regimen's own description, split on the
                    // newline the string already carries.
                    std::string help = str(kHelpIds[level_up_choice], "");
                    for (size_t b = 0, e; b <= help.size(); b = e + 1) {
                        e = help.find('\n', b);
                        if (e == std::string::npos) e = help.size();
                        rows.push_back(help.substr(b, e - b));
                        if (e == help.size()) break;
                    }
                    std::vector<float> verts;
                    auto push = [&](float x0, float y0, float x1, float y1,
                                    float u0, float v0, float u1, float v1) {
                        auto sx = [&](float px) { return px / float(W) * 2.f - 1.f; };
                        auto sy = [&](float py) { return 1.f - py / float(H) * 2.f; };
                        float q[6][4] = {{sx(x0), sy(y0), u0, v0}, {sx(x1), sy(y0), u1, v0},
                                         {sx(x1), sy(y1), u1, v1}, {sx(x0), sy(y0), u0, v0},
                                         {sx(x1), sy(y1), u1, v1}, {sx(x0), sy(y1), u0, v1}};
                        for (auto& v : q) verts.insert(verts.end(), v, v + 4);
                    };
                    glUseProgram(progText);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDisable(GL_DEPTH_TEST);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, fontTex);
                    glUniform1i(glGetUniformLocation(progText, "tex"), 0);
                    GLint uTint = glGetUniformLocation(progText, "tint");
                    GLint uUse = glGetUniformLocation(progText, "useTex");
                    auto flush = [&](float r, float g, float b, float a, float useTex) {
                        if (verts.empty()) return;
                        glUniform4f(uTint, r, g, b, a);
                        glUniform1f(uUse, useTex);
                        glBindBuffer(GL_ARRAY_BUFFER, textVbo);
                        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * 4),
                                     verts.data(), GL_STREAM_DRAW);
                        glEnableVertexAttribArray(0);
                        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
                        glEnableVertexAttribArray(1);
                        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16,
                                              (void*)(uintptr_t)8);
                        glDrawArrays(GL_TRIANGLES, 0, GLsizei(verts.size() / 4));
                        glDisableVertexAttribArray(1);
                        verts.clear();
                    };
                    float lineH = 11.f * kScale;
                    float boxH = lineH * float(rows.size()) + 24.f;
                    float boxY = (float(H) - boxH) * 0.5f;
                    // Dim the world, then the panel over it.
                    push(0, 0, float(W), float(H), 0, 0, 1, 1);
                    flush(0.f, 0.f, 0.f, 0.55f, 0.f);
                    push(kMargin, boxY, float(W) - kMargin, boxY + boxH, 0, 0, 1, 1);
                    flush(0.05f, 0.07f, 0.18f, 0.92f, 0.f);
                    push(kMargin, boxY, float(W) - kMargin, boxY + 2.f, 0, 0, 1, 1);
                    flush(0.55f, 0.65f, 0.95f, 0.9f, 0.f);
                    float aw = float(font.width()), ah = float(font.height());
                    float ty = boxY + 12.f;
                    for (const auto& row : rows) {
                        float tx = kMargin + 16.f;
                        for (uint32_t ch : mcf::Utf8Codepoints(row)) {
                            const mcf::Glyph* g = font.Find(ch);
                            if (!g) continue;
                            if (g->w && g->h) {
                                float gx = tx + float(g->left) * kGlyph;
                                float gy = ty + float(g->top) * kGlyph;
                                push(gx, gy, gx + float(g->w) * kGlyph,
                                     gy + float(g->h) * kGlyph,
                                     float(g->x) / aw, float(g->y) / ah,
                                     float(g->x + g->w) / aw, float(g->y + g->h) / ah);
                            }
                            tx += float(g->Advance()) * kGlyph;
                        }
                        ty += lineH;
                    }
                    flush(1.f, 1.f, 1.f, 1.f, 1.f);
                    glDisableVertexAttribArray(0);
                    glEnable(GL_DEPTH_TEST);
                    glDisable(GL_BLEND);
                }

                if (fontTex && !sc.last_message.empty()) {
                    const float kScale = 2.f;                 // line pitch and boxes
                    // Glyph metrics scale separately: the two fonts have
                    // different cell heights and font_scale equalises them.
                    const float kGlyph = kScale * font_scale;
                    const int kPadX = 14, kPadY = 10;
                    const float kMargin = 16.f;
                    std::vector<float> verts;         // x,y,u,v per vertex
                    auto push = [&](float x0, float y0, float x1, float y1,
                                    float u0, float v0, float u1, float v1) {
                        auto sx = [&](float px) { return px / float(W) * 2.f - 1.f; };
                        auto sy = [&](float py) { return 1.f - py / float(H) * 2.f; };
                        float q[6][4] = {{sx(x0), sy(y0), u0, v0}, {sx(x1), sy(y0), u1, v0},
                                         {sx(x1), sy(y1), u1, v1}, {sx(x0), sy(y0), u0, v0},
                                         {sx(x1), sy(y1), u1, v1}, {sx(x0), sy(y1), u0, v1}};
                        for (auto& v : q) verts.insert(verts.end(), v, v + 4);
                    };
                    // Lay the text out first so the box can be sized to it.
                    // Word-wrap to the panel width: the game's lines carry their
                    // own breaks, but they were authored for a phone screen and
                    // overflow here, and silently clipping text is worse than
                    // wrapping it.
                    float avail = float(W) - kMargin * 2 - kPadX * 2;
                    std::vector<std::string> lines{""};
                    auto width_of = [&](const std::string& t) {
                        float x = 0;
                        for (unsigned char c : t)
                            if (const mcf::Glyph* g = font.Find(c))
                                x += float(g->Advance()) * kGlyph;
                        return x;
                    };
                    for (size_t i = 0; i <= sc.last_message.size(); ++i) {
                        bool end = i == sc.last_message.size();
                        char c = end ? '\n' : sc.last_message[i];
                        if (c == '\n') { if (!end) lines.emplace_back(); continue; }
                        std::string probe = lines.back() + c;
                        if (c != ' ' && width_of(probe) > avail) {
                            // Break at the last space if there is one.
                            auto sp = lines.back().find_last_of(' ');
                            if (sp != std::string::npos) {
                                std::string carry = lines.back().substr(sp + 1);
                                lines.back().erase(sp);
                                lines.push_back(carry + c);
                            } else {
                                lines.push_back(std::string(1, c));
                            }
                            continue;
                        }
                        lines.back() = probe;
                    }
                    float lineH = 11.f * kScale;
                    float boxH = lineH * float(lines.size()) + kPadY * 2;
                    float boxY = float(H) - boxH - kMargin;
                    glUseProgram(progText);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDisable(GL_DEPTH_TEST);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, fontTex);
                    glUniform1i(glGetUniformLocation(progText, "tex"), 0);
                    GLint uTint = glGetUniformLocation(progText, "tint");
                    GLint uUse = glGetUniformLocation(progText, "useTex");
                    auto flush = [&](float r, float g, float b, float a, float useTex) {
                        if (verts.empty()) return;
                        glUniform4f(uTint, r, g, b, a);
                        glUniform1f(uUse, useTex);
                        glBindBuffer(GL_ARRAY_BUFFER, textVbo);
                        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * 4),
                                     verts.data(), GL_STREAM_DRAW);
                        glEnableVertexAttribArray(0);
                        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
                        glEnableVertexAttribArray(1);
                        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16,
                                              (void*)(uintptr_t)8);
                        glDrawArrays(GL_TRIANGLES, 0, GLsizei(verts.size() / 4));
                        glDisableVertexAttribArray(1);
                        verts.clear();
                    };
                    // Panel, then a lighter border line along the top.
                    push(kMargin, boxY, float(W) - kMargin, boxY + boxH, 0, 0, 1, 1);
                    flush(0.05f, 0.07f, 0.18f, 0.85f, 0.f);
                    push(kMargin, boxY, float(W) - kMargin, boxY + 2.f, 0, 0, 1, 1);
                    flush(0.55f, 0.65f, 0.95f, 0.9f, 0.f);
                    // Glyphs.
                    float aw = float(font.width()), ah = float(font.height());
                    float ty = boxY + kPadY;
                    for (const auto& line : lines) {
                        float tx = kMargin + kPadX;
                        for (uint32_t ch : mcf::Utf8Codepoints(line)) {
                            const mcf::Glyph* g = font.Find(ch);
                            if (!g) continue;
                            if (g->w && g->h) {
                                float gx = tx + float(g->left) * kGlyph;
                                float gy = ty + float(g->top) * kGlyph;
                                push(gx, gy, gx + float(g->w) * kGlyph,
                                     gy + float(g->h) * kGlyph,
                                     float(g->x) / aw, float(g->y) / ah,
                                     float(g->x + g->w) / aw, float(g->y + g->h) / ah);
                            }
                            tx += float(g->Advance()) * kGlyph;
                        }
                        ty += lineH;
                    }
                    flush(1.f, 1.f, 1.f, 1.f, 1.f);
                    glDisableVertexAttribArray(0);
                    glEnable(GL_DEPTH_TEST);
                    glDisable(GL_BLEND);
                }

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
                         "{} frame-overlaps ({} rejected by the faction filter) "
                         "-> {} landed hits -> {} kills; "
                         "{} volume pairs over {} live-swing frames; "
                         "closest approach {} units; skipped {} attackers / {} "
                         "defenders with no loaded model, {} with no such bone",
                         cs.hits, cs.blocked_by_faction, cs.landed, cs.kills,
                         cs.pairs, cs.swing_frames,
                         cs.pairs ? std::format("{:.1f}", cs.closest) : "n/a",
                         cs.atk_no_model, cs.def_no_model, cs.def_no_bone);
            lucent::info("combat",
                         "player took {} damage over {} hits, ending on {}/{} HP; "
                         "{} enemy-vs-player volume pairs tested, closest {}",
                         player_damage_taken, player_hits, ps.hp, ps.max_hp(),
                         cs.pairs_vs_player,
                         cs.pairs_vs_player ? std::format("{:.1f} (xz {:.1f}, y {:.1f})",
                                                          cs.closest_vs_player,
                                                          cs.closest_xz, cs.closest_y)
                                            : "n/a");
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
