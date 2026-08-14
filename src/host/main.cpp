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
#include <limits>
#include <numbers>
#include <filesystem>
#include <random>
#include <queue>
#include <deque>
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

bool EventBoxCharacterContact(const mcf::EventBox& box,
                              float x, float y, float z,
                              float room_x, float room_z,
                              float radius) {
    if (!box.enabled || box.no_touch ||
        y <= box.lo[1] || y >= box.hi[1])
        return false;
    const float lx = x - room_x;
    const float lz = z - room_z;
    const float dx = std::max({box.lo[0] - lx, 0.f, lx - box.hi[0]});
    const float dz = std::max({box.lo[2] - lz, 0.f, lz - box.hi[2]});
    // Preserve EventBox::IsHit's strict edges: exact tangency is not contact.
    return dx * dx + dz * dz < radius * radius;
}

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
    bool no_audio = false;
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
    bool camera_selftest = false;
    bool movement_selftest = false;
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
    bool opening_story = false;
    bool continue_story = false;
    std::string stop_room;
    int stop_sccnt = -1;
    int stop_item = -1;
    int warmup = 0;
    bool fixed_step = false;
    bool combat_demo = false;
    bool explicit_model = false;
    std::string lang = "en";
    bool auto_advance = false;
    bool auto_talk = false;
    bool force_window = false;
    bool no_window = false;
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
        else if (a == "--no-window") no_window = true;
        else if (a == "--run-room" && i + 1 < argc) room = argv[++i];
        else if (a == "--render-room" && i + 1 < argc) render_room = argv[++i];
        else if (a == "--bgm-dir" && i + 1 < argc) bgm_dir = argv[++i];
        else if (a == "--audio-selftest") audio_selftest = true;
        else if (a == "--no-audio") no_audio = true;
        else if (a == "--combat-selftest") combat_selftest = true;
        else if (a == "--text-selftest") text_selftest = true;
        else if (a == "--player-selftest") player_selftest = true;
        else if (a == "--inventory-selftest") inventory_selftest = true;
        else if (a == "--ai-selftest") ai_selftest = true;
        else if (a == "--eventbox-selftest") eventbox_selftest = true;
        else if (a == "--camera-selftest") camera_selftest = true;
        else if (a == "--movement-selftest") movement_selftest = true;
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
        else if (a == "--opening-story") {
            opening_story = true;
            auto_attack = true;
            auto_advance = true;
            auto_levelup = 0;
            walk_to = true;
            walk_x = 30.f;
            walk_z = 30.f;
            fixed_step = true;
            no_audio = true;
            no_window = true;
        }
        else if (a == "--continue-story") {
            continue_story = true;
            auto_talk = true;
        }
        else if (a == "--stop-room" && i + 1 < argc) stop_room = argv[++i];
        else if (a == "--stop-sccnt" && i + 1 < argc)
            stop_sccnt = std::atoi(argv[++i]);
        else if (a == "--stop-item" && i + 1 < argc)
            stop_item = std::atoi(argv[++i]);
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
                "  --no-audio          skip playback and audio decoding\n"
                "  --lang en|ja        dialogue language (default en)\n"
                "  --spawn X Z         start at these room-local coordinates\n"
                "\nControls: WASD / arrows to move, Space or Z to attack, Esc to quit.\n"
                "\nTools:\n"
                "  --model NAME [--anim FILE] [--time T]   view one model\n"
                "  --screenshot OUT.png [--warmup N]       render N frames, save, exit\n"
                "  --window            open a real window during a --screenshot run\n"
                "  --no-window         use SDL's offscreen video backend\n"
                "  --fixed-step        step at a fixed 30 Hz (implied by --warmup)\n"
                "  --collision-probe ROOM                  walk outward, report walls\n"
                "  --script-census     run every shipping script and tally cmd calls\n"
                "  --room-census       load every room headlessly, report what is missing\n"
                "  --string ID         resolve a dialogue id in every language\n"
                "  --show-string ID    open the message window on that line\n"
                "  --combat-selftest / --audio-selftest / --text-selftest / --player-selftest\n"
                "  --inventory-selftest / --ai-selftest / --eventbox-selftest / --camera-selftest\n"
                "  --movement-selftest / --nameentry-selftest\n"
                "  --boot              boot through the engine's real mode chain\n"
                "  --title-phase P     TEST HOOK: attract|menu|crawl|names\n"
                "  --shot-delay N      wait N frames inside --shot-mode before capturing\n"
                "                                         self-tests, non-zero on failure\n"
                "  --auto-attack       swing continuously (headless combat driver)\n"
                "  --opening-story     drive the authored opening through both Jackal fights\n"
                "  --continue-story    continue the opening driver past waterfall recovery\n"
                "  --stop-room NAME    exit successfully after loading this room\n"
                "  --stop-sccnt N      exit after scenario N settles with no dialogue/coroutines\n"
                "  --stop-item N       exit after item N is acquired and scripts settle\n"
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
        !audio_selftest && !combat_selftest && !text_selftest && !player_selftest &&
        !camera_selftest && !movement_selftest && !explicit_model && !room_census &&
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
            float cx = has_spawn ? col.aabb_lo[0] + spawn_x
                                 : (col.aabb_lo[0] + col.aabb_hi[0]) * .5f;
            float cz = has_spawn ? col.aabb_lo[2] + spawn_z
                                 : (col.aabb_lo[2] + col.aabb_hi[2]) * .5f;
            float cy = 0;
            const bool have_probe_floor =
                col.GetFloor(cx, cz, mcf::Collision::kFloorMask, &cy);
            lucent::info("probe", "{}: AABB ({:.0f},{:.0f})..({:.0f},{:.0f}), "
                         "{} ({:.1f},{:.1f}) floor={} y={:.1f}", probe,
                         col.aabb_lo[0], col.aabb_lo[2], col.aabb_hi[0],
                         col.aabb_hi[2], has_spawn ? "spawn" : "centre",
                         cx - col.aabb_lo[0], cz - col.aabb_lo[2],
                         have_probe_floor, cy);
            if (!have_probe_floor) {
                lucent::error("probe", "scanned one requested origin, found no floor; "
                              "no directional segment was tested");
                return 1;
            }
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
            // A ray cannot distinguish a room-wide path around an obstacle
            // from an isolated strip. Measure the complete component using
            // the same 7.5-unit lattice, floor query, and wall segments as
            // the headless route instrument.
            constexpr float kProbeStep = 7.5f;
            const int probe_w = int(std::lround(
                (col.aabb_hi[0] - col.aabb_lo[0]) / kProbeStep)) + 1;
            const int probe_h = int(std::lround(
                (col.aabb_hi[2] - col.aabb_lo[2]) / kProbeStep)) + 1;
            const int probe_total = probe_w * probe_h;
            std::vector<float> probe_height(size_t(probe_total), kChipNoFloor);
            std::vector<unsigned char> probe_seen(size_t(probe_total), 0);
            auto probe_x = [&](int x) { return col.aabb_lo[0] + x * kProbeStep; };
            auto probe_z = [&](int z) { return col.aabb_lo[2] + z * kProbeStep; };
            for (int z = 0; z < probe_h; ++z)
                for (int x = 0; x < probe_w; ++x) {
                    float floor = 0.f;
                    if (col.GetFloorBelow(probe_x(x), probe_z(z), cy + 30.f,
                                          mcf::Collision::kFloorMask, &floor) &&
                        std::fabs(floor - cy) < 5.f)
                        probe_height[size_t(z * probe_w + x)] = floor;
                }
            int probe_start = -1;
            float probe_attach = std::numeric_limits<float>::infinity();
            for (int i = 0; i < probe_total; ++i) {
                if (probe_height[size_t(i)] >= kChipNoFloor) continue;
                const float dx = probe_x(i % probe_w) - cx;
                const float dz = probe_z(i / probe_w) - cz;
                const float d2 = dx * dx + dz * dz;
                if (d2 >= probe_attach ||
                    col.BlockedXZ(cx, cz, probe_x(i % probe_w),
                                  probe_z(i / probe_w), cy, 30.f,
                                  mcf::Collision::kWallMask))
                    continue;
                probe_attach = d2;
                probe_start = i;
            }
            std::queue<int> probe_pending;
            if (probe_start >= 0) {
                probe_seen[size_t(probe_start)] = 1;
                probe_pending.push(probe_start);
            }
            int probe_reached = 0;
            int probe_edges[4]{};
            int probe_approach[4]{};
            int probe_edge_lo[4]{probe_w, probe_h, probe_w, probe_h};
            int probe_edge_hi[4]{-1, -1, -1, -1};
            int probe_min_x = probe_w, probe_max_x = -1;
            int probe_min_z = probe_h, probe_max_z = -1;
            static constexpr int probe_dx[4] = {1, -1, 0, 0};
            static constexpr int probe_dz[4] = {0, 0, 1, -1};
            while (!probe_pending.empty()) {
                const int here = probe_pending.front();
                probe_pending.pop();
                ++probe_reached;
                const int hx = here % probe_w;
                const int hz = here / probe_w;
                probe_min_x = std::min(probe_min_x, hx);
                probe_max_x = std::max(probe_max_x, hx);
                probe_min_z = std::min(probe_min_z, hz);
                probe_max_z = std::max(probe_max_z, hz);
                constexpr float kProbeExitApproach = 60.f;
                if (hz * kProbeStep <= kProbeExitApproach) ++probe_approach[0];
                if (hx * kProbeStep >=
                    (probe_w - 1) * kProbeStep - kProbeExitApproach)
                    ++probe_approach[1];
                if (hz * kProbeStep >=
                    (probe_h - 1) * kProbeStep - kProbeExitApproach)
                    ++probe_approach[2];
                if (hx * kProbeStep <= kProbeExitApproach) ++probe_approach[3];
                auto mark_edge = [&](int edge, int along) {
                    ++probe_edges[edge];
                    probe_edge_lo[edge] = std::min(probe_edge_lo[edge], along);
                    probe_edge_hi[edge] = std::max(probe_edge_hi[edge], along);
                };
                if (hz == 0) mark_edge(0, hx);
                if (hx == probe_w - 1) mark_edge(1, hz);
                if (hz == probe_h - 1) mark_edge(2, hx);
                if (hx == 0) mark_edge(3, hz);
                for (int d = 0; d < 4; ++d) {
                    const int nx = hx + probe_dx[d];
                    const int nz = hz + probe_dz[d];
                    if (nx < 0 || nz < 0 || nx >= probe_w || nz >= probe_h)
                        continue;
                    const int next = nz * probe_w + nx;
                    if (probe_seen[size_t(next)] ||
                        probe_height[size_t(next)] >= kChipNoFloor ||
                        col.BlockedXZ(probe_x(hx), probe_z(hz),
                                      probe_x(nx), probe_z(nz),
                                      probe_height[size_t(here)], 30.f,
                                      mcf::Collision::kWallMask))
                        continue;
                    probe_seen[size_t(next)] = 1;
                    probe_pending.push(next);
                }
            }
            lucent::info("probe", "  component reached {}/{} lattice samples; "
                         "boundary nodes up={} right={} down={} left={} (order URDL)",
                         probe_reached, probe_total, probe_edges[0], probe_edges[1],
                         probe_edges[2], probe_edges[3]);
            lucent::info("probe", "    local extent x {:.1f}..{:.1f}, z {:.1f}..{:.1f}; "
                         "60-unit approach nodes up={} right={} down={} left={}",
                         probe_min_x * kProbeStep, probe_max_x * kProbeStep,
                         probe_min_z * kProbeStep, probe_max_z * kProbeStep,
                         probe_approach[0], probe_approach[1],
                         probe_approach[2], probe_approach[3]);
            const char* probe_edge_names[4] = {"up", "right", "down", "left"};
            for (int edge = 0; edge < 4; ++edge)
                if (probe_edges[edge])
                    lucent::info("probe", "    {} boundary local span {:.1f}..{:.1f}",
                                 probe_edge_names[edge],
                                 probe_edge_lo[edge] * kProbeStep,
                                 probe_edge_hi[edge] * kProbeStep);
            if (probe_start < 0) {
                lucent::error("probe", "scanned {} lattice samples but could not "
                              "attach the requested origin to its floor component",
                              probe_total);
                return 1;
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
            ck("floor contact is below strict event volume",
               b.IsHit(15, 60, 35, 0, 0), false);
            ck("character centre enters floor event volume",
               b.IsHit(15, 75, 35, 0, 0), true);
            ck("character body reaches a box behind its blocked origin",
               EventBoxCharacterContact(b, -4.9f, 75, 35, 0, 0, 15.f), true);
            ck("character body near miss remains outside",
               EventBoxCharacterContact(b, -5.1f, 75, 35, 0, 0, 15.f), false);
            // A name is one callback and may own multiple physical volumes.
            // Keeping only the last one makes a script's joined doorway lose
            // part of its actual trigger area.
            mcf::World w;
            b.name = "joined";
            w.boxes.push_back(b);
            b.lo[0] = 20; b.hi[0] = 30;
            w.boxes.push_back(b);
            ck("same-name volumes coexist", w.boxes.size() == 2, true);
            ck("first same-name volume remains hittable",
               w.boxes[0].IsHit(15, 75, 35, 0, 0), true);
            ck("second same-name volume remains hittable",
               w.boxes[1].IsHit(25, 75, 35, 0, 0), true);
            bool both_named = std::all_of(w.boxes.begin(), w.boxes.end(),
                                          [](const mcf::EventBox& e) {
                                              return e.name == "joined";
                                          });
            ck("same-name volumes retain their callback", both_named, true);
            mcf::World vines;
            mcf::EventBox up, down;
            up.lo[0] = 215; up.hi[0] = 235;
            up.lo[1] = 0; up.hi[1] = 11;
            up.lo[2] = 168; up.hi[2] = 195; up.flags = mcf::EventBox::kWallUp;
            down.lo[0] = 210; down.hi[0] = 240;
            down.lo[1] = 76; down.hi[1] = 114;
            down.lo[2] = 168; down.hi[2] = 195; down.flags = mcf::EventBox::kWallDown;
            vines.boxes = {up, down};
            ck("shipping vine contact resolves upward pair",
               vines.FindEventWall(213.1f, 192.6f, 226.f, 199.f,
                                   0.f, 0.f, 0.f).source != nullptr, true);
            ck("shipping vine near miss stays outside",
               vines.FindEventWall(198.f, 192.6f, 212.f, 199.f,
                                   0.f, 0.f, 0.f).source == nullptr, true);
            lucent::info("eventbox", "SELFTEST: 20 cases, {} failures", bad);
            return bad ? 1 : 0;
        }
        if (camera_selftest) {
            int bad = 0, checked = 0;
            auto ck = [&](const char* what, bool got) {
                ++checked;
                if (!got) {
                    ++bad;
                    lucent::error("camera", "SELFTEST FAIL: {}", what);
                } else {
                    lucent::info("camera", "  ok: {}", what);
                }
            };
            mcf::World w;
            mcf::Script script;
            script.world = &w;
            const std::string source =
                "CamSetTargetPos(10,20,30)\n"
                "CamSetTargetPosSub(1,2,3)\n"
                "CamSetPos(40,50,60)\n";
            std::vector<uint8_t> bytes(source.begin(), source.end());
            ck("camera command script executes",
               script.Run("camera-selftest", bytes));
            ck("target position is distinct from eye position",
               w.camera.has_target_pos && w.camera.target_pos[0] == 10.f &&
               w.camera.target_pos[1] == 20.f && w.camera.target_pos[2] == 30.f &&
               w.camera.has_eye_pos && w.camera.eye_pos[0] == 40.f &&
               w.camera.eye_pos[1] == 50.f && w.camera.eye_pos[2] == 60.f);
            ck("target offset reaches all three axes",
               w.camera.target_sub[0] == 1.f && w.camera.target_sub[1] == 2.f &&
               w.camera.target_sub[2] == 3.f);
            const std::string chr_source = "CamSetTargetChr('MainPlayer')\n";
            bytes.assign(chr_source.begin(), chr_source.end());
            ck("character target command executes",
               script.Run("camera-character-selftest", bytes));
            ck("character target replaces fixed target",
               w.camera.target_chr == "MainPlayer" && !w.camera.has_target_pos);
            w.camera.Reset();
            ck("reset clears target, offset, and explicit eye",
               w.camera.target_chr.empty() && !w.camera.has_target_pos &&
               !w.camera.has_eye_pos && w.camera.target_sub[0] == 0.f &&
               w.camera.target_sub[1] == 0.f && w.camera.target_sub[2] == 0.f);
            lucent::info("camera", "SELFTEST: {} cases, {} failures", checked, bad);
            return bad ? 1 : 0;
        }
        if (movement_selftest) {
            int bad = 0, checked = 0;
            auto ck = [&](const char* what, bool got) {
                ++checked;
                if (!got) {
                    ++bad;
                    lucent::error("movement", "SELFTEST FAIL: {}", what);
                } else {
                    lucent::info("movement", "  ok: {}", what);
                }
            };
            mcf::World w;
            auto& actor = w.Spawn("MainPlayer", 0, 0.f, 5.f, 0.f);
            mcf::Script script;
            mcf::Inventory script_inventory;
            script.world = &w;
            script.inventory = &script_inventory;
            script.motion_duration = [](char, int, int motion) {
                return motion == 7 ? 12.f : 0.f;
            };
            script.ground_attribute = [](float x, float) {
                return x > 0.f ? uint32_t(0x02000001) : uint32_t(0x00000001);
            };
            auto run = [&](const char* name, std::string_view source) {
                std::vector<uint8_t> bytes(source.begin(), source.end());
                return script.Run(name, bytes);
            };
            ck("NPC-tagged enemy ids resolve the enemy model namespace",
               mcf::ActorModelName('N', 100) == "E0000_00" &&
               mcf::ActorModelName('N', 173) == "E0073_00");
            ck("NPC-tagged boss ids resolve the boss model namespace",
               mcf::ActorModelName('N', 1010) == "B0010_00" &&
               mcf::ActorModelName('N', 1020) == "B0020_00");
            ck("ordinary and party NPC mappings remain distinct",
               mcf::ActorModelName('N', 10) == "N0000_00" &&
               mcf::ActorModelName('N', 5) == "C0005_00");
            ck("ChrMoveTo starts and reports an automatic move",
               run("movement-start",
                   "ChrMoveTo('MainPlayer',10,6,8)\n"
                   "assert(IsChrAutoMove('MainPlayer'))\n"));
            ck("ChrMoveTo preserves Y and records x/z destination",
               actor.script_move_target[0] == 6.f &&
               actor.script_move_target[1] == 5.f &&
               actor.script_move_target[2] == 8.f);
            w.TickScriptMoves(0.5f);
            ck("half duration advances half the 3-4-5 path",
               actor.script_auto_move && actor.pos[0] == 3.f &&
               actor.pos[1] == 5.f && actor.pos[2] == 4.f &&
               actor.script_distance_moved == 5.f);
            w.TickScriptMoves(0.5f);
            ck("destination ends the automatic move",
               !actor.script_auto_move && actor.pos[0] == 6.f &&
               actor.pos[1] == 5.f && actor.pos[2] == 8.f &&
               actor.script_distance_moved == 10.f &&
               run("movement-finished",
                   "assert(not IsChrAutoMove('MainPlayer'))\n"));
            float before[3]{actor.pos[0], actor.pos[1], actor.pos[2]};
            ck("speed-zero look-at executes",
               run("movement-look", "ChrMoveTo('MainPlayer',0,16,8)\n"));
            ck("speed-zero look-at rotates without translating",
               !actor.script_auto_move && actor.pos[0] == before[0] &&
               actor.pos[1] == before[1] && actor.pos[2] == before[2] &&
               std::abs(actor.rot_y - float(std::numbers::pi / 2.0)) < 0.0001f);
            ck("math_LerpSin follows pre-start, midpoint and end clamps",
               run("lerp-sin",
                   "assert(math.abs(math_LerpSin(10,5,20,2,30,90)-1) < 0.0001)\n"
                   "assert(math.abs(math_LerpSin(10,20,20,2,30,90)-1.7320508) < 0.0001)\n"
                   "assert(math.abs(math_LerpSin(10,31,20,2,30,90)-2) < 0.0001)\n"));
            ck("math_atan2 and bit_and return both nonzero and zero classes",
               run("math-bits",
                   "assert(math.abs(math_atan2(1,0)-math.pi/2) < 0.0001)\n"
                   "assert(bit_and(6,3) == 2 and bit_and(4,1) == 0)\n"));
            ck("GetGroundAttribute exposes both set and clear script-flag classes",
               run("ground-attribute",
                   "assert(bit_and(GetGroundAttribute(1,0),0x02000000) ~= 0)\n"
                   "assert(bit_and(GetGroundAttribute(-1,0),0x02000000) == 0)\n"));
            {
                auto g = mcf::ParseGdt(ar.Read("sk1/M0001_00_00.gdt"));
                ck("shipping GDT resolves boundary EX_1 and clear arena centre",
                   g.cols == 40 && g.rows == 32 &&
                   (g.Get(30.f, 30.f) & 0x02000000u) != 0 &&
                   (g.Get(150.f, 135.f) & 0x02000000u) == 0);
                auto c = mcf::ParseScol(ar.Read("sk1/M0001_00_00.scol"));
                ck("shipping collision blocks an arena wall but not open floor",
                   c.BlockedXZ(35.f, 100.f, 25.f, 100.f, 0.f, 30.f,
                               mcf::Collision::kWallMask) &&
                   !c.BlockedXZ(100.f, 100.f, 110.f, 100.f, 0.f, 30.f,
                                mcf::Collision::kWallMask));
                auto stacked =
                    mcf::ParseScol(ar.Read("sk1/M0000_06_05.scol"));
                float top = 0.f, lower = 0.f;
                const bool have_top = stacked.GetFloor(
                    2070.f, 1200.f, mcf::Collision::kFloorMask, &top);
                const bool have_lower = stacked.GetFloorBelow(
                    2070.f, 1200.f, 210.f,
                    mcf::Collision::kFloorMask, &lower);
                lucent::info("movement",
                             "  stacked-floor discriminator: top={} ({:.1f}), "
                             "below-210={} ({:.1f})",
                             have_top, top, have_lower, lower);
                ck("shipping floor query distinguishes stacked ledges",
                   have_top && have_lower && top == 240.f && lower == 180.f);
            }
            actor.hp = 37;
            actor.max_hp = 40;
            ck("ChrGetData HP reads live combat state and ChrSetData writes it",
               run("combat-status", "assert(ChrGetData('MainPlayer',0) == 37)\n"
                                      "assert(ChrGetData('MainPlayer',1) == 40)\n"
                                      "ChrSetData('MainPlayer',0,12)\n"
                                      "assert(ChrGetData('MainPlayer',0) == 12)\n") &&
               actor.hp == 12);
            ck("script attack phases advance once per false-to-true edge",
               run("attack-phase",
                   "ChrAttackBoneSet('MainPlayer',2,'cog')\n"
                   "ChrAttackBoneValid('MainPlayer',2,true)\n"
                   "ChrAttackBoneValid('MainPlayer',2,true)\n"
                   "ChrAttackBoneValid('MainPlayer',2,false)\n"
                   "ChrAttackBoneValid('MainPlayer',2,true)\n") &&
               actor.swing_id == 2);
            ck("ChrMotion resolves its end frame synchronously",
               run("motion-start", "ChrMotionForce('MainPlayer', 7)\n"
                                     "assert(ChrMotionGetEndFrame('MainPlayer') == 12)\n"));
            w.TickMotions(5.f);
            ck("motion clock reports an unfinished shipping-duration motion",
               run("motion-mid", "assert(ChrMotionGetFrame('MainPlayer') == 5)\n"
                                  "assert(ChrMotionGetEndFrame('MainPlayer') == 12)\n"
                                  "assert(not IsChrMotionFinish('MainPlayer'))\n"));
            w.TickMotions(7.f);
            ck("motion clock reaches and reports its exact end frame",
               run("motion-end", "assert(ChrMotionGetFrame('MainPlayer') == 12)\n"
                                  "assert(IsChrMotionFinish('MainPlayer'))\n"));
            ck("ChrMotionForce restarts even the current motion",
               run("motion-force", "ChrMotionForce('MainPlayer', 7)\n"
                                    "assert(ChrMotionGetFrame('MainPlayer') == 0)\n"
                                    "assert(ChrMotionGetEndFrame('MainPlayer') == 12)\n"
                                    "assert(not IsChrMotionFinish('MainPlayer'))\n"));
            actor.data[mcf::chr_data::kMapCollision] = 1.f;
            ck("map collision starts a test move",
               run("collision-start", "ChrMoveTo('MainPlayer',10,16,8)\n"));
            float blocked_x = actor.pos[0], blocked_z = actor.pos[2];
            w.TickScriptMoves(0.5f, [](const mcf::Actor&, float, float) {
                return true;
            });
            ck("blocked scripted move stays put, ends, and reports ISHITMAP",
               actor.pos[0] == blocked_x && actor.pos[2] == blocked_z &&
               !actor.script_auto_move &&
               actor.Get(mcf::chr_data::kIsHitMap) == 1.f &&
               actor.script_map_hits == 1);
            ck("an empty world does not report a cleared enemy wave",
               !w.ConsumeEnemyWaveCleared());
            auto& foe = w.Spawn("wave-enemy", 0, 0.f, 0.f, 0.f);
            foe.kind = 'E';
            ck("a live enemy arms but does not fire the wave transition",
               !w.ConsumeEnemyWaveCleared());
            foe.defeated = true;
            ck("the last enemy defeat fires exactly once",
               w.ConsumeEnemyWaveCleared());
            ck("an empty frame after the transition does not refire it",
               !w.ConsumeEnemyWaveCleared());
            auto& boss = w.Spawn("wave-boss", 0, 0.f, 0.f, 0.f);
            boss.kind = 'B';
            ck("a later boss wave rearms and clears independently",
               !w.ConsumeEnemyWaveCleared() &&
               ((boss.defeated = true), w.ConsumeEnemyWaveCleared()));
            ck("SetDoor preserves FREE as distinct from a missing door",
               run("door-free", "SetDoor(0, 0)\n") &&
               w.DoorType(0) == 0 && w.DoorType(1) == mcf::World::kNoDoor);
            ck("SetDoor preserves a locked door type",
               run("door-key", "SetDoor(2, 1)\n") && w.DoorType(2) == 1);
            ck("OpenDoor clears the authored directional room mark",
               run("door-open", "OpenDoor(2)\n") &&
               w.DoorType(2) == mcf::World::kNoDoor);
            w.Reset();
            ck("room reset clears every authored door",
               w.DoorType(0) == mcf::World::kNoDoor &&
               w.DoorType(2) == mcf::World::kNoDoor);
            ck("SetCinema exposes both locked and unlocked input states",
               run("cinema-on", "SetCinema(true)\neventScene=101\n") &&
               script.cinema && script.GlobalNumber("eventScene") == 101.0 &&
               run("cinema-off", "SetCinema(false)\neventScene=0\n") &&
               !script.cinema && script.GlobalNumber("eventScene") == 0.0);
            ck("SetPlayerControllEnable exposes both input states",
               run("control-off", "SetPlayerControllEnable(false)\n") &&
               !script.player_control_enabled &&
               run("control-on", "SetPlayerControllEnable(true)\n") &&
               script.player_control_enabled);
            script.SetGlobalNumber("switch_result", 0.0);
            const bool released_switch =
                script.GlobalNumber("switch_result", -1.0) == 0.0;
            script.SetGlobalNumber("switch_result", 1.0);
            ck("host switch payload distinguishes released and pressed",
               released_switch &&
               script.GlobalNumber("switch_result", -1.0) == 1.0);
            ck("object visibility exposes both visible and hidden states",
               run("object-visible",
                   "ObjVisible(1302, true)\n"
                   "assert(ObjIsVisible(1302))\n"
                   "ObjVisible(1302, false)\n"
                   "assert(not ObjIsVisible(1302))\n"));
            ck("Lua inventory commands exercise success and refusal classes",
               run("inventory-bridge",
                   "assert(IsAddItem(17))\n"
                   "assert(AddItem(17))\n"
                   "assert(DelItem(17))\n"
                   "assert(not DelItem(17))\n") &&
               !script_inventory.Has(17));
            ck("GetEquipID exposes empty and out-of-range slots",
               run("equipment-empty",
                   "assert(GetEquipID(4) == 0)\n"
                   "assert(GetEquipID(-1) == 0)\n"
                   "assert(GetEquipID(8) == 0)\n"));
            ck("owned equipment is visible through the shipping Lua command",
               script_inventory.Add(30) && script_inventory.Equip(4, 30) &&
               run("equipment-present", "assert(GetEquipID(4) == 30)\n"));
            ck("AddBox creates a closed item-bearing shipping box",
               run("box-closed", "AddBox(150,0,90,17)\n") &&
               w.Find("_BOX") && w.Find("_BOX")->treasure_box &&
               !w.Find("_BOX")->treasure_open &&
               w.Find("_BOX")->treasure_item == 17);
            w.Reset();
            ck("shipping item 97 creates the already-open box class",
               run("box-open", "AddBox(150,0,90,97)\n") &&
               w.Find("_BOX") && w.Find("_BOX")->treasure_open);
            {
                mcf::World zw;
                mcf::Script zs;
                zs.world = &zw;
                int rolls = 0;
                zs.random_index = [&](int n) { return (rolls++) % n; };
                auto zrun = [&](std::string_view source) {
                    std::vector<uint8_t> bytes(source.begin(), source.end());
                    return zs.Run("zaco-selftest", bytes);
                };
                ck("AddEnemyZaco zero count produces no actors",
                   zrun("AddEnemyZaco(0,-1,-1,-1,-1,-1)\n") &&
                   zw.actors().empty());
                ck("AddEnemyZaco spawns the requested count from the terminated id list",
                   zrun("AddEnemyZaco(3,3,7,-1,-1,-1)\n") &&
                   zw.actors().size() == 3 && rolls == 3 &&
                   zw.actors()[0].type_id == 3 &&
                   zw.actors()[1].type_id == 7 &&
                   zw.actors()[2].type_id == 3);
                ck("AddEnemyZaco actors have unique handles and pending engine placement",
                   zw.actors().size() == 3 &&
                   zw.actors()[0].handle != zw.actors()[1].handle &&
                   zw.actors()[1].handle != zw.actors()[2].handle &&
                   std::all_of(zw.actors().begin(), zw.actors().end(),
                               [](const mcf::Actor& a) {
                                   return a.kind == 'E' && a.random_place &&
                                          !a.random_placed;
                               }));
                zw.Reset();
                ck("shipping party id table resolves every script-visible handle",
                   std::string(mcf::PartyHandle(1)) == "PARTY_HEROINE" &&
                   std::string(mcf::PartyHandle(9)) == "PARTY_MARCIE" &&
                   std::string(mcf::PartyHandle(0)).empty() &&
                   std::string(mcf::PartyHandle(10)).empty());
                ck("AddParty creates the named persistent heroine actor",
                   zrun("AddParty(1,150,0,150)\n") && zs.party_id == 1 &&
                   zw.Find("PARTY_HEROINE") &&
                   zw.Find("PARTY_HEROINE")->kind == 'C' &&
                   zw.Find("PARTY_HEROINE")->pos[0] == 150.f);
                const size_t party_actor_count = zw.actors().size();
                ck("AddParty of the current id is idempotent",
                   zrun("AddParty(1,0,0,0)\n") &&
                   zw.actors().size() == party_actor_count);
                ck("AddParty zero removes the current companion",
                   zrun("AddParty(0,0,0,0)\n") && zs.party_id == 0 &&
                   zw.Find("PARTY_HEROINE") &&
                   !zw.Find("PARTY_HEROINE")->alive);
            }
            lucent::info("movement", "SELFTEST: {} cases, {} failures", checked, bad);
            return bad ? 1 : 0;
        }
        if (ai_selftest) {
            {
                mcf::Actor enemy;
                enemy.kind = 'E';
                mcf::Actor boss;
                boss.kind = 'B';
                if (!mcf::UsesHostEnemyAI(enemy) || mcf::UsesHostEnemyAI(boss)) {
                    lucent::error("ai", "SELFTEST FAIL: host AI ownership: "
                                  "ordinary enemy={}, scripted boss={}",
                                  mcf::UsesHostEnemyAI(enemy),
                                  mcf::UsesHostEnemyAI(boss));
                    return 1;
                }
                lucent::info("ai", "  ok: host AI owns ordinary enemies and "
                             "refuses script-owned bosses");
            }
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
                check("new-game equipment slots match the four grants",
                      inv.Equipped(0) == 101 && inv.Equipped(1) == 201 &&
                      inv.Equipped(2) == 301 && inv.Equipped(3) == 401, 1);
                check("new-game button slots are empty",
                      inv.Equipped(4) + inv.Equipped(5) +
                      inv.Equipped(6) + inv.Equipped(7), 0);
                check("GetEquipID rejects both out-of-range classes",
                      inv.Equipped(-1) + inv.Equipped(8), 0);
                check("an unowned item cannot be equipped", inv.Equip(4, 30), 0);
                check("an owned item equips into a button slot",
                      inv.Add(30) && inv.Equip(4, 30) &&
                      inv.Equipped(4) == 30, 1);
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
            {   // tblItem+0x04 is the remaining-use count. The binary stores
                // one as zero, but UseInventory still removes it on first use;
                // the port normalizes that representation to an explicit one.
                mcf::Inventory inv;
                check("Mattock grant succeeds", inv.Add(17), 1);
                check("Mattock starts with seven uses", inv.Uses(17), 7);
                bool first_six = true;
                for (int i = 0; i < 6; ++i) first_six &= inv.Consume(17);
                check("six Mattock uses leave one", first_six && inv.Uses(17), 1);
                check("seventh Mattock use succeeds", inv.Consume(17), 1);
                check("seventh use removes Mattock", inv.Has(17), 0);
                check("eighth use is refused", inv.Consume(17), 0);
                check("one-use item grant succeeds", inv.Add(1), 1);
                check("one-use item is removed immediately", inv.Consume(1) &&
                      !inv.Has(1), 1);
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
            if (!no_audio)
                audio.Init();      // logs and disables itself if there is no device
            mcf::Script sc;
            sc.world = &world;
            sc.audio = &audio;
            mcf::Inventory inventory;
            inventory.NewGame();
            sc.inventory = &inventory;
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
        if ((no_window || !shot.empty()) && !force_window)
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
        if (!force_window && !SDL_GL_SetSwapInterval(0))
            lucent::warn("host", "could not disable swap pacing: {}", SDL_GetError());
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
            mcf::GroundAttributes ground;
            bool have_ground = false;
            float room_org[3]{0, 0, 0};
            mcf::RoomSize room_size;
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
            struct PlacedObj {
                const mcf::Renderable* r;
                float pos[3];
                int32_t id = 0;
                uint32_t flags = 0;
                int32_t script_id = 0;
                bool alive = true;
            };
            std::vector<PlacedObj> objects;
            std::map<std::string, mcf::Renderable> cache;   // survives transitions
            std::set<std::string> missing_actor_models;
            std::set<std::string> missing_actor_motions;
            std::set<std::string> reported_script_moves;
            std::set<std::string> reported_script_map_hits;
            std::map<std::string, mcf::Motion> motions;     // survives transitions

            mcf::World world;
            mcf::Audio audio;
            if (!no_audio)
                audio.Init();      // logs and disables itself if there is no device
            mcf::Script sc;
            sc.world = &world;
            sc.audio = &audio;
            mcf::Inventory inventory;
            inventory.NewGame();
            sc.inventory = &inventory;
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
            std::function<int()> placeRandomActors = [] { return 0; };

            std::string room_name = render_room;
            bool bogard_house_visited = false;
            bool bogard_return_reached_vine_summit = false;
            bool post_matock_middle_crossed = false;
            bool post_matock_cave_crossed = false;
            bool hydra_upper_bridge_crossed = false;
            bool hydra_mountain_climbed = false;
            std::optional<mcf::EventBox> mapjump_floor_owner;
            float mapjump_floor_owner_y = 0.f;
            bool fallman_talked = false;
            auto loadRoom = [&](const std::string& name) -> bool {
                const std::string previous_room = room_name;
                room_name = name;
                mapjump_floor_owner.reset();
                if (previous_room == "M0011_00_02" &&
                    name == "M0000_09_06") {
                    post_matock_cave_crossed = true;
                    lucent::info("world", "completed authored post-Matock cave "
                                 "crossing through M0011_00_02/in_1");
                }
                if (previous_room == "M0013_00_04" &&
                    name == "M0013_02_00")
                    hydra_upper_bridge_crossed = true;
                if (previous_room == "M0013_06_05" &&
                    name == "M0013_01_00")
                    hydra_mountain_climbed = true;
                if (name == "M0010_00_01") bogard_house_visited = true;
                if (bogard_house_visited && name == "M0000_07_04")
                    bogard_return_reached_vine_summit = true;
                stage = mcf::Renderable{};
                if (!mcf::LoadRenderable(ar, room_name, white, &stage)) {
                    lucent::error("world", "no model for room {}", room_name);
                    return false;
                }

                // The Lua state persists ACROSS rooms: scenario flags (sccnt,
                // scflagNN) must survive a transition, which is why SystemInit
                // runs once at startup and not per room. Map-local callbacks
                // and pending threads must not survive: an old Init/event
                // handler can otherwise run in the new map by its stale name.
                sc.ClearRoomScript();
                world.Reset();

                auto sp = std::format("sk1/{}.lua", room_name);
                if (ar.Has(sp)) {
                    auto globals_before = sc.Globals();
                    if (!sc.Run(sp, ar.Read(sp)))
                        lucent::warn("lua", "{}: {}", sp, sc.last_error());
                    else
                        sc.RememberRoomFunctions(globals_before);
                }

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
            have_ground = false;
            {
                auto cs = std::format("sk1/{}.scol", room_name);
                if (ar.Has(cs)) { col = mcf::ParseScol(ar.Read(cs)); have_col = true; }
                else lucent::warn("world", "no {}; actors keep their script Y", cs);
                auto gs = std::format("sk1/{}.gdt", room_name);
                if (ar.Has(gs)) {
                    ground = mcf::ParseGdt(ar.Read(gs));
                    have_ground = true;
                }
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

            placeRandomActors = [&]() {
                if (!have_col) return 0;
                std::vector<std::pair<int, int>> taken_chips;
                for (const auto& a : world.actors()) {
                    if (!a.random_place || !a.random_placed) continue;
                    taken_chips.emplace_back(int(a.pos[0] / 30.f),
                                             int(a.pos[2] / 30.f));
                }
                int placed_count = 0;
                for (auto& a : world.actors_mutable()) {
                    if (!a.random_place || a.random_placed) continue;
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
                            if (!col.GetFloor(wx, wz, mcf::Collision::kFloorMask, &g))
                                continue;
                            // One actor per chip, or a room's NPCs all stack on the
                            // single chip nearest the centre.
                            if (std::find(taken_chips.begin(), taken_chips.end(),
                                          std::make_pair(gx, gz)) != taken_chips.end())
                                continue;
                            float d = (wx - cx) * (wx - cx) +
                                      (wz - cz) * (wz - cz);
                            if (d < best_d) {
                                best_d = d;
                                best[0] = wx;
                                best[1] = wz;
                                found = true;
                                best_chip = {gx, gz};
                            }
                        }
                    }
                    if (found) {
                        taken_chips.push_back(best_chip);
                        a.pos[0] = best[0] - room_org[0];
                        a.pos[2] = best[1] - room_org[2];
                        a.random_placed = true;
                        ++placed_count;
                        lucent::info("world", "{}: engine-placed (script gave 0,0, "
                                     "extent {:.0f}) -> room-local ({:.0f},{:.0f})",
                                     a.handle, a.place_extent, a.pos[0], a.pos[2]);
                    } else {
                        lucent::warn("world", "{}: engine placement scanned {} chips "
                                     "({}x{}), found 0 available walkable chips; "
                                     "leaving it at the origin", a.handle,
                                     chips_x * chips_z, chips_x, chips_z);
                    }
                }
                return placed_count;
            };
            placeRandomActors();

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
                    objects.push_back({&cache[nm],
                                       {o.pos[0], o.pos[1], o.pos[2]},
                                       o.id, o.flags, o.script_id, true});
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

            // Event boxes are how the game connects rooms, so a room that
            // registered none is worth seeing -- previously indistinguishable
            // from a room whose script never ran. Report after resolving the
            // floor sentinel: a pre-resolution report hid the Y interval that
            // decides whether a player can actually enter a box.
            {
                size_t live = 0;
                for (const auto& bx : world.boxes)
                    if (bx.enabled && !bx.no_touch) ++live;
                lucent::info("world", "{} event box(es), {} touchable",
                             world.boxes.size(), live);
                for (const auto& bx : world.boxes)
                    lucent::info("world", "  box '{}' local ({:.0f},{:.0f},{:.0f}).."
                                 "({:.0f},{:.0f},{:.0f}) flags=0x{:x}{}",
                                 bx.name, bx.lo[0], bx.lo[1], bx.lo[2],
                                 bx.hi[0], bx.hi[1], bx.hi[2], bx.flags,
                                 bx.floor_y ? " (floor unresolved)" : "");
            }

                return true;
            };
            if (!loadRoom(render_room))
                throw mcf::Error(std::format("cannot load room {}", render_room));
            // AppObjectModel::IsHitCharacter tests the character sphere against
            // the object's model AABB. CheckAddPos uses a radius-12 sphere for
            // the same object occupancy bit, so route planning and live motion
            // share that measured radius here.
            auto objectBlockedXZ = [&](float x0, float z0, float x1, float z1,
                                       float y, PlacedObj** hit) {
                constexpr float kObjectPlayerRadius = 12.f;
                for (auto& object : objects) {
                    // Only flag 0x08 is decoded end-to-end so far:
                    // DamageMove accepts weapon kinds 5/6 and kills the
                    // object. Other table entries may be decorative or use a
                    // table-defined collision box that is not the render AABB;
                    // do not manufacture collision from visual geometry.
                    if (!object.alive || !sc.ObjectVisible(object.script_id) ||
                        !(object.flags & 0x08))
                        continue;
                    // The automated route may cross a destructible object only
                    // while it has the shipping tool that can clear it. Live
                    // movement still asks for the hit object and performs the
                    // actual inventory-consuming collision below.
                    if (!hit && opening_story && inventory.Has(17) &&
                        (object.flags & 0x08))
                        continue;
                    const float lo_y = object.pos[1] + object.r->lo[1];
                    const float hi_y = object.pos[1] + object.r->hi[1];
                    if (y + 30.f < lo_y || y > hi_y) continue;
                    const float lo_x = object.pos[0] + object.r->lo[0] -
                                       kObjectPlayerRadius;
                    const float hi_x = object.pos[0] + object.r->hi[0] +
                                       kObjectPlayerRadius;
                    const float lo_z = object.pos[2] + object.r->lo[2] -
                                       kObjectPlayerRadius;
                    const float hi_z = object.pos[2] + object.r->hi[2] +
                                       kObjectPlayerRadius;
                    float t0 = 0.f, t1 = 1.f;
                    auto clip = [&](float p, float q) {
                        if (p == 0.f) return q >= 0.f;
                        const float r = q / p;
                        if (p < 0.f) {
                            if (r > t1) return false;
                            t0 = std::max(t0, r);
                        } else {
                            if (r < t0) return false;
                            t1 = std::min(t1, r);
                        }
                        return true;
                    };
                    if (clip(-(x1 - x0), x0 - lo_x) &&
                        clip( (x1 - x0), hi_x - x0) &&
                        clip(-(z1 - z0), z0 - lo_z) &&
                        clip( (z1 - z0), hi_z - z0)) {
                        if (hit) *hit = &object;
                        return true;
                    }
                }
                return false;
            };
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
            auto resolveMotionDuration = [&](char kind, int type_id, int motion) {
                auto nm = mcf::ActorModelName(kind, type_id);
                if (nm.empty()) return 0.f;
                auto file = ar.FindByPrefix(mcf::World::MotionPrefix(nm, motion));
                if (file.empty()) return 0.f;
                auto mit = motions.find(file);
                if (mit == motions.end())
                    mit = motions.emplace(file, mcf::ParseSmot(ar.Read(file))).first;
                return mit->second.duration;
            };
            sc.motion_duration = resolveMotionDuration;
            sc.ground_attribute = [&](float x, float z) {
                return have_ground ? ground.Get(x, z) : uint32_t(0);
            };
            auto actorMotion = [&](mcf::Actor& a) -> const mcf::Motion* {
                auto nm = mcf::ActorModelName(a.kind, a.type_id);
                if (nm.empty()) return nullptr; // eNPC.TRANS is intentionally invisible
                auto it = cache.find(nm);
                if (it == cache.end()) {
                    mcf::Renderable late;
                    if (!mcf::LoadRenderable(ar, nm, white, &late)) {
                        if (missing_actor_models.insert(nm).second)
                            lucent::warn("world", "late actor {} (kind {} id {}) "
                                         "has no model {}", a.handle, a.kind,
                                         a.type_id, nm);
                        return nullptr;
                    }
                    lucent::info("world", "loaded late actor {} model {}", a.handle, nm);
                    it = cache.emplace(nm, std::move(late)).first;
                }
                if (it->second.model.bones.empty()) return nullptr;
                auto prefix = mcf::World::MotionPrefix(nm, a.motion);
                auto file = ar.FindByPrefix(prefix);
                if (file.empty()) {
                    auto key = std::format("{}:{}", nm, a.motion);
                    if (missing_actor_motions.insert(key).second)
                        lucent::warn("world", "actor {} model {} has no motion {} "
                                     "(prefix {}); finish polling cannot advance",
                                     a.handle, nm, a.motion, prefix);
                    return nullptr;
                }
                auto mit = motions.find(file);
                if (mit == motions.end())
                    mit = motions.emplace(file, mcf::ParseSmot(ar.Read(file))).first;
                a.motion_duration = mit->second.duration;
                return &mit->second;
            };
            float px = ctr[0], pz = ctr[2], py = 0, pdeg = 0;
            constexpr float kEventBoxProbeHeight = 15.f;
            constexpr float kCharacterCollisionRadius = 30.f;
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

            auto startRoomInit = [&]() {
                if (!sc.HasFunction("Init")) return;
                if (!sc.StartCoroutine("Init"))
                    lucent::warn("lua", "{} Init: {}", room_name, sc.last_error());
                else
                    lucent::info("world", "started {} Init coroutine", room_name);
            };
            auto restoreParty = [&]() {
                const char* handle = mcf::PartyHandle(sc.party_id);
                if (!*handle || world.Find(handle)) return;
                auto& party = world.Spawn(handle, sc.party_id,
                                          px - room_org[0], py - room_org[1],
                                          pz - room_org[2]);
                party.kind = 'C';
                lucent::info("world", "restored {} after room load at "
                             "room-local ({:.1f},{:.1f},{:.1f})", handle,
                             party.pos[0], party.pos[1], party.pos[2]);
            };
            startRoomInit();

            // Service whatever the room script asked for. BGM lives in the APK
            // assets, not the MPK, so it is loaded from a directory on disk.
            auto serviceAudio = [&]() {
                if (no_audio) {
                    if (sc.pending_bgm >= 0) {
                        sc.current_bgm = sc.pending_bgm;
                        lucent::info("audio", "bgm {} requested (audio disabled)",
                                     sc.pending_bgm);
                    }
                    sc.stop_all_se = false;
                    sc.pending_se_stop.clear();
                    sc.pending_se.clear();
                    sc.pending_bgm = -1;
                    return;
                }
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
            bool auto_attack_armed = auto_attack && !walk_to;
            bool auto_attack_armed_logged = false;
            std::string auto_attack_target_logged;
            // GameRandom @ 0x3da480 is NOT reversed, so this is a stand-in with
            // the same range contract, seeded fixed so a headless run is
            // reproducible. The SHAPE of the roll is the engine's; the sequence
            // is not.
            std::mt19937 rng(12345);
            auto game_random = [&rng](int n) {
                return std::uniform_int_distribution<int>(0, n - 1)(rng);
            };
            sc.random_index = game_random;
            int player_attack = ps.attack();
            if (!mcf::FindWeapon(ps.weapon))
                lucent::warn("combat", "weapon {} not in tblWeapon; the player's "
                             "attack is the power stat alone", ps.weapon);
            seedCombat = [&] {
                if (auto* pl = world.Find("MainPlayer")) {
                    auto& av = pl->attack[0];
                    if (av.bone.empty()) {
                        av.bone = "cog"; av.radius = 45.f; av.arc_deg = 180.f;
                        av.valid = false;
                    }
                    // The player must also be a TARGET, or enemy attack volumes
                    // have nothing to hit and combat stays one-sided.
                    auto& pdv = pl->damage[0];
                    if (pdv.bone.empty()) {
                        pdv.bone = "y_ang"; pdv.radius = 15.f; pdv.valid = true;
                    }
                }
                int with_stats = 0, without = 0;
                for (auto& a : world.actors_mutable()) {
                    if (a.kind != 'E' && a.kind != 'B') continue;
                    if (a.combat_seeded) continue;
                    a.combat_seeded = true;
                    auto& dv = a.damage[0];
                    dv.bone = "y_ang"; dv.radius = 15.f; dv.valid = true;
                    auto it = enemy_stats.find(a.type_id);
                    if (it == enemy_stats.end()) { ++without; continue; }
                    // Ordinary enemies attack too. The volume is always live because
                    // enemy attack timing is native code that is not reversed;
                    // the player's i-frame window is what keeps this from
                    // being a continuous damage stream, and that IS reversed.
                    // Radius/arc are an ATTESTED script configuration rather
                    // than a made-up number: ChrAttackBoneSize(my, 1, 20, 360)
                    // is the most common enemy setup in the shipping scripts.
                    // Bosses are different: their _BOSS coroutine owns the
                    // explicitly numbered attack volumes and their timing.
                    // Giving them this host volume made a stationary boss hurt
                    // the player and produced a false-positive attack trace.
                    if (a.kind == 'E') {
                        auto& eav = a.attack[0];
                        eav.bone = "y_ang"; eav.radius = 20.f; eav.arc_deg = 360.f;
                        eav.valid = true;
                    }
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
            bool run_failed = false;
            struct RoomExitReq {
                bool pending = false;
                std::string dest;
                int arrow = 0;
                float world_x = 0.f, world_z = 0.f;
            } room_exit;
            std::set<std::pair<std::string, int>> warned_empty_room_edges;
            auto requestRoomExit = [&](int side, float lx, float lz) {
                constexpr float kDoorHalfWidth = 30.f;
                const int door = world.DoorType(side);
                const bool at_door = (side == 0 || side == 2)
                    ? std::fabs(lx - room_size.w * .5f) <= kDoorHalfWidth
                    : std::fabs(lz - room_size.h * .5f) <= kDoorHalfWidth;
                if (door != mcf::World::kNoDoor && !(door == 0 && at_door))
                    return false;
                int map = -1, gx = -1, gy = -1;
                if (std::sscanf(room_name.c_str(), "M%d_%d_%d",
                                &map, &gx, &gy) != 3)
                    return false;
                static constexpr int kDx[4]{0, 1, 0, -1};
                static constexpr int kDy[4]{-1, 0, 1, 0};
                const std::string& next = mcf::WorldRoomName(
                    map, gx + kDx[side], gy + kDy[side]);
                if (next.empty()) {
                    if (warned_empty_room_edges.emplace(room_name, side).second)
                        lucent::warn("world", "room edge {} in {} leads to an "
                                     "empty world-table cell", side, room_name);
                    return false;
                }
                float wx = room_org[0] + lx;
                float wz = room_org[2] + lz;
                const float inf = std::numeric_limits<float>::infinity();
                if (side == 0) wz = std::nextafter(room_org[2], -inf);
                if (side == 1)
                    wx = std::nextafter(room_org[0] + room_size.w, inf);
                if (side == 2)
                    wz = std::nextafter(room_org[2] + room_size.h, inf);
                if (side == 3) wx = std::nextafter(room_org[0], -inf);
                room_exit = {true, next.substr(4), side, wx, wz};
                return true;
            };
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
            std::string driver_route_room;
            float driver_route_goal_x = std::numeric_limits<float>::quiet_NaN();
            float driver_route_goal_z = std::numeric_limits<float>::quiet_NaN();
            float driver_route_contact_x = std::numeric_limits<float>::quiet_NaN();
            float driver_route_contact_z = std::numeric_limits<float>::quiet_NaN();
            float driver_route_through_x = std::numeric_limits<float>::quiet_NaN();
            float driver_route_through_z = std::numeric_limits<float>::quiet_NaN();
            struct DriverWaypoint { float x, y, z; };
            std::deque<DriverWaypoint> driver_route;
            bool driver_route_is_staging = false;
            bool driver_route_descending = false;
            float driver_route_staging_floor = 0.f;
            int driver_blocked_frames = 0;
            bool event_wall_inside = false;
            bool reported_unlinked_event_wall_contact = false;
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
                long atk_no_model = 0, atk_no_bone = 0;
                long def_no_model = 0, def_no_bone = 0;
                long pairs_vs_player = 0;   // enemy attack volume vs the player
                long pairs_from_player = 0, hits_from_player = 0;
                float closest_vs_player = 1e30f, closest_xz = 0, closest_y = 0;
                float closest_from_player = 1e30f;
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
                bool player_script_moving = false;
                if (auto* pl = world.Find("MainPlayer"))
                    player_script_moving = pl->script_auto_move;
                bool player_input_enabled = !player_script_moving &&
                    sc.player_control_enabled && !sc.cinema &&
                    sc.GlobalNumber("eventScene") == 0.0;
                // Resolve clocks independently of rendering. Lua may poll a
                // hidden actor or MainPlayer, and neither is allowed to wait
                // forever merely because the draw loop skipped it.
                for (auto& a : world.actors_mutable())
                    if (a.alive) actorMotion(a);
                world.TickLookTargets();
                world.TickScriptMoves(dt, [&](const mcf::Actor& a, float nx, float nz) {
                    if (!have_col) return false;
                    return col.BlockedXZ(a.pos[0] + room_org[0],
                                         a.pos[2] + room_org[2],
                                         nx + room_org[0], nz + room_org[2],
                                         a.pos[1] + room_org[1], 30.f,
                                         mcf::Collision::kWallMask);
                });
                for (const auto& a : world.actors())
                    if (a.script_distance_moved > 0.f &&
                        reported_script_moves.insert(a.handle).second)
                        lucent::info("world", "scripted movement began for {}",
                                     a.handle);
                for (const auto& a : world.actors())
                    if (a.script_map_hits > 0 &&
                        reported_script_map_hits.insert(a.handle).second)
                        lucent::info("world", "scripted map collision for {}",
                                     a.handle);
                world.TickMotions(dt * 30.f);
                if (player_script_moving) {
                    if (const auto* pl = world.Find("MainPlayer")) {
                        px = pl->pos[0] + room_org[0];
                        py = pl->pos[1] + room_org[1];
                        pz = pl->pos[2] + room_org[2];
                        const float lx = pl->pos[0];
                        const float lz = pl->pos[2];
                        // Authored PlMoveToChip destinations intentionally use
                        // a chip just beyond the room (for example x=10 in a
                        // ten-chip room). Scripted and manual movement must
                        // therefore feed the same transition owner.
                        if (lz < 0.f && pl->script_move_target[2] < 0.f)
                            requestRoomExit(0, lx, lz);
                        else if (lx > room_size.w &&
                                 pl->script_move_target[0] > room_size.w)
                            requestRoomExit(1, lx, lz);
                        else if (lz > room_size.h &&
                                 pl->script_move_target[2] > room_size.h)
                            requestRoomExit(2, lx, lz);
                        else if (lx < 0.f && pl->script_move_target[0] < 0.f)
                            requestRoomExit(3, lx, lz);
                    }
                }
                int nk = 0;
                const bool* key = SDL_GetKeyboardState(&nk);
                float mx = 0, my = 0, mz = 0;
                if (key) {
                    if (key[SDL_SCANCODE_LEFT]  || key[SDL_SCANCODE_A]) mx -= 1;
                    if (key[SDL_SCANCODE_RIGHT] || key[SDL_SCANCODE_D]) mx += 1;
                    if (key[SDL_SCANCODE_UP]    || key[SDL_SCANCODE_W]) mz -= 1;
                    if (key[SDL_SCANCODE_DOWN]  || key[SDL_SCANCODE_S]) mz += 1;
                }
                if (!player_input_enabled) mx = mz = 0.f;
                float headless_move_limit =
                    std::numeric_limits<float>::infinity();
                bool headless_lower_event_goal = false;
                const bool player_on_wall = [&] {
                    const auto* player = world.Find("MainPlayer");
                    return player &&
                           player->Get(mcf::chr_data::kFloorType) == 1.f;
                }();
                // Headless driver: steer toward a room-local target so the
                // walk-into-an-event-box path (which is how the game connects
                // rooms) can be exercised without a human at the keyboard.
                if (walk_to && player_input_enabled) {
                    float active_walk_x = walk_x, active_walk_z = walk_z;
                    std::vector<const mcf::EventBox*> active_event_walls;
                    std::vector<const mcf::EventBox*> active_goal_boxes;
                    if (opening_story) {
                        const int story_sccnt =
                            int(sc.GlobalNumber("sccnt", -1));
                        const bool escorting_heroine =
                            story_sccnt >= 12 && story_sccnt < 14;
                        if (room_name == "M0001_00_02" ||
                            room_name == "M0001_00_01") {
                            active_walk_x = 165.f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0001_01_03") {
                            active_walk_x = -30.f;
                            active_walk_z = 135.f;
                        } else if (room_name == "M0000_05_06" &&
                                   !continue_story) {
                            // The mandatory opening verifier's documented end
                            // is the playable waterfall recovery. A targeted
                            // continuation (`--stop-room`) may keep driving;
                            // the default gate must not wander into later
                            // overworld rooms merely because more traversal
                            // mechanics become available.
                            active_walk_x = px - room_org[0];
                            active_walk_z = pz - room_org[2];
                        } else if (room_name == "M0000_05_06" ||
                                   (room_name == "M0000_06_06" &&
                                    !bogard_house_visited)) {
                            active_walk_x = room_size.w + 30.f;
                            active_walk_z = 135.f;
                        } else if (room_name == "M0000_09_08" &&
                                   escorting_heroine) {
                            active_walk_x = -30.f;
                            active_walk_z = 135.f;
                        } else if (room_name == "M0000_09_08" &&
                                   story_sccnt >= 10 && story_sccnt < 12) {
                            bool live_enemy = false;
                            for (const auto& a : world.actors())
                                if (a.alive && !a.defeated &&
                                    mcf::CharType(a) == mcf::Actor::kEnemy) {
                                    live_enemy = true;
                                    break;
                                }
                            if (!live_enemy)
                                if (const auto* hasim = world.Find("hasim")) {
                                    active_walk_x = hasim->pos[0];
                                    active_walk_z = hasim->pos[2] + 20.f;
                                }
                        } else if (room_name == "M0000_08_08" &&
                                   escorting_heroine) {
                            active_walk_x = 150.f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0000_08_07" &&
                                   escorting_heroine) {
                            active_walk_x = 150.f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0000_08_06" &&
                                   escorting_heroine) {
                            active_walk_x = -30.f;
                            active_walk_z = 105.f;
                        } else if (room_name == "M0000_07_06" &&
                                   escorting_heroine) {
                            active_walk_x = 150.f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0000_07_06" &&
                                   !bogard_house_visited) {
                            active_walk_x = 150.f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0000_07_05" &&
                                   (!bogard_house_visited || escorting_heroine)) {
                            // The southern entrance is connected to the east
                            // vines through floors 0 and 90. Floor 150 joins
                            // the two branches, where the west vine reaches the
                            // y=180 ledge whose next authored cell is west.
                            active_walk_x = py < 140.f ? 225.f : 105.f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0000_07_04") {
                            if (bogard_house_visited && !escorting_heroine) {
                                // The west return vine exits into this upper
                                // cell. Cross its continuous y=180 floor and
                                // re-enter above the middle/east descent.
                                active_walk_x = 225.f;
                                active_walk_z = room_size.h + 30.f;
                            } else {
                                active_walk_x = -30.f;
                                active_walk_z = 180.f;
                            }
                        } else if (room_name == "M0000_06_04") {
                            active_walk_x = 285.f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0000_06_05" &&
                                   (!bogard_house_visited || escorting_heroine)) {
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    bx.name == "in_01") {
                                    active_goal_boxes.push_back(&bx);
                                    active_walk_x = (bx.lo[0] + bx.hi[0]) * .5f;
                                    active_walk_z = (bx.lo[2] + bx.hi[2]) * .5f;
                                    break;
                                }
                        } else if (room_name == "M0010_00_01") {
                            const bool greeted_bogard =
                                sc.GlobalNumber("tmp0") >= 1.0;
                            const char* wanted =
                                (greeted_bogard || story_sccnt >= 14) &&
                                !escorting_heroine &&
                                (story_sccnt != 14 || inventory.Has(17))
                                    ? "out_01" : nullptr;
                            if (wanted) {
                                for (const auto& bx : world.boxes)
                                    if (bx.enabled && !bx.no_touch &&
                                        bx.name == wanted) {
                                        active_goal_boxes.push_back(&bx);
                                        active_walk_x =
                                            (bx.lo[0] + bx.hi[0]) * .5f;
                                        active_walk_z =
                                            (bx.lo[2] + bx.hi[2]) * .5f;
                                        break;
                                    }
                            } else if (story_sccnt == 14 && !inventory.Has(17)) {
                                active_walk_x = room_size.w * .5f;
                                active_walk_z = -30.f;
                            } else if (const auto* npc = world.Find("NPC_01")) {
                                active_walk_x = npc->pos[0];
                                active_walk_z = npc->pos[2] + 20.f;
                            }
                        } else if (room_name == "M0010_00_00") {
                            if (!inventory.Has(17)) {
                                if (const auto* box = world.Find("_BOX")) {
                                    active_walk_x = box->pos[0];
                                    active_walk_z = box->pos[2] + 20.f;
                                }
                            } else {
                                active_walk_x = room_size.w * .5f;
                                active_walk_z = room_size.h + 30.f;
                            }
                        } else if (room_name == "M0000_06_05") {
                            active_walk_x = room_size.w + 30.f;
                            active_walk_z = 105.f;
                        } else if (room_name == "M0000_08_06" &&
                                   post_matock_cave_crossed) {
                            if (py > 100.f) {
                                active_walk_x = 225.f;
                                active_walk_z = 0.f;
                            } else if (py > 30.f) {
                                active_walk_x = 135.f;
                                active_walk_z = 60.f;
                            } else {
                                active_walk_x = 150.f;
                                active_walk_z = room_size.h + 30.f;
                            }
                        } else if (room_name == "M0000_08_06" &&
                                   story_sccnt == 14 && inventory.Has(17)) {
                            if (py < 50.f ||
                                (py < 100.f && post_matock_middle_crossed)) {
                                active_walk_x = post_matock_middle_crossed
                                    ? 225.f : 135.f;
                                active_walk_z = post_matock_middle_crossed
                                    ? 0.f : 60.f;
                            } else {
                                active_walk_x = py < 100.f ? 135.f : 225.f;
                                active_walk_z = -30.f;
                            }
                        } else if (room_name == "M0000_08_05" &&
                                   story_sccnt == 14 && inventory.Has(17)) {
                            if (post_matock_cave_crossed) {
                                // The cave exit reaches this cell's isolated
                                // y=150 southeast component. Measured route
                                // diagnostics find 93 reachable south-band
                                // samples, 56 east (the way back), and zero
                                // north/west; continue through the authored
                                // southern edge.
                                active_walk_x = 225.f;
                                active_walk_z = room_size.h + 30.f;
                            } else if (py < 100.f) {
                                post_matock_middle_crossed = true;
                                active_walk_x = 195.f;
                                active_walk_z = room_size.h + 30.f;
                            } else {
                                active_walk_x = room_size.w + 30.f;
                                active_walk_z = 210.f;
                            }
                        } else if (room_name == "M0000_09_05" &&
                                   story_sccnt == 14 && inventory.Has(17)) {
                            active_walk_x = post_matock_cave_crossed
                                ? -30.f : 195.f;
                            active_walk_z = post_matock_cave_crossed
                                ? 210.f : room_size.h + 30.f;
                        } else if (room_name == "M0000_09_06" &&
                                   story_sccnt == 14 && inventory.Has(17) &&
                                   py >= 85.f && !post_matock_cave_crossed) {
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    bx.name == "in_1") {
                                    active_goal_boxes.push_back(&bx);
                                    active_walk_x = (bx.lo[0] + bx.hi[0]) * .5f;
                                    active_walk_z = (bx.lo[2] + bx.hi[2]) * .5f;
                                    break;
                                }
                        } else if (room_name == "M0000_09_06" &&
                                   post_matock_cave_crossed) {
                            active_walk_x = 135.f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0000_09_08" &&
                                   post_matock_cave_crossed) {
                            active_walk_x = room_size.w + 30.f;
                            active_walk_z = 135.f;
                        } else if (room_name == "M0000_10_08" &&
                                   post_matock_cave_crossed) {
                            active_walk_x = 150.f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0000_10_09" &&
                                   post_matock_cave_crossed &&
                                   story_sccnt == 14) {
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    bx.name == "in_01") {
                                    active_goal_boxes.push_back(&bx);
                                    active_walk_x =
                                        (bx.lo[0] + bx.hi[0]) * .5f;
                                    active_walk_z =
                                        (bx.lo[2] + bx.hi[2]) * .5f;
                                    break;
                                }
                        } else if (room_name == "M0012_01_01" &&
                                   story_sccnt == 14) {
                            active_walk_x = -30.f;
                            active_walk_z = 135.f;
                        } else if (room_name == "M0012_00_01" &&
                                   story_sccnt == 14) {
                            active_walk_x = 150.f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0012_00_00" &&
                                   story_sccnt == 14) {
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    bx.name == "bed_01") {
                                    active_goal_boxes.push_back(&bx);
                                    active_walk_x =
                                        (bx.lo[0] + bx.hi[0]) * .5f;
                                    active_walk_z =
                                        (bx.lo[2] + bx.hi[2]) * .5f;
                                    break;
                                }
                        } else if (room_name == "M0012_00_00" &&
                                   story_sccnt >= 15) {
                            active_walk_x = room_size.w * .5f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0012_00_01" &&
                                   story_sccnt >= 15) {
                            active_walk_x = room_size.w + 30.f;
                            active_walk_z = room_size.h * .5f;
                        } else if (room_name == "M0012_01_01" &&
                                   story_sccnt >= 15) {
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    bx.name == "out_1") {
                                    active_goal_boxes.push_back(&bx);
                                    active_walk_x =
                                        (bx.lo[0] + bx.hi[0]) * .5f;
                                    active_walk_z =
                                        (bx.lo[2] + bx.hi[2]) * .5f;
                                    break;
                                }
                        } else if ((room_name == "M0000_10_09" ||
                                    room_name == "M0000_11_09" ||
                                    room_name == "M0000_12_09") &&
                                   story_sccnt == 15 && !inventory.Has(30)) {
                            active_walk_x = room_size.w + 30.f;
                            active_walk_z = room_size.h * .5f;
                        } else if (room_name == "M0000_13_09" &&
                                   story_sccnt == 15 && !inventory.Has(30)) {
                            active_walk_x = room_size.w * .5f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0000_13_10" &&
                                   story_sccnt == 15 && !inventory.Has(30)) {
                            if (const auto* box = world.Find("_BOX")) {
                                active_walk_x = box->pos[0];
                                active_walk_z = box->pos[2] + 20.f;
                            }
                        } else if ((room_name == "M0000_13_10" ||
                                    room_name == "M0000_13_09") &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            active_walk_x = room_size.w * .5f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0000_13_08" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            active_walk_x = room_size.w * .5f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0000_13_07" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            active_walk_x = room_size.w * .5f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0000_13_06" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            active_walk_x = room_size.w + 30.f;
                            active_walk_z = room_size.h * .5f;
                        } else if (room_name == "M0000_14_06" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            active_walk_x = room_size.w * .5f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0000_14_07" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            active_walk_x = room_size.w * .5f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0000_14_08" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    bx.name == "in_01") {
                                    active_goal_boxes.push_back(&bx);
                                    active_walk_x =
                                        (bx.lo[0] + bx.hi[0]) * .5f;
                                    active_walk_z =
                                        (bx.lo[2] + bx.hi[2]) * .5f;
                                    break;
                                }
                        } else if (room_name == "M0013_03_01" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            active_walk_x = room_size.w * .5f;
                            active_walk_z = -30.f;
                        } else if (room_name == "M0013_03_00" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            active_walk_x = -30.f;
                            active_walk_z = room_size.h * .5f;
                        } else if (room_name == "M0013_02_00" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            if (hydra_upper_bridge_crossed) {
                                active_walk_x = -30.f;
                                active_walk_z = room_size.h * .5f;
                            } else {
                                const mcf::EventBox* target = nullptr;
                                for (const auto& bx : world.boxes)
                                    if (bx.enabled && !bx.no_touch &&
                                        (bx.name == "down_1" ||
                                         (!target && bx.name == "sw_01")))
                                        target = &bx;
                                if (target) {
                                    active_goal_boxes.push_back(target);
                                    active_walk_x =
                                        (target->lo[0] + target->hi[0]) * .5f;
                                    active_walk_z =
                                        (target->lo[2] + target->hi[2]) * .5f;
                                }
                            }
                        } else if (room_name == "M0013_00_04" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    bx.name == "left_1") {
                                    active_goal_boxes.push_back(&bx);
                                    active_walk_x =
                                        (bx.lo[0] + bx.hi[0]) * .5f;
                                    active_walk_z =
                                        (bx.lo[2] + bx.hi[2]) * .5f;
                                    break;
                                }
                        } else if (room_name == "M0013_01_00" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            if (hydra_mountain_climbed) {
                                active_walk_x = -30.f;
                                active_walk_z = room_size.h * .5f;
                            } else {
                                for (const auto& bx : world.boxes)
                                    if (bx.enabled && !bx.no_touch &&
                                        bx.name == "down_01") {
                                        active_goal_boxes.push_back(&bx);
                                        active_walk_x =
                                            (bx.lo[0] + bx.hi[0]) * .5f;
                                        active_walk_z =
                                            (bx.lo[2] + bx.hi[2]) * .5f;
                                        break;
                                    }
                            }
                        } else if (room_name == "M0013_06_05" &&
                                   story_sccnt == 15 && inventory.Has(30)) {
                            if (player_on_wall) {
                                const auto it = std::find_if(
                                    world.boxes.begin(), world.boxes.end(),
                                    [&](const mcf::EventBox& bx) {
                                        return bx.enabled && !bx.no_touch &&
                                               bx.name == "up_01";
                                });
                                if (it != world.boxes.end()) {
                                    active_goal_boxes.push_back(&*it);
                                    active_walk_x =
                                        (it->lo[0] + it->hi[0]) * .5f;
                                    active_walk_z =
                                        (it->lo[2] + it->hi[2]) * .5f;
                                }
                            } else {
                                // Approach the lone WALL_UP which enters wall
                                // movement. It has no paired WALL_DOWN, so the
                                // ordinary event-wall router cannot select it.
                                const auto it = std::find_if(
                                    world.boxes.begin(), world.boxes.end(),
                                    [](const mcf::EventBox& bx) {
                                        return bx.enabled && !bx.no_touch &&
                                               (bx.flags &
                                                mcf::EventBox::kWallUp);
                                });
                                if (it != world.boxes.end()) {
                                    active_walk_x =
                                        (it->lo[0] + it->hi[0]) * .5f;
                                    active_walk_z =
                                        (it->lo[2] + it->hi[2]) * .5f;
                                }
                            }
                        } else if ((room_name == "M0011_00_00" ||
                                    room_name == "M0011_00_01") &&
                                   story_sccnt == 14 && inventory.Has(17)) {
                            active_walk_x = 150.f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0011_00_02" &&
                                   story_sccnt == 14 && inventory.Has(17)) {
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    bx.name == "in_1") {
                                    active_goal_boxes.push_back(&bx);
                                    active_walk_x = (bx.lo[0] + bx.hi[0]) * .5f;
                                    active_walk_z = (bx.lo[2] + bx.hi[2]) * .5f;
                                    break;
                                }
                        } else if (room_name == "M0000_07_05") {
                            active_walk_x = 150.f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0000_07_06" &&
                                   bogard_house_visited) {
                            active_walk_x = room_size.w + 30.f;
                            // The destination edge has authored y=0 bands at
                            // z<=112.5 and z>=150, separated by an isolated
                            // y=-15 pocket. Cross through the measured lower
                            // connected band, on the 7.5-unit ground lattice.
                            active_walk_z = 105.f;
                        } else if (room_name == "M0000_08_06") {
                            active_walk_x = 150.f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0000_08_07") {
                            active_walk_x = 150.f;
                            active_walk_z = room_size.h + 30.f;
                        } else if (room_name == "M0000_08_08") {
                            active_walk_x = room_size.w + 30.f;
                            active_walk_z = 135.f;
                        } else if (room_name == "M0001_00_00") {
                            // After the second Jackal, EnemyDead authors two
                            // joined out_01 boxes. Drive their actual centre;
                            // before they exist, retain the proven EX_1 combat
                            // boundary target.
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    bx.name == "out_01") {
                                    active_walk_x = (bx.lo[0] + bx.hi[0]) * .5f;
                                    active_walk_z = (bx.lo[2] + bx.hi[2]) * .5f;
                                    break;
                                }
                        }

                        if (active_goal_boxes.empty() ||
                            (!driver_route_room.empty() &&
                             driver_route_room != room_name))
                            driver_route_descending = false;
                        const bool target_starts_below = std::any_of(
                            active_goal_boxes.begin(),
                            active_goal_boxes.end(),
                            [&](const mcf::EventBox* box) {
                                return box->hi[1] <=
                                       py + kEventBoxProbeHeight + .01f;
                            });
                        if (target_starts_below)
                            driver_route_descending = true;
                        headless_lower_event_goal = driver_route_descending;

                        // Multi-level overworld rooms author paired WALL_UP /
                        // WALL_DN volumes. Approach the direction belonging to
                        // both the player's current floor and story objective;
                        // the return from Bogard's elevated house must descend
                        // the same vine the outward route climbed elsewhere.
                        const bool returning_through_vines =
                            bogard_house_visited && !escorting_heroine &&
                            room_name == "M0000_07_05";
                        if (returning_through_vines && py >= 175.f)
                            bogard_return_reached_vine_summit = true;
                        const bool seek_wall_down =
                            (bogard_house_visited && !escorting_heroine &&
                             room_name == "M0000_06_05") ||
                            (post_matock_cave_crossed &&
                             room_name == "M0000_08_06" && py > 30.f) ||
                            (returning_through_vines &&
                             bogard_return_reached_vine_summit);
                        const bool seek_wall_up =
                            (room_name == "M0000_07_05" &&
                             (!bogard_house_visited || escorting_heroine ||
                              !bogard_return_reached_vine_summit)) ||
                            (room_name == "M0000_09_06" &&
                             post_matock_cave_crossed) ||
                            (room_name == "M0000_08_06" &&
                             !post_matock_cave_crossed && story_sccnt == 14 &&
                             inventory.Has(17) &&
                             (py < 50.f ||
                              (py < 100.f && post_matock_middle_crossed)));
                        for (const auto& bx : world.boxes) {
                            if (!bx.enabled || bx.no_touch ||
                                (!seek_wall_down && !seek_wall_up) ||
                                !(bx.flags & (seek_wall_down
                                    ? mcf::EventBox::kWallDown
                                    : mcf::EventBox::kWallUp)) ||
                                (seek_wall_down
                                    ? (py < bx.lo[1] || py > bx.hi[1])
                                    : (py < bx.lo[1] - 20.f ||
                                       py > bx.hi[1] + 5.f)))
                                continue;
                            active_event_walls.push_back(&bx);
                        }

                        // The headless driver is an instrument: route through
                        // the shipping floor and wall queries instead of
                        // assuming that a straight line, or even a 30-unit
                        // chip-centre line, represents a traversable corridor.
                        // The 7.5-unit lattice is the room ground-attribute
                        // resolution and preserves doorways that lie between
                        // the coarser AI chip centres.
                        const bool outside_left = active_walk_x < 0.f;
                        const bool outside_right =
                            active_walk_x >= room_size.w;
                        const bool outside_up = active_walk_z < 0.f;
                        const bool outside_down =
                            active_walk_z >= room_size.h;
                        const bool route_changed =
                            driver_route_room != room_name ||
                            driver_route_goal_x != active_walk_x ||
                            driver_route_goal_z != active_walk_z;
                        if (route_changed && player_on_wall &&
                            !active_goal_boxes.empty()) {
                            // Floor type 1 is the shipping wall/ivy movement
                            // plane. AppCharacterBase::Update compares X/Y in
                            // this mode instead of X/Z, and the authored
                            // callback pins Z to the wall plane.
                            driver_route.clear();
                            const float target_y =
                                (active_goal_boxes.front()->lo[1] +
                                 active_goal_boxes.front()->hi[1]) * .5f -
                                kEventBoxProbeHeight;
                            driver_route.push_back({active_walk_x, target_y,
                                                    pz - room_org[2]});
                            driver_route_room = room_name;
                            driver_route_goal_x = active_walk_x;
                            driver_route_goal_z = active_walk_z;
                            lucent::info("host", "opening wall-plane route in {} "
                                         "to '{}' at x={:.1f}, fixed z={:.1f}",
                                         room_name, active_goal_boxes.front()->name,
                                         active_walk_x, pz - room_org[2]);
                        } else if (have_col && route_changed) {
                            driver_route_room = room_name;
                            driver_route_goal_x = active_walk_x;
                            driver_route_goal_z = active_walk_z;
                            driver_route.clear();
                            driver_route_is_staging = false;
                            driver_route_contact_x = active_walk_x;
                            driver_route_contact_z = active_walk_z;
                            driver_route_through_x = active_walk_x;
                            driver_route_through_z = active_walk_z;
                            constexpr float kNavStep = 7.5f;
                            const int nav_w = int(std::lround(room_size.w / kNavStep)) + 1;
                            const int nav_h = int(std::lround(room_size.h / kNavStep)) + 1;
                            auto nav_x = [&](int x) { return room_org[0] + x * kNavStep; };
                            auto nav_z = [&](int z) { return room_org[2] + z * kNavStep; };
                            int sx = std::clamp(int(std::lround(
                                (px - room_org[0]) / kNavStep)), 0, nav_w - 1);
                            int sz = std::clamp(int(std::lround(
                                (pz - room_org[2]) / kNavStep)), 0, nav_h - 1);
                            int start = sz * nav_w + sx;
                            std::vector<float> height(size_t(nav_w) * size_t(nav_h),
                                                      kChipNoFloor);
                            std::vector<int> prev(height.size(), -1);
                            std::vector<int> dist(height.size(), -1);
                            float nav_query_y = py + 30.f;
                            for (int z = 0; z < nav_h; ++z)
                                for (int x = 0; x < nav_w; ++x) {
                                    const int sample = z * nav_w + x;
                                    const bool any_boundary =
                                        x == 0 || x == nav_w - 1 ||
                                        z == 0 || z == nav_h - 1;
                                    const bool unrelated_boundary =
                                        (x == 0 && !outside_left) ||
                                        (x == nav_w - 1 && !outside_right) ||
                                        (z == 0 && !outside_up) ||
                                        (z == nav_h - 1 && !outside_down);
                                    // The arrival itself may be on the opposite
                                    // boundary; retain that one node but never
                                    // let routing use an unrelated room edge as
                                    // a shortcut.
                                    if (sample != start &&
                                        ((active_event_walls.empty() && any_boundary) ||
                                         (returning_through_vines &&
                                          unrelated_boundary)))
                                        continue;
                                    float floor;
                                    if (col.GetFloorBelow(nav_x(x), nav_z(z),
                                                          nav_query_y,
                                                          mcf::Collision::kFloorMask,
                                                          &floor))
                                        height[size_t(sample)] = floor;
                                }
                            // A live position can sit between lattice samples
                            // on one side of a floor seam while nearest-round
                            // lands on the other side. Attach it to the nearest
                            // sampled node on the same floor through a clear
                            // segment; otherwise BFS starts in the wrong
                            // connected component despite valid live ground.
                            float live_floor = py;
                            float best_attach =
                                std::numeric_limits<float>::infinity();
                            int attached = -1;
                            for (int i = 0; i < int(height.size()); ++i) {
                                if (height[size_t(i)] >= kChipNoFloor ||
                                    std::fabs(height[size_t(i)] - live_floor) >= 5.f)
                                    continue;
                                const float cx = nav_x(i % nav_w);
                                const float cz = nav_z(i / nav_w);
                                const float dx = cx - px;
                                const float dz = cz - pz;
                                const float d2 = dx * dx + dz * dz;
                                if (d2 >= best_attach ||
                                    col.BlockedXZ(px, pz, cx, cz, live_floor,
                                                  30.f,
                                                  mcf::Collision::kWallMask) ||
                                    objectBlockedXZ(px, pz, cx, cz, live_floor,
                                                    nullptr))
                                    continue;
                                best_attach = d2;
                                attached = i;
                            }
                            if (attached >= 0) {
                                start = attached;
                                sx = start % nav_w;
                                sz = start / nav_w;
                            }
                            std::queue<int> pending;
                            if (height[size_t(start)] < kChipNoFloor) {
                                dist[size_t(start)] = 0;
                                pending.push(start);
                            }
                            static constexpr int kDx[8] =
                                {1, -1, 0, 0, 1, 1, -1, -1};
                            static constexpr int kDz[8] =
                                {0, 0, 1, -1, 1, -1, 1, -1};
                            const int nav_neighbor_count =
                                active_goal_boxes.empty() ? 4 : 8;
                            while (!pending.empty()) {
                                const int here = pending.front();
                                pending.pop();
                                const int hx = here % nav_w;
                                const int hz = here / nav_w;
                                for (int k = 0; k < nav_neighbor_count; ++k) {
                                    const int nx = hx + kDx[k];
                                    const int nz = hz + kDz[k];
                                    if (nx < 0 || nz < 0 || nx >= nav_w || nz >= nav_h)
                                        continue;
                                    const int next = nz * nav_w + nx;
                                    if (dist[size_t(next)] >= 0 ||
                                        height[size_t(next)] >= kChipNoFloor)
                                        continue;
                                    float mid_floor;
                                    if (!col.GetFloorBelow(
                                            (nav_x(hx) + nav_x(nx)) * .5f,
                                            (nav_z(hz) + nav_z(nz)) * .5f,
                                            height[size_t(here)] + 30.f,
                                            mcf::Collision::kFloorMask,
                                            &mid_floor))
                                        continue;
                                    const auto event_link = world.FindEventWall(
                                        nav_x(hx), nav_z(hz), nav_x(nx), nav_z(nz),
                                        height[size_t(here)], room_org[0], room_org[2]);
                                    const bool stair_step =
                                        (height[size_t(next)] >
                                             height[size_t(here)] + .01f &&
                                         height[size_t(next)] -
                                             height[size_t(here)] <= 30.f) ||
                                        (headless_lower_event_goal &&
                                         height[size_t(here)] >
                                             height[size_t(next)] + .01f &&
                                         height[size_t(here)] -
                                             height[size_t(next)] <= 30.f);
                                    bool edge_wall = false;
                                    bool edge_object = false;
                                    // Begin at the actual lattice node. Moving
                                    // the validator backward across a collision
                                    // boundary can make a blocked diagonal look
                                    // clear even though shipping movement starts
                                    // exactly on that boundary. The one
                                    // exception is the first edge of a lower-
                                    // event route: its rounded node can lie on
                                    // the opposite side of a slope wall from
                                    // the live centre, so retain the live point.
                                    const float edge_start_x =
                                        headless_lower_event_goal && here == start
                                            ? px : nav_x(hx);
                                    const float edge_start_z =
                                        headless_lower_event_goal && here == start
                                            ? pz : nav_z(hz);
                                    const float edge_dx = nav_x(nx) - edge_start_x;
                                    const float edge_dz = nav_z(nz) - edge_start_z;
                                    const float edge_length = std::sqrt(
                                        edge_dx * edge_dx + edge_dz * edge_dz);
                                    const float route_move_step = kWalk / 30.f;
                                    const int edge_sweep_samples = std::max(
                                        1, int(std::ceil(edge_length /
                                                        route_move_step)));
                                    float edge_floor = height[size_t(here)];
                                    for (int sample = 1;
                                         sample <= edge_sweep_samples; ++sample) {
                                        const float t0 = std::min(
                                            1.f, float(sample - 1) *
                                                     route_move_step /
                                                     edge_length);
                                        const float t1 = std::min(
                                            1.f, float(sample) * route_move_step /
                                                     edge_length);
                                        const float ax = std::lerp(
                                            edge_start_x, nav_x(nx), t0);
                                        const float az = std::lerp(
                                            edge_start_z, nav_z(nz), t0);
                                        const float bx = std::lerp(
                                            edge_start_x, nav_x(nx), t1);
                                        const float bz = std::lerp(
                                            edge_start_z, nav_z(nz), t1);
                                        if (headless_lower_event_goal) {
                                            float sample_floor = 0.f;
                                            const bool floor_found =
                                                col.GetFloorBelow(
                                                    bx, bz, edge_floor + 30.f,
                                                    mcf::Collision::kFloorMask,
                                                    &sample_floor);
                                            const bool sample_step = floor_found &&
                                                std::fabs(sample_floor - edge_floor) >
                                                    .01f &&
                                                std::fabs(sample_floor - edge_floor) <=
                                                    30.f;
                                            if (!floor_found ||
                                                std::fabs(sample_floor - edge_floor) >
                                                    30.f ||
                                                (!sample_step && col.BlockedXZ(
                                                    ax, az, bx, bz, edge_floor,
                                                    30.f,
                                                    mcf::Collision::kWallMask)))
                                                edge_wall = true;
                                            if (floor_found)
                                                edge_floor = sample_floor;
                                        } else if (!stair_step && col.BlockedXZ(
                                                       ax, az, bx, bz,
                                                       height[size_t(here)], 30.f,
                                                       mcf::Collision::kWallMask)) {
                                            edge_wall = true;
                                        }
                                        if (objectBlockedXZ(
                                                ax, az, bx, bz,
                                                headless_lower_event_goal
                                                    ? edge_floor
                                                    : height[size_t(here)],
                                                nullptr))
                                            edge_object = true;
                                    }
                                    if ((std::fabs(mid_floor -
                                                   height[size_t(here)]) > 30.f ||
                                         std::fabs(height[size_t(next)] -
                                                   mid_floor) > 30.f ||
                                         std::fabs(height[size_t(next)] -
                                                   height[size_t(here)]) > 30.f ||
                                         edge_wall || edge_object) &&
                                        !event_link.source)
                                        continue;
                                    dist[size_t(next)] = dist[size_t(here)] + 1;
                                    prev[size_t(next)] = here;
                                    pending.push(next);
                                }
                            }
                            int goal = -1;
                            float best = std::numeric_limits<float>::infinity();
                            int reached = 0;
                            int reachable_side[4]{};
                            for (int i = 0; i < int(dist.size()); ++i) {
                                if (dist[size_t(i)] < 0) continue;
                                ++reached;
                                const int gx = i % nav_w;
                                const int gz = i / nav_w;
                                // Manual transition detection consumes a
                                // blocked outward step from within one chip of
                                // the requested side; the static boundary wall
                                // itself is intentionally not a reachable nav
                                // sample. Route to that contact band, then let
                                // the unchanged outside target exercise the
                                // transition.
                                // Stop the planned path within two chips of
                                // the side. The live mover then closes the
                                // remaining distance until its blocked step
                                // enters the one-chip transition contact band.
                                // Requiring a sampled node inside that band is
                                // wrong: the static wall may be inset between
                                // lattice points even though a continuous
                                // approach reaches the transition probe.
                                constexpr float kExitApproach = 60.f;
                                const float cx = gx * kNavStep;
                                const float cz = gz * kNavStep;
                                if (cz <= kExitApproach) ++reachable_side[0];
                                if (cx >= room_size.w - kExitApproach)
                                    ++reachable_side[1];
                                if (cz >= room_size.h - kExitApproach)
                                    ++reachable_side[2];
                                if (cx <= kExitApproach) ++reachable_side[3];
                                if (active_event_walls.empty() &&
                                    ((outside_left && cx > kExitApproach) ||
                                     (outside_right &&
                                      cx < room_size.w - kExitApproach) ||
                                     (outside_up && cz > kExitApproach) ||
                                     (outside_down &&
                                      cz < room_size.h - kExitApproach)))
                                    continue;
                                bool in_wall_goal = active_event_walls.empty();
                                if (!in_wall_goal)
                                    for (const auto* bx : active_event_walls)
                                        if (cx > bx->lo[0] && cx < bx->hi[0] &&
                                            cz > bx->lo[2] && cz < bx->hi[2]) {
                                            in_wall_goal = true;
                                            break;
                                        }
                                if (!in_wall_goal) continue;
                                const float dx = cx - active_walk_x;
                                const float dz = cz - active_walk_z;
                                bool in_goal_box = active_goal_boxes.empty();
                                if (!in_goal_box)
                                    for (const auto* bx : active_goal_boxes)
                                        if (EventBoxCharacterContact(
                                                *bx, room_org[0] + cx,
                                                height[size_t(i)] +
                                                    kEventBoxProbeHeight,
                                                room_org[2] + cz,
                                                room_org[0], room_org[2],
                                                kCharacterCollisionRadius)) {
                                            in_goal_box = true;
                                            break;
                                        }
                                if (!in_goal_box) continue;
                                // A room-local point objective is an
                                // interaction/contact target, not permission
                                // to accept the nearest connected component.
                                // One chip is the same measured reach used by
                                // NPC and chest interaction below.
                                // For a staircase choose the reachable volume
                                // that advances toward the eventual room goal.
                                // M0000_07_05 has east and west vine branches;
                                // shortest-from-player selects the wrong one,
                                // while both the goal and reachability are
                                // authored data.
                                const float score = dx * dx + dz * dz;
                                if (score < best) {
                                    best = score;
                                    goal = i;
                                }
                            }
                            if (goal < 0 && !active_goal_boxes.empty() &&
                                !headless_lower_event_goal) {
                                float best_stage_height = py + 5.f;
                                float best_stage_distance =
                                    std::numeric_limits<float>::infinity();
                                for (int i = 0; i < int(dist.size()); ++i) {
                                    if (dist[size_t(i)] < 0 ||
                                        height[size_t(i)] < best_stage_height)
                                        continue;
                                    const float cx = (i % nav_w) * kNavStep;
                                    const float cz = (i / nav_w) * kNavStep;
                                    const float dx = cx - active_walk_x;
                                    const float dz = cz - active_walk_z;
                                    const float d2 = dx * dx + dz * dz;
                                    if (height[size_t(i)] >
                                            best_stage_height + .01f ||
                                        d2 < best_stage_distance) {
                                        best_stage_height = height[size_t(i)];
                                        best_stage_distance = d2;
                                        goal = i;
                                    }
                                }
                                if (goal >= 0) {
                                    driver_route_is_staging = true;
                                    driver_route_staging_floor =
                                        height[size_t(goal)];
                                    lucent::info("host", "opening route in {} "
                                                 "stages toward a vertical "
                                                 "goal via floor {:.1f}",
                                                 room_name,
                                                 height[size_t(goal)]);
                                }
                            }
                            if (goal < 0) {
                                float max_sampled = -std::numeric_limits<float>::infinity();
                                float max_reachable = -std::numeric_limits<float>::infinity();
                                float min_reachable_x =
                                    std::numeric_limits<float>::infinity();
                                float max_reachable_x =
                                    -std::numeric_limits<float>::infinity();
                                float min_reachable_z =
                                    std::numeric_limits<float>::infinity();
                                float max_reachable_z =
                                    -std::numeric_limits<float>::infinity();
                                int rise_frontier = 0;
                                int rise_low_wall = 0;
                                int rise_high_wall = 0;
                                int rise_object = 0;
                                for (int i = 0; i < int(height.size()); ++i) {
                                    if (height[size_t(i)] < kChipNoFloor)
                                        max_sampled = std::max(
                                            max_sampled, height[size_t(i)]);
                                    if (dist[size_t(i)] >= 0) {
                                        max_reachable = std::max(
                                            max_reachable, height[size_t(i)]);
                                        const float cx = (i % nav_w) * kNavStep;
                                        const float cz = (i / nav_w) * kNavStep;
                                        min_reachable_x = std::min(min_reachable_x, cx);
                                        max_reachable_x = std::max(max_reachable_x, cx);
                                        min_reachable_z = std::min(min_reachable_z, cz);
                                        max_reachable_z = std::max(max_reachable_z, cz);
                                    }
                                    if (dist[size_t(i)] < 0) continue;
                                    const int ix = i % nav_w;
                                    const int iz = i / nav_w;
                                    for (int k = 0; k < nav_neighbor_count; ++k) {
                                        const int nx = ix + kDx[k];
                                        const int nz = iz + kDz[k];
                                        if (nx < 0 || nz < 0 || nx >= nav_w ||
                                            nz >= nav_h)
                                            continue;
                                        const int next = nz * nav_w + nx;
                                        const float rise = height[size_t(next)] -
                                                           height[size_t(i)];
                                        if (dist[size_t(next)] >= 0 || rise <= .01f ||
                                            rise > 30.f)
                                            continue;
                                        ++rise_frontier;
                                        if (col.BlockedXZ(
                                                nav_x(ix), nav_z(iz), nav_x(nx),
                                                nav_z(nz), height[size_t(i)], 30.f,
                                                mcf::Collision::kWallMask))
                                            ++rise_low_wall;
                                        if (col.BlockedXZ(
                                                nav_x(ix), nav_z(iz), nav_x(nx),
                                                nav_z(nz), height[size_t(next)], 30.f,
                                                mcf::Collision::kWallMask))
                                            ++rise_high_wall;
                                        if (objectBlockedXZ(
                                                nav_x(ix), nav_z(iz), nav_x(nx),
                                                nav_z(nz), height[size_t(i)], nullptr))
                                            ++rise_object;
                                    }
                                }
                                lucent::error("host",
                                    "opening route from nav ({},{}) scanned {} "
                                    "of {} samples but found no reachable target "
                                    "for ({:.1f},{:.1f}) in {}; reachable "
                                    "contact-band samples up/right/down/left="
                                    "{}/{}/{}/{}; reachable local extent x="
                                    "{:.1f}..{:.1f}, z={:.1f}..{:.1f}; max "
                                    "sampled/reachable floor="
                                    "{:.1f}/{:.1f}; rise frontier={} (low-wall "
                                    "{}, high-wall {}, object {})",
                                    sx, sz, reached, height.size(),
                                    active_walk_x, active_walk_z, room_name,
                                    reachable_side[0], reachable_side[1],
                                    reachable_side[2], reachable_side[3],
                                    min_reachable_x, max_reachable_x,
                                    min_reachable_z, max_reachable_z,
                                    max_sampled, max_reachable, rise_frontier,
                                    rise_low_wall, rise_high_wall, rise_object);
                                run_failed = true;
                                running = false;
                                mx = mz = 0.f;
                            } else {
                                driver_route_contact_x = float(goal % nav_w) * kNavStep;
                                driver_route_contact_z = float(goal / nav_w) * kNavStep;
                                if (outside_left) {
                                    driver_route_through_x = -30.f;
                                    driver_route_through_z =
                                        driver_route_contact_z;
                                } else if (outside_right) {
                                    driver_route_through_x = room_size.w + 30.f;
                                    driver_route_through_z =
                                        driver_route_contact_z;
                                } else if (outside_up) {
                                    driver_route_through_x =
                                        driver_route_contact_x;
                                    driver_route_through_z = -30.f;
                                } else if (outside_down) {
                                    driver_route_through_x =
                                        driver_route_contact_x;
                                    driver_route_through_z =
                                        room_size.h + 30.f;
                                }
                            }
                            if (goal >= 0 && goal != start) {
                                std::vector<int> reverse_path;
                                for (int step = goal; step != start;
                                     step = prev[size_t(step)])
                                    reverse_path.push_back(step);
                                if (!active_event_walls.empty()) {
                                    const int approach = prev[size_t(goal)];
                                    const float approach_x =
                                        float((goal % nav_w) - (approach % nav_w));
                                    const float approach_z =
                                        float((goal / nav_w) - (approach / nav_w));
                                    const float approach_length = std::sqrt(
                                        approach_x * approach_x +
                                        approach_z * approach_z);
                                    driver_route_through_x =
                                        driver_route_contact_x + approach_x /
                                        approach_length * 30.f;
                                    driver_route_through_z =
                                        driver_route_contact_z + approach_z /
                                        approach_length * 30.f;
                                }
                                int prior = start;
                                float route_x = px - room_org[0];
                                float route_z = pz - room_org[2];
                                for (auto it = reverse_path.rbegin();
                                     it != reverse_path.rend(); ++it) {
                                    const int node = *it;
                                    if (node % nav_w != prior % nav_w)
                                        route_x = float(node % nav_w) * kNavStep;
                                    if (node / nav_w != prior / nav_w)
                                        route_z = float(node / nav_w) * kNavStep;
                                    driver_route.push_back({
                                        route_x, height[size_t(node)], route_z});
                                    prior = node;
                                }
                                float check_x = px;
                                float check_z = pz;
                                float check_floor = py;
                                bool compressed_clear = true;
                                for (const auto& waypoint : driver_route) {
                                    const float next_x = room_org[0] + waypoint.x;
                                    const float next_z = room_org[2] + waypoint.z;
                                    // Direction-only compression can cut across
                                    // the ownership boundary between stacked
                                    // floor triangles. Preserve the exact BFS
                                    // lattice whenever a segment changes floor;
                                    // the shipping mover must traverse that
                                    // authored stair shape one edge at a time.
                                    if (std::fabs(waypoint.y - check_floor) > .01f) {
                                        compressed_clear = false;
                                        break;
                                    }
                                    const float segment_x = next_x - check_x;
                                    const float segment_z = next_z - check_z;
                                    const float segment_length = std::sqrt(
                                        segment_x * segment_x + segment_z * segment_z);
                                    // Compression is only valid if the shipping
                                    // mover can execute it. Validate at its
                                    // fixed-step distance rather than testing
                                    // one long ray at the room-entry floor: a
                                    // long ray can cross a wall whose collision
                                    // becomes visible after the floor changes.
                                    const float segment_start_x = check_x;
                                    const float segment_start_z = check_z;
                                    const float kShippingMoveStep = kWalk / 30.f;
                                    const int samples = std::max(
                                        1, int(std::ceil(segment_length /
                                                        kShippingMoveStep)));
                                    for (int sample = 1; sample <= samples; ++sample) {
                                        const float t = float(sample) / float(samples);
                                        const float sample_x = segment_start_x +
                                                               segment_x * t;
                                        const float sample_z = segment_start_z +
                                                               segment_z * t;
                                        float sample_floor;
                                        if (!col.GetFloorBelow(
                                                sample_x, sample_z,
                                                check_floor + 30.f,
                                                mcf::Collision::kFloorMask,
                                                &sample_floor) ||
                                            std::fabs(sample_floor - check_floor) > 30.f) {
                                            compressed_clear = false;
                                            break;
                                        }
                                        const bool stair_step =
                                            sample_floor > check_floor + .01f;
                                        if ((!stair_step && col.BlockedXZ(
                                                 check_x, check_z,
                                                 sample_x, sample_z,
                                                 check_floor, 30.f,
                                                 mcf::Collision::kWallMask)) ||
                                            objectBlockedXZ(
                                                check_x, check_z,
                                                sample_x, sample_z,
                                                check_floor, nullptr)) {
                                            compressed_clear = false;
                                            break;
                                        }
                                        check_x = sample_x;
                                        check_z = sample_z;
                                        check_floor = sample_floor;
                                    }
                                    if (!compressed_clear) break;
                                }
                                if (!compressed_clear) {
                                    driver_route.clear();
                                    const float attach_x = float(start % nav_w) * kNavStep;
                                    const float attach_z = float(start / nav_w) * kNavStep;
                                    if (std::fabs((px - room_org[0]) - attach_x) > .01f ||
                                        std::fabs((pz - room_org[2]) - attach_z) > .01f)
                                        driver_route.push_back(
                                            {attach_x, py, attach_z});
                                    for (auto it = reverse_path.rbegin();
                                         it != reverse_path.rend(); ++it)
                                        driver_route.push_back({
                                            float(*it % nav_w) * kNavStep,
                                            height[size_t(*it)],
                                            float(*it / nav_w) * kNavStep});
                                    lucent::info("host", "compressed opening route in {} "
                                                 "failed shipping movement; emitted {} "
                                                 "exact lattice waypoint(s)",
                                                 room_name, driver_route.size());
                                }
                            }
                            if (goal >= 0 && opening_story)
                                lucent::info("host", "opening route in {}: start ({},{}) "
                                             "reached {}/{} -> goal ({},{}) contact "
                                             "({:.1f},{:.1f}), {} waypoint(s)",
                                             room_name, sx, sz, reached, height.size(),
                                             goal % nav_w, goal / nav_w,
                                             driver_route_contact_x,
                                             driver_route_contact_z,
                                             driver_route.size());
                        }
                        const float waypoint_reach =
                            (mapjump_floor_owner ||
                             (driver_route_is_staging &&
                              py >= driver_route_staging_floor - 5.f))
                            ? kWalk * dt : .1f;
                        while (!driver_route.empty()) {
                            const float dx = driver_route.front().x -
                                             (px - room_org[0]);
                            const float dy = driver_route.front().y - py;
                            const float dz = driver_route.front().z -
                                             (pz - room_org[2]);
                            // A map-jump arrival lattice node can lie exactly
                            // on the collision shell which owns the temporary
                            // arrival floor. The mover cannot close its final
                            // sub-step into that shell, so use one simulation
                            // step only while that ownership is active. Normal
                            // routes retain their exact 0.1-unit discipline;
                            // widening every waypoint cuts dungeon corners.
                            if (dx * dx + dz * dz +
                                    (player_on_wall ? dy * dy : 0.f) >
                                waypoint_reach * waypoint_reach)
                                break;
                            // A lattice sample and live triangle interpolation
                            // can legitimately differ by one 7.5-unit ground
                            // cell on a slope. They are different connected
                            // components only at the shipping 30-unit step
                            // boundary, not at an arbitrary 5-unit tolerance.
                            if (!player_on_wall &&
                                std::fabs(driver_route.front().y - py) >= 30.f) {
                                lucent::info(
                                    "host", "opening route in {} reached "
                                    "waypoint ({:.1f},{:.1f}) on floor {:.1f}, "
                                    "planned {:.1f}; rebuilding from live state",
                                    room_name, driver_route.front().x,
                                    driver_route.front().z,
                                    py - room_org[1],
                                    driver_route.front().y - room_org[1]);
                                driver_route.clear();
                                driver_route_room.clear();
                                break;
                            }
                            driver_route.pop_front();
                        }
                        if (!driver_route.empty()) {
                            active_walk_x = driver_route.front().x;
                            active_walk_z = driver_route.front().z;
                        } else if (driver_route_is_staging) {
                            driver_route_room.clear();
                            driver_route_is_staging = false;
                            active_walk_x = px - room_org[0];
                            active_walk_z = pz - room_org[2];
                        } else if (!active_event_walls.empty() || outside_left ||
                                   outside_right || outside_up || outside_down) {
                            active_walk_x = driver_route_through_x;
                            active_walk_z = driver_route_through_z;
                        }
                    }
                    float tx = active_walk_x + room_org[0];
                    float tz = active_walk_z + room_org[2];
                    float dx = tx - px, dz = tz - pz;
                    const float dy = player_on_wall && !driver_route.empty()
                        ? driver_route.front().y - py : 0.f;
                    if (dx * dx + dy * dy + dz * dz > .01f) {
                        mx = dx;
                        my = dy;
                        mz = dz;
                        headless_move_limit =
                            std::sqrt(dx * dx + dy * dy + dz * dz);
                    }
                    else if (auto_attack) auto_attack_armed = true;
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
                } else if (player_input_enabled &&
                           (confirm_edge || (auto_talk && frames % 30 == 0))) {
                    // ModeGame::AddBox creates an NPC-like `_BOX` actor whose
                    // four payload slots all contain the authored item. The
                    // player activates it at the same one-chip interaction
                    // reach used below. Inventory refusal leaves it closed;
                    // success invokes the room's `_BOX` completion callback.
                    constexpr float kTalkReach = 30.f;
                    mcf::Actor* nearest_box = nullptr;
                    float nearest_box_d2 = kTalkReach * kTalkReach;
                    for (auto& a : world.actors_mutable()) {
                        if (!a.alive || !a.treasure_box || a.treasure_open)
                            continue;
                        const float dx = a.pos[0] + room_org[0] - px;
                        const float dz = a.pos[2] + room_org[2] - pz;
                        const float d2 = dx * dx + dz * dz;
                        if (d2 < nearest_box_d2) {
                            nearest_box_d2 = d2;
                            nearest_box = &a;
                        }
                    }
                    if (nearest_box) {
                        if (!inventory.Add(nearest_box->treasure_item, true)) {
                            lucent::error("inventory", "box item {} refused: bag full or invalid",
                                          nearest_box->treasure_item);
                        } else {
                            if (opening_story &&
                                nearest_box->treasure_item == 30) {
                                if (!inventory.Equip(4, 30)) {
                                    lucent::error("inventory", "headless Silver Key "
                                                  "equip failed after acquisition");
                                    run_failed = true;
                                } else {
                                    lucent::info("inventory", "equipped Silver Key "
                                                 "item 30 in sub-item slot 4");
                                }
                            }
                            nearest_box->treasure_open = true;
                            // The pre-Bogard return route has completed. A
                            // post-chest traversal of the same multi-level
                            // cell starts from its lower component and must
                            // seek the upward half before it can descend the
                            // opposite branch.
                            bogard_return_reached_vine_summit = false;
                            post_matock_middle_crossed = false;
                            lucent::info("inventory", "opened box and acquired item {}",
                                         nearest_box->treasure_item);
                            if (sc.HasFunction("_BOX") && !sc.StartCoroutine("_BOX"))
                                lucent::error("lua", "_BOX coroutine: {}", sc.last_error());
                        }
                    } else if (!(opening_story && room_name == "M0010_00_01" &&
                                 int(sc.GlobalNumber("sccnt", -1)) >= 14) &&
                               !(opening_story && room_name == "M0001_00_02" &&
                                 fallman_talked)) {
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
                        {
                            if (best->handle == "FALLMAN") fallman_talked = true;
                            lucent::info("world", "talking to '{}' ({:.0f} units away)",
                                         best->handle, std::sqrt(best_d));
                        }
                        else
                            lucent::debug("world", "'{}' has no conversation: {}",
                                          best->handle, sc.last_error());
                    }
                    }
                }
                bool attacking = attack_left > 0.f;
                if (combat_demo && !attacking && frames > 30) {
                    attack_left = kAttackFrames;   // swing continuously, for testing
                    attacking = true;
                }
                const mcf::Actor* auto_attack_target = nullptr;
                // Headless driver target selection, after its requested
                // --walk-to phase (if any) has completed. Actor-origin distance
                // is not a valid attack reach because the narrow phase uses
                // animated bone centres; select the nearest hostile here and
                // leave actual contact entirely to the unchanged hit volumes.
                float auto_attack_dist2 = 1e30f;
                if (auto_attack_armed && player_input_enabled) {
                    for (const auto& a : world.actors()) {
                        if (!a.alive || a.defeated ||
                            mcf::CharType(a) != mcf::Actor::kEnemy) continue;
                        float dx = a.pos[0] + room_org[0] - px;
                        float dy = a.pos[1] + room_org[1] - py;
                        float dz = a.pos[2] + room_org[2] - pz;
                        // An enemy directly below the player is not in melee
                        // range. X/Z-only selection made the driver swing
                        // forever across stacked floors while the unchanged
                        // narrow phase correctly rejected every volume pair.
                        float d2 = dx * dx + dy * dy + dz * dz;
                        if (d2 <= auto_attack_dist2) {
                            auto_attack_dist2 = d2;
                            auto_attack_target = &a;
                        }
                    }
                }
                if (auto_attack_target) {
                    if (auto_attack_target_logged != auto_attack_target->handle) {
                        auto_attack_target_logged = auto_attack_target->handle;
                        lucent::info("combat", "auto attack acquired {} at "
                                     "origin distance {:.1f}",
                                     auto_attack_target->handle,
                                     std::sqrt(auto_attack_dist2));
                    }
                    float dx = auto_attack_target->pos[0] + room_org[0] - px;
                    float dz = auto_attack_target->pos[2] + room_org[2] - pz;
                    pdeg = std::atan2(dx, dz);
                    // After the authored positioning phase, close on the
                    // target until the 45+15 player/enemy volume reach can
                    // actually be exercised. Movement still goes through the
                    // room's floor and wall collision below.
                    if (!attacking && auto_attack_dist2 > 50.f * 50.f) {
                        mx = dx;
                        mz = dz;
                    }
                }
                if (auto_attack_target && auto_attack_dist2 <= 50.f * 50.f &&
                    !attacking && !level_up_open) {
                    attack_left = kAttackFrames;
                    attacking = true;
                }
                if (player_input_enabled && confirm && !sc.message_pending &&
                    !level_up_open && !attacking) {
                    attack_left = kAttackFrames;
                    attacking = true;
                }
                    bool have_blocked_event_probe = false;
                    float blocked_event_probe_x = px;
                    float blocked_event_probe_z = pz;
                    bool moving = (mx != 0 || my != 0 || mz != 0) &&
                                  !attacking;   // no moving mid-swing
                    if (moving) {
                        float ox = px, oz = pz;
                    float len = std::sqrt(mx * mx + my * my + mz * mz);
                    const float move_step = std::min(kWalk * dt,
                                                     headless_move_limit);
                    px += mx / len * move_step;
                    py += my / len * move_step;
                    pz += mz / len * move_step;
                    pdeg = std::atan2(mx, mz);
                    float g;
                    bool touching_mapjump_floor_owner = false;
                    if (mapjump_floor_owner) {
                        const auto& bx = *mapjump_floor_owner;
                        const float lx = px - room_org[0];
                        const float lz = pz - room_org[2];
                        touching_mapjump_floor_owner =
                            lx > bx.lo[0] - kEventBoxProbeHeight &&
                            lx < bx.hi[0] + kEventBoxProbeHeight &&
                            lz > bx.lo[2] - kEventBoxProbeHeight &&
                            lz < bx.hi[2] + kEventBoxProbeHeight;
                    }
                    PlacedObj* hit_object = nullptr;
                    bool move_has_floor = !have_col || col.GetFloorBelow(
                        px, pz, py + 30.f, mcf::Collision::kFloorMask, &g);
                    const bool point_stair_step = have_col && move_has_floor &&
                        ((g > py + .01f && g - py <= 30.f) ||
                         (headless_lower_event_goal && py > g + .01f &&
                          py - g <= 30.f));
                    float body_floor = 0.f;
                    const float body_probe_x = px + mx / len *
                                                    kEventBoxProbeHeight;
                    const float body_probe_z = pz + mz / len *
                                                    kEventBoxProbeHeight;
                    const bool body_stair_step = have_col &&
                        col.GetFloorBelow(body_probe_x, body_probe_z, py + 30.f,
                                          mcf::Collision::kFloorMask,
                                          &body_floor) &&
                        body_floor > py + .01f && body_floor - py <= 30.f &&
                        !col.BlockedXZ(ox, oz, body_probe_x, body_probe_z,
                                       body_floor, 30.f,
                                       mcf::Collision::kWallMask);
                    const bool move_stair_step = point_stair_step ||
                                                 body_stair_step;
                    if (body_stair_step) {
                        g = body_floor;
                        move_has_floor = true;
                    }
                    const bool static_blocked = have_col && !player_on_wall &&
                        (!move_has_floor ||
                         (!move_stair_step && col.BlockedXZ(
                             ox, oz, px, pz, py, 30.f,
                             mcf::Collision::kWallMask)));
                    bool object_blocked = objectBlockedXZ(
                        ox, oz, px, pz, py, &hit_object);
                    if (object_blocked && hit_object && opening_story &&
                        (hit_object->flags & 0x08) && inventory.Consume(17)) {
                        hit_object->alive = false;
                        object_blocked = false;
                        lucent::info("inventory", "used Mattock weapon kind 6 "
                                     "on breakable object id {} at "
                                     "({:.1f},{:.1f}); {} use(s) remain",
                                     hit_object->id,
                                     hit_object->pos[0] - room_org[0],
                                     hit_object->pos[2] - room_org[2],
                                     inventory.Uses(17));
                        if (hit_object->script_id != 0) {
                            const std::string callback = std::format(
                                "_BREAKOBJ_{}", hit_object->script_id);
                            if (sc.HasFunction(callback)) {
                                if (!sc.StartCoroutine(callback))
                                    lucent::warn("lua", "{}: {}", callback,
                                                 sc.last_error());
                                else
                                    lucent::info("world", "started break "
                                                 "callback {}", callback);
                            }
                        }
                    }
                    bool blocked = static_blocked || object_blocked;
                    float lx = px - room_org[0], lz = pz - room_org[2];
                    // SetDoor creates a centred map object on the requested
                    // room side. FREE doors open on body contact (the shipping
                    // eDoor comment says exactly that); the static .scol still
                    // contains the boundary wall. PORT CHOICE: the object's
                    // precise collision volume is not reversed, so contact is
                    // the blocked outward step within one fundamental chip of
                    // its centre; docs/re-frontier.md records the debt.
                    constexpr float kDoorHalfWidth = 30.f;
                    const bool horizontal_exit = std::fabs(mx) > std::fabs(mz);
                    bool outward[4]{!horizontal_exit && mz < 0.f,
                                     horizontal_exit && mx > 0.f,
                                     !horizontal_exit && mz > 0.f,
                                     horizontal_exit && mx < 0.f};
                    // Transition is character-volume contact, not origin
                    // contact. Multi-level ledges stop the origin several
                    // units inside their visual shell; requiring the point
                    // itself to enter the one-chip band can therefore make a
                    // real open edge impossible to cross.
                    const float side_contact =
                        kDoorHalfWidth + kCharacterCollisionRadius;
                    bool at_side[4]{lz <= side_contact,
                                    lx >= room_size.w - side_contact,
                                    lz >= room_size.h - side_contact,
                                    lx <= side_contact};
                    bool at_door[4]{
                        std::fabs(lx - room_size.w * .5f) <= kDoorHalfWidth,
                        std::fabs(lz - room_size.h * .5f) <= kDoorHalfWidth,
                        std::fabs(lx - room_size.w * .5f) <= kDoorHalfWidth,
                        std::fabs(lz - room_size.h * .5f) <= kDoorHalfWidth};
                    int exit_arrow = -1;
                    if (blocked)
                        for (int side = 0; side < 4; ++side) {
                            int door = world.DoorType(side);
                            // A clear room mark is the ordinary cell edge.
                            // _SetDoor ORs the side's mark bit into the room;
                            // OpenDoor clears it in this cell and its neighbor.
                            // Therefore kNoDoor is traversable across the full
                            // edge, FREE opens on centred body contact, and the
                            // other authored types remain closed.
                            bool passable = door == mcf::World::kNoDoor ||
                                            (door == 0 && at_door[side]);
                            if (outward[side] && at_side[side] && passable) {
                                exit_arrow = side;
                                break;
                            }
                    }
                    if (exit_arrow >= 0) {
                        requestRoomExit(exit_arrow, lx, lz);
                    }
                    if (player_on_wall) {
                        // Wall-plane movement is not resolved against the X/Z
                        // boundary mesh. Once its authored path crosses a room
                        // edge, hand that crossing to the same room-transition
                        // owner used by ordinary blocked movement.
                        if (lz < 0.f) requestRoomExit(0, lx, lz);
                        else if (lx > room_size.w)
                            requestRoomExit(1, lx, lz);
                        else if (lz > room_size.h)
                            requestRoomExit(2, lx, lz);
                        else if (lx < 0.f)
                            requestRoomExit(3, lx, lz);
                    }
                    bool traversed_event_wall = false;
                    bool touching_event_wall = false;
                    for (const auto& bx : world.boxes)
                        if ((bx.flags & (mcf::EventBox::kWallUp |
                                         mcf::EventBox::kWallDown)) &&
                            px > room_org[0] + bx.lo[0] - kEventBoxProbeHeight &&
                            px < room_org[0] + bx.hi[0] + kEventBoxProbeHeight &&
                            pz > room_org[2] + bx.lo[2] - kEventBoxProbeHeight &&
                            pz < room_org[2] + bx.hi[2] + kEventBoxProbeHeight) {
                            touching_event_wall = true;
                            break;
                        }
                    if (!touching_event_wall) event_wall_inside = false;
                    float wall_probe_x = px, wall_probe_z = pz;
                    if (blocked) {
                        // AppCharacterBase resolves event collision with the
                        // character volume, while the port's wall query stops
                        // its origin at the shell. Probe one established
                        // half-height/radius (15 units), not another point
                        // step, so body contact reaches the authored volume.
                        wall_probe_x += mx / len * kEventBoxProbeHeight;
                        wall_probe_z += mz / len * kEventBoxProbeHeight;
                    }
                    auto wall = event_wall_inside ? mcf::EventWallLink{} :
                        world.FindEventWall(ox, oz, wall_probe_x, wall_probe_z,
                                            py, room_org[0], room_org[2]);
                    const mcf::EventBox* lone_wall_up = nullptr;
                    if (!wall.source && !player_on_wall) {
                        for (const auto& bx : world.boxes) {
                            if (!bx.enabled || bx.no_touch ||
                                !(bx.flags & mcf::EventBox::kWallUp))
                                continue;
                            const float lx = px - room_org[0];
                            const float lz = pz - room_org[2];
                            const float dx = std::max(
                                {bx.lo[0] - lx, 0.f, lx - bx.hi[0]});
                            const float dz = std::max(
                                {bx.lo[2] - lz, 0.f, lz - bx.hi[2]});
                            const bool vertical_overlap =
                                py < bx.hi[1] &&
                                py + 2.f * kEventBoxProbeHeight > bx.lo[1];
                            if (vertical_overlap &&
                                dx * dx + dz * dz <
                                    kCharacterCollisionRadius *
                                    kCharacterCollisionRadius) {
                                lone_wall_up = &bx;
                                break;
                            }
                        }
                    }
                    if (lone_wall_up) {
                        if (auto* player = world.Find("MainPlayer"))
                            player->data[mcf::chr_data::kFloorType] = 1.f;
                        driver_route.clear();
                        driver_route_room.clear();
                        lucent::info("world", "entered lone WALL_UP volume at "
                                     "floor {:.1f}; switched MainPlayer to "
                                     "wall-plane movement", py);
                    }
                    if (touching_event_wall && !event_wall_inside && !wall.source &&
                        !lone_wall_up &&
                        !reported_unlinked_event_wall_contact) {
                        reported_unlinked_event_wall_contact = true;
                        lucent::warn("world", "event-wall contact in {} had no "
                                     "link: ({:.1f},{:.1f})->({:.1f},{:.1f}) "
                                     "at floor {:.1f}; scanned {} authored boxes",
                                     room_name, ox - room_org[0], oz - room_org[2],
                                     wall_probe_x - room_org[0],
                                     wall_probe_z - room_org[2], py,
                                     world.boxes.size());
                    }
                    if (wall.source) {
                        const float dx = wall_probe_x - ox;
                        const float dz = wall_probe_z - oz;
                        if (std::fabs(dx) > std::fabs(dz)) {
                            // Finish on the far edge of the paired destination
                            // volume. Stopping at the source edge leaves the
                            // origin inside the vine strip; on the following
                            // frame the ordinary floor query then snaps it
                            // straight back to the source level.
                            const float edge = dx > 0.f ? wall.destination->hi[0]
                                                        : wall.destination->lo[0];
                            px = room_org[0] + std::nextafter(
                                edge, dx > 0.f ? std::numeric_limits<float>::infinity()
                                               : -std::numeric_limits<float>::infinity());
                            pz = std::clamp(pz,
                                           room_org[2] + wall.destination->lo[2],
                                           room_org[2] + wall.destination->hi[2]);
                        } else {
                            const float edge = dz > 0.f ? wall.destination->hi[2]
                                                        : wall.destination->lo[2];
                            pz = room_org[2] + std::nextafter(
                                edge, dz > 0.f ? std::numeric_limits<float>::infinity()
                                               : -std::numeric_limits<float>::infinity());
                            px = std::clamp(px,
                                           room_org[0] + wall.destination->lo[0],
                                           room_org[0] + wall.destination->hi[0]);
                        }
                        // sk1.lua authors WallUp at floor-1 and WallDn at
                        // floor-14. Invert those exact constructors: the vine
                        // strip itself has no horizontal collision floor to
                        // query, which is why these event volumes exist.
                        const bool upward =
                            (wall.source->flags & mcf::EventBox::kWallUp) != 0;
                        py = wall.destination->lo[1] + (upward ? 14.f : 1.f);
                        float clear_dir_x = dx;
                        float clear_dir_z = dz;
                        // An upward wall at a room boundary hands the player to
                        // the elevated neighbouring cell. Its paired downward
                        // wall returns to the lower shell in THIS cell; carrying
                        // the outward approach direction through the overlapping
                        // destination volume would reload the upper cell and
                        // bounce forever. Keep downward landings strictly inside
                        // the current room, as the destination event volume is.
                        if (!upward) {
                            px = std::clamp(px,
                                room_org[0] + kEventBoxProbeHeight,
                                room_org[0] + room_size.w -
                                    kEventBoxProbeHeight);
                            pz = std::clamp(pz,
                                room_org[2] + kEventBoxProbeHeight,
                                room_org[2] + room_size.h -
                                    kEventBoxProbeHeight);
                            if (wall.destination->lo[0] <= 0.f) {
                                clear_dir_x = 1.f; clear_dir_z = 0.f;
                            } else if (wall.destination->hi[0] >= room_size.w) {
                                clear_dir_x = -1.f; clear_dir_z = 0.f;
                            } else if (wall.destination->lo[2] <= 0.f) {
                                clear_dir_x = 0.f; clear_dir_z = 1.f;
                            } else if (wall.destination->hi[2] >= room_size.h) {
                                clear_dir_x = 0.f; clear_dir_z = -1.f;
                            }
                        }
                        traversed_event_wall = true;
                        event_wall_inside = true;
                        // The remaining path was planned on the old floor.
                        // Rebuild it from the destination landing next frame.
                        driver_route.clear();
                        driver_route_room.clear();
                        const float traverse_len = std::sqrt(
                            clear_dir_x * clear_dir_x +
                            clear_dir_z * clear_dir_z);
                        const float clear_x = px + clear_dir_x / traverse_len *
                                                   kEventBoxProbeHeight;
                        const float clear_z = pz + clear_dir_z / traverse_len *
                                                   kEventBoxProbeHeight;
                        float clear_floor = 0.f;
                        const bool clear_has_floor = !have_col ||
                            col.GetFloorBelow(clear_x, clear_z, py + 30.f,
                                              mcf::Collision::kFloorMask,
                                              &clear_floor);
                        const bool clear_blocked = have_col &&
                            col.BlockedXZ(px, pz, clear_x, clear_z, py, 30.f,
                                          mcf::Collision::kWallMask);
                        const bool applied_clear = clear_has_floor &&
                                                   !clear_blocked;
                        if (applied_clear) {
                            px = clear_x;
                            pz = clear_z;
                            py = clear_floor;
                        }
                        lucent::info("world", "traversed event wall {} -> {}, "
                                     "floor {:.1f}; one-radius clear blocked={}, "
                                     "floor={} ({:.1f}), applied={}", wall.source->flags,
                                     wall.destination->flags, py, clear_blocked,
                                     clear_has_floor, clear_floor, applied_clear);
                        const float local_x = px - room_org[0];
                        const float local_z = pz - room_org[2];
                        if (local_z < 0.f) requestRoomExit(0, local_x, local_z);
                        else if (local_x > room_size.w)
                            requestRoomExit(1, local_x, local_z);
                        else if (local_z > room_size.h)
                            requestRoomExit(2, local_x, local_z);
                        else if (local_x < 0.f)
                            requestRoomExit(3, local_x, local_z);
                    }
                    // Refuse to walk off the collision mesh rather than
                    // silently floating: revert the step if there is no floor.
                    if (!blocked || traversed_event_wall)
                        driver_blocked_frames = 0;
                    if (blocked && !traversed_event_wall) {
                        const float attempted_x = px;
                        const float attempted_z = pz;
                        have_blocked_event_probe = true;
                        blocked_event_probe_x = attempted_x;
                        blocked_event_probe_z = attempted_z;
                        px = ox;
                        pz = oz;
                        if (opening_story && !driver_route.empty() &&
                            ++driver_blocked_frames == 120) {
                            lucent::error(
                                "host", "opening route stalled for {} frames "
                                "at ({:.4f},{:.1f},{:.4f}), attempted "
                                "({:.4f},{:.4f}), toward waypoint "
                                "({:.1f},{:.1f}) floor {:.1f}; static-blocked={}, "
                                "floor-found={}, proposed-floor={:.1f}, "
                                "stair-step={}, object-blocked={}, object-id={}",
                                driver_blocked_frames,
                                ox - room_org[0], py - room_org[1],
                                oz - room_org[2],
                                attempted_x - room_org[0],
                                attempted_z - room_org[2],
                                driver_route.front().x,
                                driver_route.front().z,
                                driver_route.front().y - room_org[1],
                                static_blocked, move_has_floor,
                                move_has_floor ? g - room_org[1] : 0.f,
                                move_stair_step, object_blocked,
                                hit_object ? hit_object->id : -1);
                            run_failed = true;
                            running = false;
                        }
                    }
                    else if (!blocked && !traversed_event_wall && have_col &&
                             !player_on_wall &&
                             !event_wall_inside && !touching_event_wall) {
                        // The shipping character volume still overlaps an
                        // authored MapJump arrival box when its centre lies a
                        // few units beyond a stacked ledge. Keep that box's
                        // measured collision floor until the body clears it;
                        // once the centre reaches ordinary upper terrain, the
                        // unchanged point query takes ownership again.
                        if (mapjump_floor_owner &&
                            std::fabs(g - mapjump_floor_owner_y) >= 5.f) {
                            // Do not drop stacked-floor ownership merely
                            // because the centre has cleared the arrival box:
                            // the body can still bridge the short seam to the
                            // next collision polygon. Transfer ownership only
                            // when the point query sees the same level.
                            py = mapjump_floor_owner_y;
                        } else {
                            py = g;
                            if (mapjump_floor_owner &&
                                !touching_mapjump_floor_owner) {
                                lucent::info("world", "mapjump arrival floor "
                                             "transferred to point ground at "
                                             "{:.1f}", g);
                                mapjump_floor_owner.reset();
                            }
                        }
                    }
                }
                // The opening boss's own script gates its charge on EX_1, and
                // the requested point lies beyond the physical wall. Reaching
                // the authored boundary CELL is therefore the real end of the
                // headless positioning phase, not reaching the literal point.
                if (player_input_enabled && auto_attack && walk_to && have_ground &&
                    (ground.Get(px - room_org[0], pz - room_org[2]) &
                     0x02000000u) != 0)
                    auto_attack_armed = true;
                if (auto_attack_armed && !auto_attack_armed_logged) {
                    auto_attack_armed_logged = true;
                    lucent::info("combat", "auto attack armed at room-local "
                                 "({:.1f},{:.1f})", px - room_org[0],
                                 pz - room_org[2]);
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
                if (cam.has_target_pos) {
                    look[0] = cam.target_pos[0] + room_org[0];
                    look[1] = cam.target_pos[1] + room_org[1];
                    look[2] = cam.target_pos[2] + room_org[2];
                }
                for (int k = 0; k < 3; ++k) look[k] += cam.target_sub[k];
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
                if (cam.has_eye_pos) {
                    want[0] = cam.eye_pos[0] + room_org[0];
                    want[1] = cam.eye_pos[1] + room_org[1];
                    want[2] = cam.eye_pos[2] + room_org[2];
                }
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
                    pl->SetMotion(attacking ? kMotionAttack :
                                  (moving ? kMotionWalk : kMotionWait));
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
                    if (!mcf::UsesHostEnemyAI(a)) continue;
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
                    if (d < reach * 0.7f) { a.SetMotion(kMotionWait); continue; }
                    // The enemy's OWN speed, from enemydat +0x68 -- 12 and 24
                    // units/s dominate, against the invented 30 this replaces.
                    // Nine of the 107 enemies have 0 and simply do not move.
                    if (a.move_speed <= 0.f) { a.SetMotion(kMotionWait); continue; }

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
                                    a.SetMotion(kMotionWalk);
                                }
                            }
                            continue;
                        }
                        if (a.ai_state != 2) { a.SetMotion(kMotionWait); continue; }
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
                    a.SetMotion(kMotionWalk);
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
                sc.game_time_ms = int(t * (1000.f / 30.f));
                if (!fade_test) world.fade.Tick(dt * 1000.f);
                sc.ResumeCoroutines();
                // AddEnemyZaco is commonly called by Init, after loadRoom's
                // placement pass. Resolve every newly created random actor in
                // the same frame; otherwise its literal zero coordinates make
                // the command look implemented while spawning off the map.
                placeRandomActors();
                // Init and event-box coroutines create actors after the room's
                // first seed.  Seed only those new actors; a second complete
                // seed would restore HP and erase combat that has happened.
                seedCombat();
                serviceAudio();
                if (!no_audio) audio.Update();

                // Event boxes are edge-triggered: entering fires the handler
                // once. Firing every frame would re-enter the same transition
                // forever.
                // Collision and rendering keep `py` at the player's foot on
                // the floor. AppCharacterBase, however, tests event boxes
                // with the character position, whose centre is 15 units
                // above that contact point. Without this distinction every
                // floor-anchored box rejects the player at its strict lower-Y
                // boundary.
                for (auto& bx : world.boxes) {
                    bool in = bx.IsHit(px, py + kEventBoxProbeHeight, pz,
                                      room_org[0], room_org[2]) ||
                              (have_blocked_event_probe &&
                               EventBoxCharacterContact(
                                  bx,
                                  blocked_event_probe_x,
                                  py + kEventBoxProbeHeight,
                                  blocked_event_probe_z,
                                  room_org[0], room_org[2],
                                  kCharacterCollisionRadius));
                    const bool continuing_mapjump_overlap =
                        in && mapjump_floor_owner &&
                        bx.name == mapjump_floor_owner->name &&
                        bx.flags == mapjump_floor_owner->flags &&
                        std::equal(std::begin(bx.lo), std::end(bx.lo),
                                   std::begin(mapjump_floor_owner->lo)) &&
                        std::equal(std::begin(bx.hi), std::end(bx.hi),
                                   std::begin(mapjump_floor_owner->hi));
                    if (in && !bx.inside) {
                        if (continuing_mapjump_overlap) {
                            lucent::info("world", "continued through mapjump "
                                         "arrival box '{}' without reversing",
                                         bx.name);
                        } else if (!bx.name.empty()) {
                            lucent::info("world", "entered event box '{}'", bx.name);
                            // EvBoxSwitch expands to flags 0x1c. Its callback
                            // reads the engine-supplied switch_result payload;
                            // a generic zero return makes every authored floor
                            // switch silently take its release branch.
                            if (bx.flags == 0x1c)
                                sc.SetGlobalNumber("switch_result", 1.0);
                            if (!sc.StartCoroutine(bx.name))
                                lucent::warn("lua", "{}: {}", bx.name,
                                             sc.last_error());
                            if (const auto* player = world.Find("MainPlayer")) {
                                const float script_x =
                                    player->pos[0] + room_org[0];
                                const float script_y =
                                    player->pos[1] + room_org[1];
                                const float script_z =
                                    player->pos[2] + room_org[2];
                                if (script_x != px || script_y != py ||
                                    script_z != pz) {
                                    px = script_x;
                                    py = script_y;
                                    pz = script_z;
                                    driver_route.clear();
                                    driver_route_room.clear();
                                    lucent::info("world", "event callback '{}' "
                                                 "moved MainPlayer to room-local "
                                                 "({:.1f},{:.1f},{:.1f})",
                                                 bx.name, player->pos[0],
                                                 player->pos[1], player->pos[2]);
                                }
                            }
                        }
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
                        if (!acts[aidx].alive || acts[aidx].defeated ||
                            acts[aidx].attack.empty()) continue;
                        auto an = mcf::ActorModelName(acts[aidx].kind, acts[aidx].type_id);
                        auto ait = cache.find(an);
                        if (ait == cache.end()) { ++cs.atk_no_model; continue; }
                        for (const auto& [ai, av] : acts[aidx].attack) {
                            if (!av.valid || av.bone.empty()) continue;
                            ++cs.swing_frames;
                            float ap[3]{0.f, 0.f, 0.f};
                            if (!mcf::BoneLocalPos(ait->second.model, nullptr, t,
                                                   av.bone, ap)) {
                                ++cs.atk_no_bone;
                                // SiModelBase::GetBoneIDByName @ 0x35b414
                                // returns ID 0 after an exact-strcmp miss.
                                if (!ait->second.model.bones.empty())
                                    mcf::BoneLocalPos(ait->second.model, nullptr, t,
                                        ait->second.model.bones.front().name, ap);
                            }
                            for (int k = 0; k < 3; ++k)
                                ap[k] += acts[aidx].pos[k] + room_org[k] + av.offset[k];

                            for (size_t didx = 0; didx < acts.size(); ++didx) {
                                if (didx == aidx || !acts[didx].alive ||
                                    acts[didx].defeated ||
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
                                    if (acts[aidx].handle == "MainPlayer") {
                                        ++cs.pairs_from_player;
                                        cs.closest_from_player =
                                            std::min(cs.closest_from_player, sep);
                                    }
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
                                    if (acts[aidx].handle == "MainPlayer")
                                        ++cs.hits_from_player;

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
                                    // One hit per enabled attack volume phase,
                                    // for every defender type. Scripted attacks
                                    // advance swing_id on false->true; the host
                                    // player attack does the same on key-down.
                                    auto key = std::make_pair(atkA.swing_id,
                                                              acts[didx].handle);
                                    if (std::find(atkA.hit_this_swing.begin(),
                                                  atkA.hit_this_swing.end(), key) !=
                                        atkA.hit_this_swing.end())
                                        continue;
                                    atkA.hit_this_swing.push_back(key);
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
                                        d.defeated = true;
                                        // Script-owned bosses must remain as
                                        // characters through _BOSSDEAD; it
                                        // calls DeadEnemy only after the death
                                        // motion and effects. Ordinary enemy
                                        // death presentation is native code
                                        // not yet ported, so retain the prior
                                        // immediate removal for that class.
                                        if (d.kind != 'B') d.alive = false;
                                        d.script_auto_move = false;
                                        d.look_target.clear();
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

                if (world.ConsumeEnemyWaveCleared()) {
                    if (sc.HasFunction("EnemyDead")) {
                        if (sc.StartCoroutine("EnemyDead"))
                            lucent::info("world", "all live enemies defeated; "
                                         "started EnemyDead coroutine");
                        else
                            lucent::error("lua", "EnemyDead coroutine: {}",
                                          sc.last_error());
                    } else {
                        lucent::info("world", "all live enemies defeated; room "
                                     "has no EnemyDead handler");
                    }
                }

                if (room_exit.pending) {
                    room_exit.pending = false;
                    const float source_floor_y = py;
                    lucent::info("world", "room exit {} -> {} at world "
                                 "({:.1f},{:.1f})", room_exit.arrow,
                                 room_exit.dest, room_exit.world_x,
                                 room_exit.world_z);
                    if (loadRoom(room_exit.dest)) {
                        seedCombat();
                        px = room_exit.world_x;
                        pz = room_exit.world_z;
                        // The 330x270 room format wraps the ordinary 300x240
                        // playable area in a 15-unit margin on every side.
                        // Cross that margin plus one character radius; using
                        // radius alone strands the centre in the padding strip
                        // (M0013_01_00 measures reachable x=307.5..322.5).
                        constexpr float kRoomEdgeBodyRadius = 30.f;
                        const float inset_x = room_size.w > 300.f
                            ? 15.f + kRoomEdgeBodyRadius
                            : kEventBoxProbeHeight;
                        const float inset_z = room_size.h > 240.f
                            ? 15.f + kRoomEdgeBodyRadius
                            : kEventBoxProbeHeight;
                        if (room_exit.arrow == 0) pz -= inset_z;
                        if (room_exit.arrow == 1) px += inset_x;
                        if (room_exit.arrow == 2) pz += inset_z;
                        if (room_exit.arrow == 3) px -= inset_x;
                        py = source_floor_y;
                        bool source_level_event_wall = false;
                        if (have_col) {
                            float g;
                            for (const auto& bx : world.boxes)
                                if (bx.enabled && !bx.no_touch &&
                                    (bx.flags & (mcf::EventBox::kWallUp |
                                                 mcf::EventBox::kWallDown)) &&
                                    px > room_org[0] + bx.lo[0] -
                                             kEventBoxProbeHeight &&
                                    px < room_org[0] + bx.hi[0] +
                                             kEventBoxProbeHeight &&
                                    pz > room_org[2] + bx.lo[2] -
                                             kEventBoxProbeHeight &&
                                    pz < room_org[2] + bx.hi[2] +
                                             kEventBoxProbeHeight &&
                                    source_floor_y >= bx.lo[1] &&
                                    source_floor_y <= bx.hi[1]) {
                                    source_level_event_wall = true;
                                    break;
                                }
                            // Preserve a stacked ledge across a shared room
                            // edge. This is the engine's GetFloor contract:
                            // highest floor at or below the query Y. When the
                            // one-radius inset overlaps an authored wall at the
                            // source level, that wall is the continuation; a
                            // lower collision shell must not steal ownership
                            // before character-volume contact can traverse it.
                            if (!source_level_event_wall && (col.GetFloorBelow(
                                    px, pz, source_floor_y + 30.f,
                                    mcf::Collision::kFloorMask, &g) ||
                                col.GetFloor(px, pz,
                                             mcf::Collision::kFloorMask, &g)))
                                py = g;
                        }
                        lucent::info("world", "room-entry floor source {:.1f} -> "
                                     "resolved {:.1f}; destination event-wall "
                                     "ownership={}", source_floor_y, py,
                                     source_level_event_wall);
                        world.Spawn("MainPlayer", 0, px - room_org[0],
                                    py - room_org[1], pz - room_org[2]).kind = 'C';
                        restoreParty();
                        event_wall_inside = false;
                        reported_unlinked_event_wall_contact = false;
                        startRoomInit();
                        cam_init = false;
                        serviceAudio();
                        if (!stop_room.empty() && room_name == stop_room) {
                            lucent::info("world", "reached requested stop room {}",
                                         stop_room);
                            running = false;
                        }
                    } else {
                        lucent::error("world", "door exit to {} failed; staying put",
                                      room_exit.dest);
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
                            float point_floor;
                            const bool point_hit = col.GetFloor(
                                px, pz, mcf::Collision::kFloorMask,
                                &point_floor);
                            if (point_hit) py = point_floor;

                            // MapJump authors its destination immediately
                            // outside the matching arrival box. The shipping
                            // character is grounded as a volume, so its body
                            // can still own a ledge which ends just before the
                            // centre point (M0011_00_02 -> M0000_09_06 is the
                            // discriminator). A point-only ray instead falls
                            // through to an unrelated stacked floor. Re-sample
                            // every event box touched by the established body
                            // radius and take the nearest box's own collision
                            // floor; no room name or authored height enters the
                            // decision.
                            int touchable_boxes = 0;
                            int nearby_boxes = 0;
                            int floor_candidates = 0;
                            float best_box_d2 =
                                std::numeric_limits<float>::infinity();
                            float best_box_floor = py;
                            mcf::EventBox* best_box = nullptr;
                            for (auto& bx : world.boxes) {
                                if (!bx.enabled || bx.no_touch) continue;
                                ++touchable_boxes;
                                const float lx = px - room_org[0];
                                const float lz = pz - room_org[2];
                                const float dx = lx < bx.lo[0]
                                    ? bx.lo[0] - lx
                                    : (lx > bx.hi[0] ? lx - bx.hi[0] : 0.f);
                                const float dz = lz < bx.lo[2]
                                    ? bx.lo[2] - lz
                                    : (lz > bx.hi[2] ? lz - bx.hi[2] : 0.f);
                                const float d2 = dx * dx + dz * dz;
                                // Character/event volumes are strict: exact
                                // tangency is not overlap. Accepting equality
                                // can assign a MapJump arrival the floor of an
                                // adjacent box that the body never entered.
                                if (d2 >= kEventBoxProbeHeight *
                                              kEventBoxProbeHeight)
                                    continue;
                                ++nearby_boxes;
                                const float cx = room_org[0] +
                                    (bx.lo[0] + bx.hi[0]) * .5f;
                                const float cz = room_org[2] +
                                    (bx.lo[2] + bx.hi[2]) * .5f;
                                float box_floor;
                                if (!col.GetFloorBelow(
                                        cx, cz,
                                        room_org[1] + bx.hi[1] +
                                            kEventBoxProbeHeight,
                                        mcf::Collision::kFloorMask,
                                        &box_floor) ||
                                    box_floor < room_org[1] + bx.lo[1] -
                                                    kEventBoxProbeHeight)
                                    continue;
                                ++floor_candidates;
                                if (d2 < best_box_d2) {
                                    best_box_d2 = d2;
                                    best_box_floor = box_floor;
                                    best_box = &bx;
                                }
                            }
                            if (floor_candidates) {
                                py = best_box_floor;
                                mapjump_floor_owner = *best_box;
                                mapjump_floor_owner_y = best_box_floor;
                                // The character volume already overlaps this
                                // arrival box even though the authored centre
                                // point lies just outside it. Seed the normal
                                // edge-trigger latch: the initial move onto
                                // the box-owned ledge is continuation of that
                                // overlap, not a fresh entry. The event loop
                                // clears it after the body leaves the volume;
                                // the headless planner separately excludes a
                                // later re-entry while pursuing another goal.
                                best_box->inside = true;
                            }
                            lucent::info(
                                "world", "mapjump grounding at local "
                                "({:.1f},{:.1f}): point floor={} ({:.1f}); "
                                "scanned {} touchable boxes, {} within body "
                                "radius, {} owned a collision floor; chose {:.1f}",
                                j.x, j.z, point_hit, point_hit ? point_floor : 0.f,
                                touchable_boxes, nearby_boxes, floor_candidates,
                                py);
                        }
                        world.Spawn("MainPlayer", 0, px - room_org[0],
                                    py - room_org[1], pz - room_org[2]).kind = 'C';
                        restoreParty();
                        event_wall_inside = false;
                        reported_unlinked_event_wall_contact = false;
                        startRoomInit();
                        // eArrow: UP=0 RI=1 DN=2 LF=3.
                        pdeg = float(j.arrow) * (float(std::numbers::pi) / 2.f);
                        cam_init = false;
                        serviceAudio();
                        if (!stop_room.empty() && room_name == stop_room) {
                            lucent::info("world", "reached requested stop room {}",
                                         stop_room);
                            running = false;
                        }
                    } else {
                        lucent::error("world", "mapjump to {} failed; staying put", dest);
                    }
                }

                if (stop_sccnt >= 0 &&
                    int(sc.GlobalNumber("sccnt", -1)) == stop_sccnt &&
                    !sc.message_pending && sc.live_coroutines() == 0) {
                    lucent::info("world", "reached settled scenario state sccnt={}",
                                 stop_sccnt);
                    running = false;
                }
                if (stop_item >= 0 && inventory.Has(stop_item) &&
                    !sc.message_pending && sc.live_coroutines() == 0) {
                    lucent::info("inventory", "reached settled requested item {}",
                                 stop_item);
                    running = false;
                }

                // A windowless gameplay run has no pixels to consume. Keep
                // simulation, script clocks, collision and combat running, but
                // do not rebuild every skeletal palette merely to throw the
                // offscreen framebuffer away. Capture runs still render.
                ++frames;
                if (!no_window || !shot.empty()) {
                glViewport(0, 0, W, H);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                anim_t = t;
                drawOne(stage, origin_zero, nullptr);
                for (const auto& o : objects)
                    if (o.alive && sc.ObjectVisible(o.script_id))
                        drawOne(*o.r, o.pos, nullptr);
                // Draw from LIVE actor state: `placed` was a load-time snapshot,
                // so enemies that move would have rendered at their spawn point.
                for (auto& a : world.actors_mutable()) {
                    if (!a.alive || a.handle == "MainPlayer") continue;
                    auto nm = mcf::ActorModelName(a.kind, a.type_id);
                    if (nm.empty()) continue;   // eNPC.TRANS: invisible by design
                    auto it = cache.find(nm);
                    if (it == cache.end()) continue; // actorMotion reported why
                    const mcf::Motion* mo = actorMotion(a);
                    float wp[3]{a.pos[0] + room_org[0], a.pos[1] + room_org[1],
                                a.pos[2] + room_org[2]};
                    anim_t = a.motion_frame;
                    drawOne(it->second, wp, mo, a.rot_y);
                }
                if (have_hero) {
                    float hp[3]{px, py, pz};
                    drawOne(hero, hp, heroMotion(moving ? 1 : 0), pdeg);
                }
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
            }
            lucent::info("host", "{} frames; audio decoded {} sounds / {} frames, bgm={}",
                         frames, audio.stat.decoded_sounds, audio.stat.decoded_frames,
                         audio.bgm_id());
            lucent::info("world", "ended in {} at room-local ({:.1f},{:.1f},{:.1f})",
                         room_name, px - room_org[0], py - room_org[1],
                         pz - room_org[2]);
            if (!driver_route.empty())
                lucent::info("host", "opening route has {} waypoint(s) left; "
                             "next is ({:.1f},{:.1f})",
                             driver_route.size(), driver_route.front().x,
                             driver_route.front().z);
            lucent::info("lua", "end state: sccnt={:.0f}, eventScene={:.0f}, "
                         "cinema={}, player-control={}, {} live coroutine(s)",
                         sc.GlobalNumber("sccnt", -1),
                         sc.GlobalNumber("eventScene", -1), sc.cinema,
                         sc.player_control_enabled, sc.live_coroutines());
            if (const auto* pl = world.Find("MainPlayer"))
                lucent::info("world", "player motion {} at {:.1f}/{:.1f}, "
                             "script-moving={}", pl->motion, pl->motion_frame,
                             pl->motion_duration, pl->script_auto_move);
            if (sc.party_id > 0) {
                const char* handle = mcf::PartyHandle(sc.party_id);
                const auto* party = world.Find(handle);
                if (party && party->alive)
                    lucent::info("world", "party id {} is {} alive at room-local "
                                 "({:.1f},{:.1f},{:.1f})", sc.party_id, handle,
                                 party->pos[0], party->pos[1], party->pos[2]);
                else
                    lucent::error("world", "party id {} expects {}, but no live "
                                  "actor exists after scanning {} actor(s)",
                                  sc.party_id, handle, world.actors().size());
            } else {
                lucent::info("world", "party id 0: no companion requested");
            }
            lucent::info("combat",
                         "{} frame-overlaps ({} rejected by the faction filter) "
                         "-> {} landed hits -> {} kills; "
                         "{} volume pairs over {} live-swing frames; "
                         "closest approach {} units; skipped {} attackers / {} "
                         "defenders with no loaded model; {} attacker volumes "
                         "fell back to bone 0, {} defenders had no such bone",
                         cs.hits, cs.blocked_by_faction, cs.landed, cs.kills,
                         cs.pairs, cs.swing_frames,
                         cs.pairs ? std::format("{:.1f}", cs.closest) : "n/a",
                         cs.atk_no_model, cs.def_no_model, cs.atk_no_bone,
                         cs.def_no_bone);
            lucent::info("combat", "player attack tested {} volume pairs, "
                         "produced {} geometric overlaps; closest separation {}",
                         cs.pairs_from_player, cs.hits_from_player,
                         cs.pairs_from_player
                             ? std::format("{:.1f}", cs.closest_from_player)
                             : "n/a");
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
            return run_failed ? 1 : 0;
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
