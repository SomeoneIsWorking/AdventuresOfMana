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
    static constexpr uint32_t kWallMask  = 0x18;   // bits 3|4

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
};

// Parses the whole file. Returns empty if the size is not a multiple of 408,
// which is the same size check the engine's own division implies.
std::vector<EnemyStats> ParseEnemyDat(const std::vector<uint8_t>& file);

// tblWeapon, lifted from the binary's .data -- see docs/weapon-table.md.
struct WeaponStats { int32_t id, atk_lo, atk_hi; };
const WeaponStats* FindWeapon(int32_t id);
// VERIFIED, not assumed: GameParameter::Init @ 0x2c6d14 grants weapon 101
// (`mov w1, #0x65` into AddItem with count 1), alongside 201, 301 and 401.
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

class Font {
public:
    bool Load(const std::vector<uint8_t>& file);
    const Glyph* Find(uint32_t codepoint) const;
    // 8-bit coverage, `width * height` bytes.
    const std::vector<uint8_t>& atlas() const { return atlas_; }
    uint32_t width() const { return w_; }
    uint32_t height() const { return h_; }
    size_t glyphs() const { return glyphs_.size(); }
    // Pixel width of an ASCII string, using the engine's advance rule.
    int Measure(const std::string& utf8) const;

private:
    uint32_t w_ = 0, h_ = 0;
    std::vector<uint8_t> atlas_;
    std::vector<Glyph> glyphs_;
    std::vector<uint16_t> map_;     // codepoint -> glyph index, 0xFFFF = none
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
        kGdt,       // measured from the room's own .gdt -- engine-attested
        kAabb,      // inferred from the collision AABB -- see docs/assets.md
        kDefault,   // neither file is present
    } source = kDefault;
    const char* source_name() const {
        return source == kGdt ? ".gdt" : source == kAabb ? "collision AABB" : "default";
    }
};
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

// tblHelm / tblArmor defence, indexed by DataTableGetDefence @ 0x2c3bd8.
// Returns 0 for an id not in either table.
int32_t EquipDefence(int32_t id);
// The other two ids GameParameter::Init grants a new game (AddItem 0xc9, 0x12d).
constexpr int32_t kStartingHelmId = 201;
constexpr int32_t kStartingArmorId = 301;

}  // namespace mcf
