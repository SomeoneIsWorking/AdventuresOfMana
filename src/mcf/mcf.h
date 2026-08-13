// Asset layer for the Adventures of Mana PC port.
//
// Formats are documented in docs/assets.md and docs/mpk-format.md. Each was
// reversed from the shipping engine's own loader and validated over the whole
// corpus; the Python reference implementations under tools/asset/ remain the
// cross-check for this code.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mcf {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
// LHA static-Huffman (-lh5-), 13-bit dictionary. See docs/mpk-format.md.
// ---------------------------------------------------------------------------
namespace lha {
constexpr int kDicBit = 13;

// Decodes exactly `out.size()` bytes. Throws on a malformed Huffman table.
// `consumed` receives the number of input bytes actually read, which is the
// only meaningful validity check -- the output buffer is caller-sized, so its
// length proves nothing.
void Decode(std::span<const uint8_t> in, std::span<uint8_t> out,
            size_t* consumed = nullptr, int dicbit = kDicBit);
}  // namespace lha

// ---------------------------------------------------------------------------
// MPK archive (magic "mcfa")
// ---------------------------------------------------------------------------
class Archive {
public:
    struct Entry {
        std::string name;
        uint32_t flags = 0;
        uint32_t offset = 0;   // relative to the payload base
        uint32_t csize = 0;
        uint32_t usize = 0;
    };

    explicit Archive(const std::string& path);

    // Decompresses one entry. Throws if the decoder does not consume exactly
    // `csize` bytes.
    std::vector<uint8_t> Read(const Entry& e) const;
    std::vector<uint8_t> Read(std::string_view name) const;

    bool Has(std::string_view name) const { return index_.count(std::string(name)) != 0; }

    // First entry whose name starts with `prefix`, or "" if none. Motion files
    // are `<model>_<NNN>_<LABEL>.smot` and the LABEL is not canonical, so
    // lookup is by the numeric prefix only. See docs/script-data-model.md.
    std::string FindByPrefix(std::string_view prefix) const;
    const std::vector<Entry>& entries() const { return entries_; }

private:
    std::string path_;
    uint32_t payload_ = 0;
    std::vector<Entry> entries_;
    std::unordered_map<std::string, size_t> index_;
};

// ---------------------------------------------------------------------------
// PNG. The boot path loads its art by name -- ModeMakerLogo's `sk1/sqex%s.png`,
// ModeTitle's title logos -- and those are plain PNGs in the archive rather than
// .stex. Every one the boot path touches is 8-bit RGBA, non-interlaced, so the
// decoder handles exactly that and refuses anything else loudly.
// ---------------------------------------------------------------------------
bool DecodePng(std::span<const uint8_t> file, int* w, int* h,
               std::vector<uint8_t>* rgba);

// ---------------------------------------------------------------------------
// .stex texture container (magic "SMDI")
// ---------------------------------------------------------------------------
struct Texture {
    std::string name;
    uint32_t width = 0, height = 0, mips = 0;
    uint32_t fmtcode = 0;
    std::span<const uint8_t> pixels;  // RGBA8888, mip 0 first; views `data`
};

struct TextureSet {
    std::vector<uint8_t> data;
    std::vector<Texture> textures;
};

TextureSet ParseStex(std::vector<uint8_t> file);

// A `.mtex` is one standalone texture: 128-byte header
// {u32 format, u32 width, u32 height, u32 max_mip_level, u32 data_size}
// followed by pixels at +0x80. Layout from AppMapTexture::SetBinary, which
// forwards them to MCFSiSurfaceTexture::Create.
TextureSet ParseMtex(std::vector<uint8_t> file, std::string name);

// A `.stexinfo` is a texture *name list* for a map: u32 count followed by
// 256-byte STEXINFOFILE_BODY records, each beginning with a NUL-terminated
// name. Map materials index into this list; each name resolves to
// `sk1/<name>.mtex`.
std::vector<std::string> ParseStexInfo(std::span<const uint8_t> file);

// ---------------------------------------------------------------------------
// .smdl model (magic "Smd3")
// ---------------------------------------------------------------------------
enum class VertexUsage : uint32_t {
    kPosition = 0,
    kWeight = 1,
    kIncidence = 2,
    kColor = 5,
    kTexcoord0 = 7,
};

enum class VertexType : uint32_t {
    kFloat2 = 1,
    kFloat3 = 2,
    kFloat4 = 3,
    kUByte4Color = 4,
    kUByte4Index = 5,
};

uint32_t VertexTypeSize(VertexType t);

struct VertexAttribute {
    VertexUsage usage;
    VertexType type;
    uint32_t offset;
};

// One material: a name, the original Maya source path, and an index into the
// companion .stex texture array.
struct Material {
    std::string name;
    std::string source_path;
    uint32_t texture_index = 0;
    // Word 9 (+0x24). NOT confirmed against the engine's GL calls -- the
    // material -> SiVertexStream hop that feeds _SetBlendingInfo is not
    // reversed. What IS measured, across all 6145 shipping materials: every
    // one of the 377 named "*shadow*" sets it, with ZERO exceptions, and of the
    // 425 others that set it, all are wave/sea/water surfaces. Both classes
    // were checked, not just the positive one. See docs/assets.md.
    bool blend = false;
};

// A contiguous run of the index buffer drawn with one material.
struct DrawRange {
    uint32_t material = 0;
    uint32_t index_count = 0;
    uint32_t byte_offset = 0;   // into the index buffer
};

// A 352-byte SiModelBone, used in place by the engine (CreateSkeleton passes the
// file pointer straight to SiModelSkeleton::_SetBinary). Field offsets come from
// SiModelBase::GetBoneName / GetBoneIDByName, which index at stride 0x160 and
// read the name at +0x14C.
struct Bone {
    std::string name;
    int32_t parent = -1;          // +0x144, -1 for root; always < own index
    float local[16]{};            // +0x000 parent-relative bind transform
    float inv_world[16]{};        // +0x080 inverse of the accumulated bind transform
    bool degenerate = false;      // zero-scale bone: inv_world holds inf/nan
};

struct Model {
    std::vector<uint8_t> data;
    uint32_t bone_count = 0;
    uint32_t vertex_count = 0, vertex_offset = 0, vertex_stride = 0;
    uint32_t index_count = 0, index_offset = 0, index_size = 0;
    std::vector<VertexAttribute> layout;
    std::vector<Material> materials;
    std::vector<DrawRange> draws;
    std::vector<Bone> bones;

    std::span<const uint8_t> vertices() const {
        return {data.data() + vertex_offset, size_t(vertex_count) * vertex_stride};
    }
    std::span<const uint8_t> indices() const {
        return {data.data() + index_offset, size_t(index_count) * index_size};
    }
    const VertexAttribute* Find(VertexUsage u) const;
};

Model ParseSmdl(std::vector<uint8_t> file);

// ---------------------------------------------------------------------------
// .scol collision mesh (magic "SCol")
// ---------------------------------------------------------------------------
// Layout reversed from SiCollisionMesh::GetFloor. Two levels: a coarse cell
// array, each cell carrying an XZ AABB and a list of triangle indices, over a
// flat array of 40-byte triangles that index a shared vec3 pool.
struct Collision {
    std::vector<uint8_t> data;
    uint32_t tri_count = 0, tri_off = 0;
    uint32_t cell_count = 0, cell_off = 0;
    uint32_t vec_count = 0, vec_off = 0;
    float aabb_lo[3]{}, aabb_hi[3]{};

    // Attribute-mask bits at triangle +0x24. Derived two ways that agree:
    // the engine's own GetFloor call sites pass 3 and 7 (bits 0..2 only), and a
    // census of 40 rooms shows bits 0/1/2 are near-horizontal (mean |normal.y|
    // 0.99, 1.00) while bits 3/4 are exactly vertical (0.00).
    static constexpr uint32_t kFloorMask = 0x7;    // what the engine queries with
    // Map collision accepts the dedicated wall classes (bits 3/4) plus class
    // 1: M0001's arena walls are class 1 while its floor shares that class.
    // BlockedXZ therefore also classifies the triangle by its normal.
    static constexpr uint32_t kWallMask  = 0x1A;   // bits 1|3|4

    // Highest floor at (x, z) among triangles whose attribute mask intersects
    // `mask`. Returns false when the point is over nothing.
    bool GetFloor(float x, float z, uint32_t mask, float* out_y) const;

    // True if the XZ segment (x0,z0)->(x1,z1) crosses a triangle matching
    // `mask` whose vertical span overlaps [y, y + height].
    bool BlockedXZ(float x0, float z0, float x1, float z1, float y, float height,
                   uint32_t mask) const;
};

Collision ParseScol(std::vector<uint8_t> file);

// ---------------------------------------------------------------------------
// .gdt ground-attribute grid (ModeGame::Load_GroundAttribute @ 0x2e6cf4)
// ---------------------------------------------------------------------------
struct GroundAttributes {
    int cols = 0, rows = 0;
    float cell_w = 0.f, cell_h = 0.f;
    std::vector<uint32_t> cells;

    uint32_t Get(float local_x, float local_z) const;
};

GroundAttributes ParseGdt(std::vector<uint8_t> file);

// ---------------------------------------------------------------------------
// .smot skeletal animation (magic "Smot")
// ---------------------------------------------------------------------------
struct MotionTrack {
    std::string name;
    uint32_t flags = 0;
    bool has_rotation = false, has_translation = false, has_scale = false;
    std::vector<float> times;                  // one per key
    std::vector<std::array<float, 4>> rot;     // empty if absent
    std::vector<std::array<float, 4>> trans;
};

struct Motion {
    std::vector<uint8_t> data;
    float duration = 0;
    std::vector<MotionTrack> tracks;
};

Motion ParseSmot(std::vector<uint8_t> file);

// ---------------------------------------------------------------------------
// .odt map objects -- see docs/assets.md. Layout is ModeGame::ObjFileLoad's:
// u32 version (must be 2), i32 count, then `count` records of 0xC0 bytes from
// offset 0x40. Positions are WORLD coordinates, not room-local.
// ---------------------------------------------------------------------------
struct MapObject {
    int32_t kind = 0;        // 1 in all 3284 shipping records
    int32_t id = 0;          // resolved through src/engine/object_table.inc
    float pos[3]{};
};

// Returns an empty list (never throws) for a file the engine itself would
// reject, so a room with a malformed table still loads.
std::vector<MapObject> ParseOdt(const std::vector<uint8_t>& file);

// .odt object id -> model name, from the table in the game binary.
// Returns nullptr for an id not in the table.
const char* MapObjectModel(int32_t id);

// ---------------------------------------------------------------------------
// sk1/enemydat.bin -- the game's data tables. DataTableInit() @ 0x2c36bc reads
// this one file; DataTableGetEnemy() @ 0x2c3d4c walks it with a 0x198 stride
// comparing word 0 against the id. 43656 / 408 = 107 records exactly.
//
// Field offsets below are each pinned to a specific engine read, not guessed:
//   +0x04  AppCharacterEnemy::GetStatusMaxHp returns it, and Damage's death
//          path stores it back into the live HP field to reset the enemy
//   +0x08  the enemy's ATTACK power (see the field comment)
//   +0x0C  the subtrahend in Damage's `sub w22, w28, w27`
//   +0x10  passed to GameParameter::AddEXP
//   +0x14  passed to GameParameter::AddRC
// ---------------------------------------------------------------------------
struct EnemyStats {
    int32_t id = 0;
    int32_t max_hp = 0;      // +0x04
    int32_t attack = 0;      // +0x08 -- SetEnemyId passes it to
                             // SetCollisionAttackParam, which stores it at the
                             // param's +0x24, and AppCharacterPlayer::Damage
                             // loads exactly that field as the attack power
    int32_t defence = 0;     // +0x0C
    int32_t exp = 0;         // +0x10
    int32_t money = 0;       // +0x14
    // Fields SetEnemyId @ 0x2b2344 distributes into the actor, identified by
    // their consumers. See docs/assets.md.
    float shadow_size = 0.f;  // +0x60 -> actor +0xaf8, read by GetShadowSize
    float move_speed = 0.f;   // +0x68 -> actor +0xc64, read by UpdateAI and
                              // _UpdateGroundAttribute
    int32_t ai_type = 0;      // +0x64 -> actor +0x3930, the 27-case switch in
                              // AppCharacterBase::UpdateAI. Recorded, not acted
                              // on: the 27 behaviours are not reversed.
    int32_t throw_id = 0;     // +0x6c -> actor +0x3938, read by WeaponThrow

    // The AI state machine, from the record's +0x80..+0x194 block. See below.
    struct AiState {
        int32_t weight[4]{};      // transition weight to states 0..3
        int32_t base = 0, range = 0;   // duration = base + GameRandom(range)
        int32_t weight_sum() const {
            return weight[0] + weight[1] + weight[2] + weight[3];
        }
        // UpdateAI's `cmp w23, #1 / b.lt`: a state with no weight never rolls,
        // so the enemy stays in it.
        bool terminal() const { return weight_sum() < 1; }
    };
    struct AiMachine { AiState state[4]; };
    // Two of them. UpdateAI selects with the toggle at actor +0x3894.
    AiMachine ai[2];
};

// ---------------------------------------------------------------------------
// The enemy AI state machine, RE-VERIFIED.
//
// `AppCharacterBase::SetAITblFromEnemyTbl` @ 0x2a6cb0 copies enemydat's
// +0x80..+0x194 into the actor at +0x377c, as two 140-byte records of four
// 24-byte state descriptors:
//
//     +0x00..+0x0c   four int32 transition weights
//     +0x10..+0x14   {base, range} -- duration in frames
//
// Descriptor bases are +0x80/+0x98/+0xb0/+0xc8 (record 0) and
// +0x10c/+0x124/+0x13c/+0x154 (record 1), each pinned by the copy rather than
// by pattern-matching.
//
// UpdateAI @ 0x2a8d50 chooses the next state by weighted roulette over the
// CURRENT state's weights:
//
//     sum = w0+w1+w2+w3            (addv s0, v0.4s)
//     if (sum < 1) the state stands
//     roll = GameRandom(sum)
//     roll -= w0; if (roll < 0) -> 0
//     roll -= w1; if (roll < 0) -> 1
//     roll -= w2; if (roll < 0) -> 2
//     -> (roll < w3) ? 3 : unchanged
//
// `NextAiState` is that selection, verbatim. `roll` is the caller's
// GameRandom(sum), so the port's RNG substitution stays in one place.
// ---------------------------------------------------------------------------
int NextAiState(const EnemyStats::AiMachine& m, int state, int32_t roll);

// Parses the whole file. Returns empty if the size is not a multiple of 408,
// which is the same size check the engine's own division implies.
std::vector<EnemyStats> ParseEnemyDat(const std::vector<uint8_t>& file);

// tblWeapon, lifted from the binary's .data -- see docs/weapon-table.md.
struct WeaponStats { int32_t id, atk_lo, atk_hi; };
const WeaponStats* FindWeapon(int32_t id);
// VERIFIED, not assumed: GameParameter::Init @ 0x2c6d14 grants weapon 101
// (`mov w1, #0x65` into AddItem), alongside 201, 301 and 401. The second
// argument is `true`, not a count -- the symbol is AddItemEib, (int, bool), and
// that bool gates the write rather than counting anything. Nothing stacks.
constexpr int32_t kStartingWeaponId = 101;

// ---------------------------------------------------------------------------
// sk1/BasicFont.sfont -- the game's bitmap font. Layout from SiFont::SetBinary
// @ 0x35a534, metrics from SiFont::DrawString @ 0x35a858.
// ---------------------------------------------------------------------------
struct Glyph {
    uint16_t x = 0, y = 0;      // position in the atlas
    uint8_t w = 0, h = 0;
    int8_t left = 0, right = 0; // DrawString advances by w + left + right
    int8_t top = 0;             // vertical offset from the line origin
    int Advance() const { return int(w) + left + right; }
};

// A UTF-8 string as code points, the unit both fonts are keyed by. Drawing a
// string BYTE by byte was what dropped the copyright sign and every kana: a
// multi-byte character became two or three lookups that could not match.
std::vector<uint32_t> Utf8Codepoints(const std::string& s);

class Font {
public:
    bool Load(const std::vector<uint8_t>& file);
    // sk1/font_<lang>.bin -- the OTHER font the game ships, and the one it
    // actually draws UI text with. `FontFileLoad` @ 0x2c2608 reads
    // `sk1/font_%s.bin` and `.txt`, hands the .bin to FontTexCreate ->
    // FTData::FTData(platform, void*) @ 0x338588, and the .txt to FontTexFix.
    //
    // The ctor states the layout outright, and the sizes it computes account
    // for every byte of all four shipping fonts:
    //
    //     +0x00 u32   (0 in all four)
    //     +0x04 u32   base font size, in pixels -- also the ROW PITCH
    //     +0x08 u32   (same value again in all four)
    //     +0x0c u32   records per page      (this+0xf8)
    //     +0x10 u32   pages                 (this+0xfc, 1 in all four)
    //     +0x14 u32   texture dimension     (this+0xec, square, 8-bit)
    //     +0x18       pages * records * 0xa04 bytes of records
    //     ...         pages * dim * dim bytes of 8-bit coverage
    //
    // A record is one ROW of the atlas: 128 entries of 0x14 bytes followed by
    // a u32 count at +0xa00. An entry is the character's UTF-8 bytes (NUL
    // padded, 8 bytes), two runtime cache fields, and its width at +0x10.
    //
    // The x position is NOT stored -- FTData::drawCharacter @ 0x339780
    // recomputes it by summing `width + 2` over the preceding entries of the
    // same record (`movi v0.4s, #0x2` @ 0x339a34 in the vector path,
    // `add w10, w10, #0x2` @ 0x339ad8 in the scalar tail). So:
    //
    //     x = sum over i < index of (width_i + 2),   y = record * pitch
    //
    // Glyph ink can spill into that 2px gap -- '#' is 19 wide in a cell of 17
    // -- so the cell is the ADVANCE, not a bounding box, and a glyph drawn at
    // cell width loses at most a pixel of overhang.
    bool LoadFontBin(const std::vector<uint8_t>& file);
    const Glyph* Find(uint32_t codepoint) const;
    // 8-bit coverage, `width * height` bytes.
    const std::vector<uint8_t>& atlas() const { return atlas_; }
    uint32_t width() const { return w_; }
    uint32_t height() const { return h_; }
    // Pixel height of one line of text. For BasicFont it is measured over the
    // glyphs (`top + h`, the lowest any glyph reaches below the line origin);
    // for a font_*.bin it is the file's own row pitch. The two fonts differ by
    // nearly 2x, so every caller scales by this rather than assuming one.
    uint32_t line_height() const { return line_; }
    size_t glyphs() const { return glyphs_.size(); }
    // Pixel width of an ASCII string, using the engine's advance rule.
    int Measure(const std::string& utf8) const;

private:
    uint32_t w_ = 0, h_ = 0;
    std::vector<uint8_t> atlas_;
    std::vector<Glyph> glyphs_;
    std::vector<uint16_t> map_;     // codepoint -> glyph index, 0xFFFF = none
    uint32_t line_ = 0;
};

// ---------------------------------------------------------------------------
// sk1/str_en.bin and sk1/str_ja.bin -- the game's string table. See
// docs/assets.md and tools/asset/strings.py.
// ---------------------------------------------------------------------------
class StringTable {
public:
    // Returns false and leaves the table empty if the file is malformed.
    bool Load(const std::vector<uint8_t>& file);
    // nullptr when the id is not in the table, so a missing string is visible
    // rather than silently becoming "".
    const std::string* Find(const std::string& id) const;
    size_t size() const { return by_id_.size(); }
    // Every id in the table, so a sweep can state its denominator.
    std::vector<std::string> ids() const {
        std::vector<std::string> v;
        v.reserve(by_id_.size());
        for (const auto& [k, _] : by_id_) v.push_back(k);
        return v;
    }

private:
    std::unordered_map<std::string, std::string> by_id_;
};

// ---------------------------------------------------------------------------
// Room extent. ModeGame::RoomLocalToWorldX/Z @ 0x2e3584 turn a script's
// room-local coordinate into a world one as `size.w * grid_x + local`, and
// MakeRoomMinMax @ 0x2e61b8 gives the room the box
// [w*gx, w*(gx+1)] x [h*gy, h*(gy+1)]. The size is per ROOM, not a constant:
// the engine reads it from a table indexed by the room's own class. See
// docs/assets.md.
// ---------------------------------------------------------------------------
struct RoomSize {
    float w = 300.f, h = 240.f;
    enum Source {
        kTable,     // the engine's own world table -- src/engine/world_table.inc
        kGdt,       // measured from the room's own .gdt -- engine-attested
        kAabb,      // inferred from the collision AABB -- see docs/assets.md
        kDefault,   // neither file is present
    } source = kDefault;
    const char* source_name() const {
        switch (source) {
            case kTable: return "world table";
            case kGdt:   return ".gdt";
            case kAabb:  return "collision AABB";
            default:     return "default";
        }
    }
};

// ---------------------------------------------------------------------------
// The engine's world grid, from src/engine/world_table.inc (see
// tools/asset/worldmap.py and docs/world-map.md). 32 worlds; each is a grid of
// cells and each non-empty cell names the room file that sits there. This is
// how the engine turns a position into a room name: Process_Room indexes the
// table at `row * cols + col` and strcpy's the name out of the record.
// ---------------------------------------------------------------------------
struct WorldGrid {
    int cols = 0, rows = 0;
    // Empty string where the grid has a hole -- 879 of 1879 cells are empty.
    const std::string& At(int col, int row) const;
    static const WorldGrid* Get(int world);   // nullptr if out of range
};
// The room at (world, col, row), or "" if that cell is empty or out of range.
const std::string& WorldRoomName(int world, int col, int row);
// Never throws: a room with no size data gets the 300x240 default, flagged as
// such, so a caller can report which rooms it is guessing about.
RoomSize FindRoomSize(const Archive& ar, const std::string& room);

// ---------------------------------------------------------------------------
// Text control codes. 393 of the 1906 strings carry them, e.g.
// "@N(36):\nAren't you...". CnvFormatString @ 0x2c33b4 expands them, and every
// window that shows text runs its string through it first (SetMessageWnd @
// 0x2c7874, SetInfoWnd, SetNameWnd, ...). See docs/assets.md.
// ---------------------------------------------------------------------------
struct FormatParams {
    // oG+0x68 and oG+0xe8, both save-persisted (_GameSaveAccess @ 0x30c874
    // passes each to SaveAccessStr). ModeInit seeds them from the string table.
    std::string hero;  // @H / @h
    std::string girl;  // @G / @g
    // szCnvFormatStringPrm: four 256-byte slots, written by
    // SetMessageWndPrmString(slot, text) @ 0x2c7860. The code letters are not
    // the slot order -- this mapping is read off the four loads at 0x2c35d4.
    std::string prm[4];  // @P = 0, @i = 1, @I = 2, @S = 3
    // The `char**` third argument, indexed by a decimal code (@1, @2, ...).
    // Every caller reached from the dialogue path passes NULL, so this stays
    // empty and an @<n> is reported rather than invented.
    std::vector<std::string> args;
};

// Expands the control codes in `src`. `tbl` resolves @N(nn) -> the
// CHARACTER_NAME_<nn> entry. Unknown codes drop the '@' and keep the letter,
// which is what the engine does (the fall-through at 0x2c3618).
std::string CnvFormatString(const std::string& src, const StringTable* tbl,
                            const FormatParams& prm);

// ---------------------------------------------------------------------------
// GameParameter -- the player's own numbers. Every field offset, constant and
// formula below is read off GameParameter::Init @ 0x2c6d14 (which sets a new
// game's values) and GameParameter::Update @ 0x2c6f14 (which derives the rest
// from them). See docs/assets.md.
// ---------------------------------------------------------------------------
struct PlayerStats {
    // Init's constants. The four stats come from one `movi v0.4s, #2`.
    int32_t level = 1;                          // +0x110
    int32_t power = 2, stamina = 2,             // +0x148, +0x14c
            wisdom = 2, will = 2;               // +0x150, +0x154
    int32_t hp = 19, mp = 6;                    // +0x114, +0x118
    int32_t exp = 0;                            // +0x12c, capped at 999999
    int32_t money = 50;                         // +0x134, capped at 65535
    // The four equipment slots Init fills, each with AddItem(id, true).
    int32_t weapon = 101, helm = 201, armor = 301, accessory = 401;

    // Update recomputes these every frame; they are not stored state.
    // Both reproduce Init's own constants at stat 2, which is the check that
    // they are right rather than merely plausible: 2*2/10 + 19 == 19, and
    // 2*94/100 + 5 == 6.
    int32_t max_hp() const {                    // -> +0x11c
        int32_t s = Cap(stamina);
        return s * s >= 9810 ? 999 : s * s / 10 + 19;
    }
    int32_t max_mp() const { return Cap(wisdom) * 94 / 100 + 5; }   // -> +0x120
    // Attack is the POWER stat plus the equipped weapon's table entry; defence
    // is STAMINA + 1 plus helm and armour. Note it is stamina, not a separate
    // defence stat, that carries both HP and defence.
    int32_t attack() const;                     // -> +0x124
    int32_t defence() const;                    // -> +0x128
    // 12L + 3L^2 + 103L^3/100, capped at 999999.
    int32_t next_exp() const {                  // -> +0x130
        int64_t l = level;
        int64_t v = 12 * l + 3 * l * l + 103 * l * l * l / 100;
        return int32_t(v > 999999 ? 999999 : v);
    }
    // Update clamps every stat to 99 before using it.
    static int32_t Cap(int32_t v) { return v > 99 ? 99 : v < 0 ? 0 : v; }
    static int32_t Clamp(int64_t v, int64_t hi) {
        return int32_t(v < 0 ? 0 : v > hi ? hi : v);
    }

    // GameParameter::AddEXP @ 0x2c72f8 and ::AddRC @ 0x2c731c. Both clamp a
    // negative result to zero (`bic w8, w8, w8, asr #31`) before capping.
    void AddExp(int32_t n) { exp = Clamp(exp + n, 999999); }
    void AddMoney(int32_t n) { money = Clamp(money + n, 65535); }
    // ModeGame::CheckLevelUp @ 0x2e000c, minus the two conditions the port has
    // no state for (a fade in progress, and the player's own control state):
    // alive, enough EXP, and not past level 98. Process_LevelUp is driven by
    // the level-up SCREEN, which the port does not have, so this only says one
    // is due.
    bool level_up_due() const {
        return hp >= 1 && exp >= next_exp() && level <= 98;
    }

    // The four training regimens the game offers on a level-up, in the order
    // ModeGame::Process_LevelUp indexes tblLevelup with -- the same order as
    // SYS_LEVELUP_TYPE_1..4.
    enum Regimen { kWarrior = 0, kMonk = 1, kMage = 2, kSage = 3 };
    static const char* RegimenName(int r);
    // Applies one level-up: +1 level, the chosen row of tblLevelup added to the
    // stats, then HP and MP refilled -- Process_LevelUp @ 0x2e0100 writes max
    // HP and max MP straight back into the current values after calling Update.
    void LevelUp(int regimen);
};

// ---------------------------------------------------------------------------
// Inventory. Four fixed-size bags living inside GameParameter, which is at
// oG+0x60, so the save's first post-header field oG+0x1c8 IS bag 0 -- the
// 92-byte header ends exactly where the inventory begins.
//
// The layout is read out of two functions that must agree, and do:
//
//   GameParameter::IsHaveItem @ 0x2c5678 -- the compiler fully unrolled its
//   search, so the bases it touches enumerate every slot: +0x168 stride 12
//   sixteen times, then +0x228 stride 8 sixteen times, then +0x2a8 stride 8
//   sixteen times.
//
//   _GameSaveAccess @ 0x30c820 -- walks bag 0 with `add x25, x25, #0xc` and
//   `cmp x25, #0xc0`, i.e. 16 records of 12 bytes, independently confirming
//   both the stride and the count.
//
// The bag a given id goes in is chosen by DataTableGetIdType @ 0x2c387c:
//
//     type 1  ids   1..37    kItem        bag 0
//     type 2  ids 101..118   kWeapon      bag 1
//     type 4  ids 201..206   kHelm    \
//     type 5  ids 301..309   kArmor    >  bag 2, shared
//     type 6  ids 401..409   kAccessory /
//     type 7  ids 501..508   kMagic       bag 3, direct-indexed
//     type 3 never occurs and type 0 is "no table".
//
// Bags 0..2 are searched linearly by id; bag 3 is not searched at all, because
// magic is addressed straight off the id -- `add x8, x19, w20, sxtw #3` then
// `sub x8, x8, #0xc80`, which for id 501 lands on +0x328, exactly where bag 2
// ends. The four bags tile GameParameter+0x168..+0x368 with no gaps, and that
// closure is the strongest evidence the entry counts are right.
//
// A slot's first word is the id, and 0 means empty. The second word is NOT a
// quantity: AddItem's write tail does
//
//     stp w20, w0, [x22]          slot->id = id;  slot->seq = *counter
//     str w8,  [x19, #0x368]      *counter += 1
//
// so it is the ACQUISITION ORDER, taken from a monotonic counter that lives at
// GameParameter+0x368 -- immediately after the magic bag, closing the region.
// GameParameter::SearchSlotGetCnt @ 0x2c66f4 confirms the direction of use: it
// searches bag 0 for the slot whose SECOND word equals its argument, which is
// how the item list walks the bag in pickup order.
//
// Nothing stacks. AddItem takes the first slot whose id is 0 and fails when all
// 16 are taken -- neither the item path nor the equipment path compares the id
// being added against the ids already held. Its `bool` parameter gates the
// write, so AddItem(id, false) is a dry run; that is what IsAddItem @ 0x2cd8b0
// uses, while the global AddItem @ 0x2cd8e4 passes true (and, incidentally,
// recomputes GameParameter as oG+0x60, confirming that offset a third time).
//
// Bag 0's third word is written as DataTableGetItem(id)+0x4, forced to 0 when
// that value is 1. tblItem+0x4 is the item's kind, whose meaning is still open
// -- see docs/re-frontier.md.
//
// NOT REVERSED: how DelItem renumbers the sequence keys of the slots after the
// one it clears, and whether the counter at +0x368 starts at 0 or 1.
// ---------------------------------------------------------------------------
struct Inventory {
    enum Bag { kItems = 0, kWeapons = 1, kArmour = 2, kMagic = 3, kBagCount = 4 };
    static constexpr int kSlots = 16;          // bags 0..2
    static constexpr int kMagicSlots = 8;      // ids 501..508
    static constexpr int32_t kMagicFirstId = 501;

    struct Slot {
        int32_t id = 0;      // +0x0, 0 = empty
        int32_t seq = 0;     // +0x4, acquisition order
        int32_t kind = 0;    // +0x8, bag 0 only
    };

    Slot items[kSlots];
    Slot weapons[kSlots];
    Slot armour[kSlots];
    Slot magic[kMagicSlots];
    int32_t seq_counter = 0;   // GameParameter+0x368

    // DataTableGetIdType @ 0x2c387c, verbatim. 0 means "in no table".
    static int IdType(int32_t id);
    // Which bag an id belongs in, or -1 if none. Types 4, 5 and 6 share bag 2.
    static int BagOf(int32_t id);

    Slot* bag(int b, int* n);
    const Slot* bag(int b, int* n) const;

    // Slots holding this id. Nothing stacks, so this is a slot count.
    int32_t Count(int32_t id) const;
    bool Has(int32_t id) const { return Count(id) > 0; }
    // GameParameter::AddItem @ 0x2c625c. First free slot; false when the bag is
    // full. `commit == false` is the dry run IsAddItem uses.
    bool Add(int32_t id, bool commit = true);
    // Clears the first slot holding this id. False when none does.
    bool Del(int32_t id);

    // GameParameter::Init @ 0x2c6d14: a new game is granted 101, 201, 301 and
    // 401 -- the same four ids PlayerStats starts equipped with, because being
    // equipped IS holding them.
    void NewGame();
};

// ---------------------------------------------------------------------------
// Damage. AppCharacterEnemy::Damage @ 0x2b2b00 computes it in one run of
// arithmetic at 0x2b3418..0x2b34a0. See docs/assets.md.
//
//     def_eff = weak_to_this_attack ? defence / 4 : defence
//     base    = (attack - def_eff + magic) * (gauge + 16000) / 16000
//     damage  = max(1, base + base * rand(0..24) / 100)
//
// `gauge` is the charge meter at oG+0x1b8, so an empty meter is 1x and a full
// one (16000) is 2x. `magic` is the attacker's magical attack, which
// SetCollisionAttackParam @ 0x2b7e8c stores next to the physical one.
// ---------------------------------------------------------------------------
struct DamageInput {
    int32_t attack = 0;
    int32_t defence = 0;
    int32_t magic = 0;
    float gauge = 0.f;      // oG+0x1b8; the port has no charge meter yet
    bool weak = false;      // the attack matches one of the enemy's weaknesses
    int32_t roll = 0;       // GameRandom(25): 0..24
};
// The floor at 1 is the ENGINE's, not a port choice: `cmp w8, #1` followed by
// `csinc w22, w8, wzr, gt` @ 0x2b349c.
int32_t ComputeDamage(const DamageInput& in);

// The same shape rewards the kill: AddEXP(exp + exp*GameRandom(11)/100) and
// AddRC(money + money*GameRandom(11)/100) @ 0x2b34b4..0x2b3524.
inline int32_t RewardWithBonus(int32_t base, int32_t roll) {
    return base + int32_t(int64_t(base) * roll / 100);
}

// tblHelm / tblArmor defence, indexed by DataTableGetDefence @ 0x2c3bd8.
// Returns 0 for an id not in either table.
int32_t EquipDefence(int32_t id);
// The other two ids GameParameter::Init grants a new game (AddItem 0xc9, 0x12d).
constexpr int32_t kStartingHelmId = 201;
constexpr int32_t kStartingArmorId = 301;
// The fourth, completing Init's set. It is an accessory (type 6) by
// DataTableGetIdType's ranges, and carries no defence -- EquipDefence covers
// helms and armour only.
constexpr int32_t kStartingAccessoryId = 401;

}  // namespace mcf
