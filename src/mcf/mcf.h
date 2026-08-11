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

}  // namespace mcf
