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

}  // namespace mcf
