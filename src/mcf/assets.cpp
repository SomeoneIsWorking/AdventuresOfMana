// MPK archive, .stex textures and .smdl models. See docs/mpk-format.md and
// docs/assets.md; mirrors tools/asset/{mpk,stex,smdl}.py.
#include "mcf/mcf.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <format>

#include <lucent/log.h>

namespace mcf {
namespace {

uint32_t RdU32(std::span<const uint8_t> b, size_t o) {
    if (o + 4 > b.size())
        throw Error(std::format("read past end of buffer at {} (size {})", o, b.size()));
    uint32_t v;
    std::memcpy(&v, b.data() + o, 4);
    return v;
}
int32_t RdS32(std::span<const uint8_t> b, size_t o) { return int32_t(RdU32(b, o)); }

std::string RdCStr(std::span<const uint8_t> b, size_t o) {
    if (o >= b.size()) throw Error(std::format("string offset {} past end", o));
    size_t e = o;
    while (e < b.size() && b[e]) ++e;
    return std::string(reinterpret_cast<const char*>(b.data()) + o, e - o);
}

constexpr uint32_t kMagicMcfa = 0x6166636D;  // "mcfa"
constexpr uint32_t kMagicSmdi = 0x49444D53;  // "SMDI"
constexpr uint32_t kMagicSmd3 = 0x33646D53;  // "Smd3"

constexpr size_t kMpkEntrySize = 256;
constexpr size_t kMpkNameMax = 240;

}  // namespace

// ---------------------------------------------------------------------------
// Archive
// ---------------------------------------------------------------------------
Archive::Archive(const std::string& path) : path_(path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw Error(std::format("cannot open archive '{}'", path));

    uint8_t hdr[16];
    if (std::fread(hdr, 1, 16, f) != 16) {
        std::fclose(f);
        throw Error(std::format("'{}' is too short to be an MPK archive", path));
    }
    std::span<const uint8_t> h(hdr, 16);
    if (RdU32(h, 0) != kMagicMcfa) {
        std::fclose(f);
        throw Error(std::format("'{}' is not an MPK archive (bad magic)", path));
    }
    uint32_t v = RdU32(h, 4);
    uint32_t version = v >> 24, count = v & 0xFFFFFF;
    uint32_t dir_csize = RdU32(h, 8);
    payload_ = RdU32(h, 12);
    if (version != 1) {
        std::fclose(f);
        throw Error(std::format("unsupported MPK version {}", version));
    }

    std::vector<uint8_t> comp(dir_csize);
    if (std::fread(comp.data(), 1, dir_csize, f) != dir_csize) {
        std::fclose(f);
        throw Error("truncated MPK directory");
    }
    std::fclose(f);

    std::vector<uint8_t> dir(size_t(count) * kMpkEntrySize);
    lha::Decode(comp, dir);

    entries_.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        size_t o = size_t(i) * kMpkEntrySize;
        Entry e;
        e.name = RdCStr(dir, o);
        e.flags = RdU32(dir, o + kMpkNameMax);
        e.offset = RdU32(dir, o + kMpkNameMax + 4);
        e.csize = RdU32(dir, o + kMpkNameMax + 8);
        e.usize = RdU32(dir, o + kMpkNameMax + 12);
        index_[e.name] = entries_.size();
        entries_.push_back(std::move(e));
    }
}

std::vector<uint8_t> Archive::Read(const Entry& e) const {
    FILE* f = std::fopen(path_.c_str(), "rb");
    if (!f) throw Error(std::format("cannot reopen archive '{}'", path_));
    std::vector<uint8_t> comp(e.csize);
    if (std::fseek(f, long(payload_) + long(e.offset), SEEK_SET) != 0 ||
        std::fread(comp.data(), 1, e.csize, f) != e.csize) {
        std::fclose(f);
        throw Error(std::format("truncated stream for '{}'", e.name));
    }
    std::fclose(f);

    std::vector<uint8_t> out(e.usize);
    size_t used = 0;
    lha::Decode(comp, out, &used);
    // The output buffer is caller-sized, so its length proves nothing. Bytes
    // consumed is the only real check that this stream decoded correctly.
    if (used != e.csize)
        throw Error(std::format("'{}': decoder consumed {} of {} compressed bytes",
                                e.name, used, e.csize));
    return out;
}

std::string Archive::FindByPrefix(std::string_view prefix) const {
    for (const auto& e : entries_)
        if (e.name.size() >= prefix.size() &&
            e.name.compare(0, prefix.size(), prefix) == 0)
            return e.name;
    return {};
}

std::vector<uint8_t> Archive::Read(std::string_view name) const {
    auto it = index_.find(std::string(name));
    if (it == index_.end())
        throw Error(std::format("'{}' not present in archive", name));
    return Read(entries_[it->second]);
}

// ---------------------------------------------------------------------------
// .stex
// ---------------------------------------------------------------------------
TextureSet ParseStex(std::vector<uint8_t> file) {
    TextureSet ts;
    ts.data = std::move(file);
    std::span<const uint8_t> b(ts.data);
    if (b.size() < 16 || RdU32(b, 0) != kMagicSmdi)
        throw Error("not a SMDI texture container");

    uint32_t tbl = RdU32(b, 8), count = RdU32(b, 12);
    for (uint32_t i = 0; i < count; ++i) {
        size_t d = tbl + size_t(i) * 32;
        Texture t;
        t.fmtcode = RdU32(b, d + 4);
        t.width = RdU32(b, d + 8);
        t.height = RdU32(b, d + 12);
        t.mips = RdU32(b, d + 16);
        uint32_t off = RdU32(b, d + 20), size = RdU32(b, d + 24);
        t.name = RdCStr(b, RdU32(b, d + 28));
        if (size_t(off) + size > b.size())
            throw Error(std::format("texture '{}' pixel data {}..{} exceeds file ({})",
                                    t.name, off, off + size, b.size()));
        t.pixels = b.subspan(off, size);
        ts.textures.push_back(std::move(t));
    }
    return ts;
}

TextureSet ParseMtex(std::vector<uint8_t> file, std::string name) {
    TextureSet ts;
    ts.data = std::move(file);
    std::span<const uint8_t> b(ts.data);
    if (b.size() < 128) throw Error("truncated .mtex");

    Texture t;
    uint32_t fmtcode = RdU32(b, 0);
    t.fmtcode = fmtcode;
    t.width = RdU32(b, 4);
    t.height = RdU32(b, 8);
    t.mips = RdU32(b, 12) + 1;   // stored value is the MAX LEVEL, not the count
    uint32_t size = RdU32(b, 16);
    t.name = std::move(name);
    if (128 + size > b.size())
        throw Error(std::format(".mtex '{}': 128 + {} exceeds file size {}", t.name,
                                size, b.size()));
    t.pixels = b.subspan(128, size);
    ts.textures.push_back(std::move(t));
    return ts;
}

std::vector<std::string> ParseStexInfo(std::span<const uint8_t> b) {
    if (b.size() < 4) throw Error("truncated .stexinfo");
    uint32_t n = RdU32(b, 0);
    if (4 + size_t(n) * 256 > b.size())
        throw Error(std::format(".stexinfo declares {} entries but holds {} bytes",
                                n, b.size()));
    std::vector<std::string> names;
    for (uint32_t i = 0; i < n; ++i) names.push_back(RdCStr(b, 4 + size_t(i) * 256));
    return names;
}

// ---------------------------------------------------------------------------
// .smdl
// ---------------------------------------------------------------------------
uint32_t VertexTypeSize(VertexType t) {
    switch (t) {
        case VertexType::kFloat2: return 8;
        case VertexType::kFloat3: return 12;
        case VertexType::kFloat4: return 16;
        case VertexType::kUByte4Color:
        case VertexType::kUByte4Index: return 4;
    }
    throw Error(std::format("unknown vertex type {}", uint32_t(t)));
}

const VertexAttribute* Model::Find(VertexUsage u) const {
    for (const auto& a : layout)
        if (a.usage == u) return &a;
    return nullptr;
}

Model ParseSmdl(std::vector<uint8_t> file) {
    Model m;
    m.data = std::move(file);
    std::span<const uint8_t> b(m.data);
    if (b.size() < 0x68 || RdU32(b, 0) != kMagicSmd3)
        throw Error("not an Smd3 model");

    auto section = [&](int k) {
        return std::pair<uint32_t, int32_t>{RdU32(b, 8 + k * 8), RdS32(b, 12 + k * 8)};
    };

    m.bone_count = section(1).first;

    auto [vsec_n, vsec_off] = section(6);
    auto [isec_n, isec_off] = section(7);
    auto [decl_n, decl_off] = section(5);
    auto [str_n, str_off] = section(8);
    (void)vsec_n; (void)isec_n; (void)str_n;

    m.vertex_count = RdU32(b, vsec_off);
    m.vertex_offset = uint32_t(RdS32(b, vsec_off + 4));
    m.vertex_stride = RdU32(b, vsec_off + 8);

    m.index_count = RdU32(b, isec_off);
    m.index_offset = uint32_t(RdS32(b, isec_off + 4));
    m.index_size = RdU32(b, isec_off + 8);
    if (m.index_size != 2 && m.index_size != 4)
        throw Error(std::format("index element size {}; loader accepts only 2 or 4",
                                m.index_size));

    // The sections tile the file with no slack; that is what validates the
    // offsets rather than merely making them plausible.
    uint32_t vend = m.vertex_offset + m.vertex_count * m.vertex_stride;
    uint32_t iend = m.index_offset + m.index_count * m.index_size;
    if (vend != m.index_offset)
        throw Error(std::format("vertex region ends at {} but index data starts at {}",
                                vend, m.index_offset));
    if (iend != uint32_t(str_off))
        throw Error(std::format("index region ends at {} but string table starts at {}",
                                iend, str_off));

    // Materials (section 0, 80-byte records) and draw ranges (section 4,
    // 32-byte records). The ranges tile the index buffer with no gaps in all
    // 1375 shipped models, which is what validates them.
    auto [mat_n, mat_off] = section(0);
    for (uint32_t i = 0; i < mat_n; ++i) {
        size_t d = size_t(mat_off) + size_t(i) * 80;
        Material mt;
        mt.texture_index = RdU32(b, d + 0x10);
        mt.name = RdCStr(b, size_t(str_off) + RdU32(b, d + 0x28));
        mt.source_path = RdCStr(b, size_t(str_off) + RdU32(b, d + 0x2C));
        mt.blend = RdU32(b, d + 0x24) == 1;
        m.materials.push_back(std::move(mt));
    }

    auto [draw_n, draw_off] = section(4);
    uint32_t expect = 0, drawn = 0;
    for (uint32_t i = 0; i < draw_n; ++i) {
        size_t d = size_t(draw_off) + size_t(i) * 32;
        DrawRange r{RdU32(b, d), RdU32(b, d + 4), RdU32(b, d + 8)};
        if (r.byte_offset != expect)
            throw Error(std::format("draw range {} starts at byte {}, expected {}",
                                    i, r.byte_offset, expect));
        expect += r.index_count * m.index_size;
        drawn += r.index_count;
        m.draws.push_back(r);
    }
    if (draw_n && drawn != m.index_count)
        throw Error(std::format("draw ranges cover {} indices, buffer holds {}",
                                drawn, m.index_count));

    // Skeleton: section 1, 352-byte SiModelBone records.
    auto [bone_n, bone_off] = section(1);
    for (uint32_t i = 0; i < bone_n; ++i) {
        size_t d = size_t(bone_off) + size_t(i) * 352;
        Bone bn;
        bn.parent = RdS32(b, d + 0x144);
        bn.name = RdCStr(b, size_t(str_off) + RdU32(b, d + 0x14C));
        std::memcpy(bn.local, b.data() + d, 64);
        std::memcpy(bn.inv_world, b.data() + d + 0x80, 64);
        for (float v : bn.inv_world)
            if (!std::isfinite(v)) { bn.degenerate = true; break; }
        m.bones.push_back(std::move(bn));
    }

    uint32_t declared_end = 0;
    for (uint32_t i = 0; i < decl_n; ++i) {
        size_t d = size_t(decl_off) + size_t(i) * 32;
        VertexAttribute a{VertexUsage(RdU32(b, d)), VertexType(RdU32(b, d + 4)),
                          RdU32(b, d + 8)};
        declared_end = std::max(declared_end, a.offset + VertexTypeSize(a.type));
        m.layout.push_back(a);
    }
    if (declared_end != m.vertex_stride)
        throw Error(std::format("vertex declaration ends at {} but stride is {}",
                                declared_end, m.vertex_stride));
    return m;
}

// ---------------------------------------------------------------------------
// .scol
// ---------------------------------------------------------------------------
Collision ParseScol(std::vector<uint8_t> file) {
    Collision c;
    c.data = std::move(file);
    std::span<const uint8_t> b(c.data);
    if (b.size() < 0x34 || RdU32(b, 0) != 0x6C6F4353 /* "SCol" */)
        throw Error("not an SCol collision mesh");

    c.tri_count  = RdU32(b, 0x08);
    c.tri_off    = uint32_t(RdS32(b, 0x0C));
    uint32_t lo_i = RdU32(b, 0x10), hi_i = RdU32(b, 0x14);
    c.cell_count = RdU32(b, 0x18);
    c.cell_off   = uint32_t(RdS32(b, 0x1C));
    uint32_t total = RdU32(b, 0x24);
    c.vec_count  = RdU32(b, 0x28);
    c.vec_off    = uint32_t(RdS32(b, 0x2C));

    if (total != b.size())
        throw Error(std::format("collision header size {} != file size {}", total, b.size()));
    if (c.tri_off + c.tri_count * 40 != c.vec_off)
        throw Error("triangle array does not abut the vec3 pool");
    if (c.vec_off + c.vec_count * 12 != b.size())
        throw Error("vec3 pool does not run to end of file");
    if (c.cell_off + c.cell_count * 32 > c.tri_off)
        throw Error("cell array overruns the triangle array");
    if (lo_i >= c.vec_count || hi_i >= c.vec_count)
        throw Error("AABB index out of range");

    std::memcpy(c.aabb_lo, b.data() + c.vec_off + size_t(lo_i) * 12, 12);
    std::memcpy(c.aabb_hi, b.data() + c.vec_off + size_t(hi_i) * 12, 12);
    return c;
}

bool Collision::GetFloor(float x, float z, uint32_t mask, float* out_y) const {
    std::span<const uint8_t> b(data);
    auto vec = [&](uint32_t i, float v[3]) {
        std::memcpy(v, b.data() + vec_off + size_t(i) * 12, 12);
    };
    if (x < aabb_lo[0] || x > aabb_hi[0] || z < aabb_lo[2] || z > aabb_hi[2])
        return false;

    bool hit = false;
    float best = -1e30f;
    for (uint32_t ci = 0; ci < cell_count; ++ci) {
        size_t e = cell_off + size_t(ci) * 32;
        float clo[3], chi[3];
        vec(RdU32(b, e), clo);
        vec(RdU32(b, e + 4), chi);
        if (x < clo[0] || x > chi[0] || z < clo[2] || z > chi[2]) continue;

        uint32_t n = RdU32(b, e + 0x0C), list = RdU32(b, e + 0x10);
        for (uint32_t k = 0; k < n; ++k) {
            uint32_t ti = RdU32(b, list + size_t(k) * 4);
            size_t t = tri_off + size_t(ti) * 40;
            if (!(RdU32(b, t + 0x24) & mask)) continue;
            float p[3][3];
            for (int j = 0; j < 3; ++j) vec(RdU32(b, t + size_t(j) * 4), p[j]);

            // Barycentric point-in-triangle in XZ, then interpolate Y.
            float d = (p[1][2] - p[2][2]) * (p[0][0] - p[2][0]) +
                      (p[2][0] - p[1][0]) * (p[0][2] - p[2][2]);
            if (d == 0) continue;
            float w0 = ((p[1][2] - p[2][2]) * (x - p[2][0]) +
                        (p[2][0] - p[1][0]) * (z - p[2][2])) / d;
            float w1 = ((p[2][2] - p[0][2]) * (x - p[2][0]) +
                        (p[0][0] - p[2][0]) * (z - p[2][2])) / d;
            float w2 = 1.f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            float y = w0 * p[0][1] + w1 * p[1][1] + w2 * p[2][1];
            if (!hit || y > best) { best = y; hit = true; }
        }
    }
    if (hit) *out_y = best;
    return hit;
}

namespace {
// 2D segment intersection, used for the XZ wall test.
bool SegHit(float ax, float az, float bx, float bz,
            float cx, float cz, float dx, float dz) {
    auto cross = [](float ux, float uz, float vx, float vz) { return ux * vz - uz * vx; };
    float r1 = cross(bx - ax, bz - az, cx - ax, cz - az);
    float r2 = cross(bx - ax, bz - az, dx - ax, dz - az);
    float r3 = cross(dx - cx, dz - cz, ax - cx, az - cz);
    float r4 = cross(dx - cx, dz - cz, bx - cx, bz - cz);
    return ((r1 > 0) != (r2 > 0)) && ((r3 > 0) != (r4 > 0));
}
}  // namespace

bool Collision::BlockedXZ(float x0, float z0, float x1, float z1, float y,
                          float height, uint32_t mask) const {
    std::span<const uint8_t> b(data);
    auto vec = [&](uint32_t i, float v[3]) {
        std::memcpy(v, b.data() + vec_off + size_t(i) * 12, 12);
    };
    float lox = std::min(x0, x1), hix = std::max(x0, x1);
    float loz = std::min(z0, z1), hiz = std::max(z0, z1);

    for (uint32_t ci = 0; ci < cell_count; ++ci) {
        size_t e = cell_off + size_t(ci) * 32;
        float clo[3], chi[3];
        vec(RdU32(b, e), clo);
        vec(RdU32(b, e + 4), chi);
        if (hix < clo[0] || lox > chi[0] || hiz < clo[2] || loz > chi[2]) continue;

        uint32_t n = RdU32(b, e + 0x0C), list = RdU32(b, e + 0x10);
        for (uint32_t k = 0; k < n; ++k) {
            uint32_t ti = RdU32(b, list + size_t(k) * 4);
            size_t t = tri_off + size_t(ti) * 40;
            if (!(RdU32(b, t + 0x24) & mask)) continue;
            float p[3][3];
            for (int j = 0; j < 3; ++j) vec(RdU32(b, t + size_t(j) * 4), p[j]);

            // Only walls the mover could actually hit at its own height.
            float tlo = std::min({p[0][1], p[1][1], p[2][1]});
            float thi = std::max({p[0][1], p[1][1], p[2][1]});
            if (thi < y || tlo > y + height) continue;

            for (int j = 0; j < 3; ++j) {
                int m = (j + 1) % 3;
                if (SegHit(x0, z0, x1, z1, p[j][0], p[j][2], p[m][0], p[m][2]))
                    return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// .smot
// ---------------------------------------------------------------------------
Motion ParseSmot(std::vector<uint8_t> file) {
    Motion mo;
    mo.data = std::move(file);
    std::span<const uint8_t> b(mo.data);
    if (b.size() < 0x28 || RdU32(b, 0) != 0x746F6D53 /* "Smot" */)
        throw Error("not an Smot motion");

    uint32_t count = RdU32(b, 8);
    int32_t tbl = RdS32(b, 12);
    std::memcpy(&mo.duration, b.data() + 0x1C, 4);
    uint32_t total = RdU32(b, 0x20);
    if (total != b.size())
        throw Error(std::format("motion header size {} != file size {}", total, b.size()));

    for (uint32_t i = 0; i < count; ++i) {
        size_t e = size_t(tbl) + size_t(i) * 32;
        uint32_t flags = RdU32(b, e);
        uint32_t keys = RdU32(b, e + 4);
        int32_t voff = RdS32(b, e + 8);
        uint32_t vstride = RdU32(b, e + 12);
        int32_t toff = RdS32(b, e + 16);

        MotionTrack t;
        t.name = RdCStr(b, RdU32(b, e + 20));
        t.flags = flags;
        t.has_rotation = flags & 4;
        t.has_translation = flags & 2;
        t.has_scale = flags & 8;

        // One 16-byte channel per set bit of flags & 0b1110. Asserting the rule
        // rather than whitelisting strides: a {16,32} whitelist silently missed
        // the 48-byte (rotation+translation+scale) tracks in 142 files.
        uint32_t want = 16 * unsigned(__builtin_popcount(flags & 0b1110));
        if (vstride != want)
            throw Error(std::format("track '{}': flags {:#x} imply stride {} but file "
                                    "says {}", t.name, flags, want, vstride));
        if (uint32_t(toff) + keys * 4 != uint32_t(voff))
            throw Error(std::format("track '{}': time array does not abut values",
                                    t.name));

        t.times.resize(keys);
        std::memcpy(t.times.data(), b.data() + toff, size_t(keys) * 4);
        // Channels are stored in bit order: rotation, then translation, then scale.
        uint32_t ch = 0;
        auto grab = [&](std::vector<std::array<float, 4>>& dst, bool present) {
            if (!present) return;
            dst.resize(keys);
            for (uint32_t k = 0; k < keys; ++k)
                std::memcpy(dst[k].data(),
                            b.data() + voff + size_t(k) * vstride + size_t(ch) * 16, 16);
            ++ch;
        };
        grab(t.rot, t.has_rotation);
        grab(t.trans, t.has_translation);
        mo.tracks.push_back(std::move(t));
    }
    return mo;
}

// ---------------------------------------------------------------------------
// .odt map objects. Header and stride are ModeGame::ObjFileLoad's own: it gates
// on `cmp w8, #0x2` for the version, skips the file unless the count is >= 1,
// and walks records with `add x8, x27, #0xc0` from offset 0x40.
// ---------------------------------------------------------------------------
std::vector<MapObject> ParseOdt(const std::vector<uint8_t>& file) {
    std::vector<MapObject> out;
    if (file.size() < 8) return out;
    uint32_t version = RdU32(file, 0);
    int32_t count = RdS32(file, 4);
    if (version != 2 || count < 1) return out;
    if (size_t(0x40) + size_t(count) * 0xC0 != file.size()) return out;
    out.reserve(size_t(count));
    for (int32_t i = 0; i < count; ++i) {
        size_t o = 0x40 + size_t(i) * 0xC0;
        MapObject m;
        m.kind = RdS32(file, o);
        m.id = RdS32(file, o + 4);
        for (int k = 0; k < 3; ++k) {
            uint32_t bits = RdU32(file, o + 8 + size_t(k) * 4);
            std::memcpy(&m.pos[k], &bits, 4);
        }
        out.push_back(m);
    }
    return out;
}

const char* MapObjectModel(int32_t id) {
    switch (id) {
#define OBJ(n, name) case n: return name;
#include "engine/object_table.inc"
#undef OBJ
        default: return nullptr;
    }
}

// ---------------------------------------------------------------------------
// sk1/enemydat.bin. Stride 0x198 is the engine's own: DataTableGetEnemy walks
// records with `add x0, x0, #0x198` and derives the count by dividing the
// stored file size by 408.
// ---------------------------------------------------------------------------
std::vector<EnemyStats> ParseEnemyDat(const std::vector<uint8_t>& file) {
    std::vector<EnemyStats> out;
    constexpr size_t kStride = 0x198;
    if (file.empty() || file.size() % kStride) return out;
    out.reserve(file.size() / kStride);
    for (size_t o = 0; o + kStride <= file.size(); o += kStride) {
        EnemyStats e;
        e.id = RdS32(file, o + 0x00);
        e.max_hp = RdS32(file, o + 0x04);
        e.attack = RdS32(file, o + 0x08);
        e.defence = RdS32(file, o + 0x0C);
        e.exp = RdS32(file, o + 0x10);
        e.money = RdS32(file, o + 0x14);
        std::memcpy(&e.shadow_size, file.data() + o + 0x60, 4);
        std::memcpy(&e.move_speed, file.data() + o + 0x68, 4);
        e.ai_type = RdS32(file, o + 0x64);
        e.throw_id = RdS32(file, o + 0x6c);
        // The AI state machine. The eight descriptor bases are the offsets
        // SetAITblFromEnemyTbl @ 0x2a6cb0 copies into the actor's two 140-byte
        // records; they are transcribed from that function, not spaced by a
        // guessed stride.
        static constexpr size_t kDesc[2][4] = {
            {0x80, 0x98, 0xB0, 0xC8}, {0x10C, 0x124, 0x13C, 0x154}};
        for (int r = 0; r < 2; ++r) {
            for (int s = 0; s < 4; ++s) {
                auto& st = e.ai[r].state[s];
                size_t b = o + kDesc[r][s];
                for (int w = 0; w < 4; ++w) st.weight[w] = RdS32(file, b + w * 4);
                st.base = RdS32(file, b + 0x10);
                st.range = RdS32(file, b + 0x14);
            }
        }
        out.push_back(e);
    }
    return out;
}

int NextAiState(const EnemyStats::AiMachine& m, int state, int32_t roll) {
    if (state < 0 || state > 3) return state;
    const auto& st = m.state[state];
    // `cmp w23, #1 / b.lt` -- with no weight there is no roll and no change.
    // This early-out is REDUNDANT for any roll >= 0, and deliberately kept: with
    // all weights zero the chain below subtracts nothing and ends at
    // `roll < weight[3]`, i.e. `roll < 0`, which is false, so it already returns
    // the state unchanged. Deleting this line was tried as a sabotage and the
    // self-test could not tell -- so it is documentation of the engine's own
    // `b.lt`, plus a guard against a negative roll, and NOT something the test
    // covers. Saying so beats leaving a line that looks tested and is not.
    if (st.terminal()) return state;
    // The engine's subtract-chain, in its order. Note the LAST arm is a `<`
    // test that leaves the state unchanged when it fails, rather than a
    // fourth subtract -- so a roll can legitimately produce no transition.
    int32_t r = roll;
    if ((r -= st.weight[0]) < 0) return 0;
    if ((r -= st.weight[1]) < 0) return 1;
    if ((r -= st.weight[2]) < 0) return 2;
    return r < st.weight[3] ? 3 : state;
}

bool Font::Load(const std::vector<uint8_t>& file) {
    // SetBinary reads: +0x0C/+0x10 texture w/h, +0x14 pixel offset, +0x18/+0x1C
    // count/offset of the codepoint map, +0x20/+0x24 count/offset of the glyph
    // records (10 bytes each, from `x*5 << 1`).
    if (file.size() < 0x30) return false;
    w_ = RdU32(file, 0x0C);
    h_ = RdU32(file, 0x10);
    uint32_t tex = RdU32(file, 0x14);
    uint32_t map_n = RdU32(file, 0x18), map_off = RdU32(file, 0x1C);
    uint32_t gly_n = RdU32(file, 0x20), gly_off = RdU32(file, 0x24);
    if (!w_ || !h_ || !gly_n) return false;
    // The three sections tile the file exactly in the shipping font; anything
    // else means this is not the layout SetBinary expects.
    if (size_t(map_off) + size_t(map_n) * 2 != gly_off) return false;
    if (size_t(gly_off) + size_t(gly_n) * 10 != tex) return false;
    if (size_t(tex) + size_t(w_) * h_ != file.size()) return false;

    atlas_.assign(file.begin() + tex, file.end());
    glyphs_.resize(gly_n);
    for (uint32_t i = 0; i < gly_n; ++i) {
        const uint8_t* r = file.data() + gly_off + size_t(i) * 10;
        Glyph g;
        std::memcpy(&g.x, r, 2);
        std::memcpy(&g.y, r + 2, 2);
        g.w = r[4];
        g.h = r[5];
        // +6 is not read by DrawString and stays undecoded.
        g.left = int8_t(r[7]);
        g.right = int8_t(r[8]);
        g.top = int8_t(r[9]);
        glyphs_[i] = g;
    }
    map_.resize(map_n);
    for (uint32_t i = 0; i < map_n; ++i)
        std::memcpy(&map_[i], file.data() + map_off + size_t(i) * 2, 2);
    return true;
}

const Glyph* Font::Find(uint32_t cp) const {
    if (cp >= map_.size()) return nullptr;
    uint16_t gi = map_[cp];
    if (gi == 0xFFFF || gi >= glyphs_.size()) return nullptr;
    return &glyphs_[gi];
}

int Font::Measure(const std::string& s) const {
    int w = 0, best = 0;
    for (unsigned char c : s) {
        if (c == '\n') { best = std::max(best, w); w = 0; continue; }
        if (const Glyph* g = Find(c)) w += g->Advance();
    }
    return std::max(best, w);
}

bool StringTable::Load(const std::vector<uint8_t>& file) {
    by_id_.clear();
    if (file.size() < 16) return false;
    uint32_t version = RdU32(file, 0);
    uint32_t count = RdU32(file, 4);
    uint32_t text_off = RdU32(file, 12);
    if (version != 1 || count == 0) return false;
    size_t ids_off = 16 + size_t(count) * 4;
    // The id block is `count` fixed 48-byte records; anything else means the
    // layout is not what this code expects, so refuse rather than half-load.
    if (text_off <= ids_off || text_off > file.size()) return false;
    if (text_off - ids_off != size_t(count) * 48) return false;

    std::vector<std::string> texts;
    texts.reserve(count);
    size_t p = text_off;
    while (p < file.size() && texts.size() < count) {
        size_t e = p;
        while (e < file.size() && file[e]) ++e;
        texts.emplace_back(reinterpret_cast<const char*>(file.data()) + p, e - p);
        p = e + 1;
    }
    if (texts.size() != count) return false;

    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* rec = file.data() + ids_off + size_t(i) * 48;
        size_t n = 0;
        while (n < 48 && rec[n]) ++n;
        by_id_.emplace(std::string(reinterpret_cast<const char*>(rec), n),
                       std::move(texts[i]));
    }
    return true;
}

const std::string* StringTable::Find(const std::string& id) const {
    auto it = by_id_.find(id);
    return it == by_id_.end() ? nullptr : &it->second;
}

int32_t ComputeDamage(const DamageInput& in) {
    // `add w8, w27, #3; cmp w27, #0; csel w8, w8, w27, lt; asr w27, w8, #2` --
    // a round-toward-zero divide by 4, applied only on a weakness.
    int32_t def = in.weak ? ((in.defence < 0 ? in.defence + 3 : in.defence) >> 2)
                          : in.defence;
    int64_t base = int64_t(in.attack) - def + in.magic;
    base = base * (int64_t(in.gauge) + 16000) / 16000;
    int64_t dmg = base + base * in.roll / 100;
    return int32_t(dmg > 1 ? dmg : 1);
}

int32_t PlayerStats::attack() const {
    int32_t a = Cap(power);
    // tblWeapon stores an attack RANGE and which end feeds the player's attack
    // power is not a guess: DataTableGetWeapon returns the record itself (its
    // first word is the id it matched on) and Update reads `[x0, #4]`, the
    // second word -- the LOW end. The port used the high end before, which was
    // an assumption and was wrong.
    if (const WeaponStats* w = FindWeapon(weapon)) a += w->atk_lo;
    return a;
}

int32_t PlayerStats::defence() const {
    // Exactly what Update adds: stamina + 1, then the helm and the armour --
    // and NOT the shield, whose id sits in the very next slot and whose table
    // (tblShield, ids 401..409) DataTableGetDefence resolves perfectly well.
    // Whether the shield is applied elsewhere or simply never counted is not
    // established, so the port matches the engine rather than "fixing" it.
    return Cap(stamina) + 1 + EquipDefence(helm) + EquipDefence(armor);
}

// tblLevelup, 64 bytes = four 16-byte rows (the symbol's own size; the stride is
// DataTableGetLevelUp's `lsl #4` @ 0x2c375c). Verbatim from .data.
static const int32_t kLevelUp[4][4] = {
    {1, 2, 0, 1},   // Warrior
    {2, 1, 0, 1},   // Monk
    {1, 0, 2, 1},   // Mage
    {1, 0, 1, 2},   // Sage
};

const char* PlayerStats::RegimenName(int r) {
    switch (r) {
        case kWarrior: return "Warrior";
        case kMonk:    return "Monk";
        case kMage:    return "Mage";
        case kSage:    return "Sage";
    }
    return "?";
}

void PlayerStats::LevelUp(int regimen) {
    if (regimen < 0 || regimen > 3) {
        lucent::warn("player", "level-up regimen {} is outside 0..3; ignored", regimen);
        return;
    }
    if (level >= 99) {
        lucent::info("player", "already at level 99");
        return;
    }
    ++level;
    const int32_t* r = kLevelUp[regimen];
    // Process_LevelUp does NOT add the row as stored. It runs it through
    // `rev64 v1.4s, v0.4s` + `mov v1.d[1], v0.d[1]`, which swaps the first two
    // lanes and leaves the last two -- so the row is written in a different
    // order from the stat array it is added to.
    power   += r[1];
    stamina += r[0];
    wisdom  += r[2];
    will    += r[3];
    // Update recomputes the maxima, then Process_LevelUp copies them into the
    // current values: a level-up is also a full heal.
    hp = max_hp();
    mp = max_mp();
}

// --- Inventory -------------------------------------------------------------
// DataTableGetIdType @ 0x2c387c is six `sub`/`cmp`/`b.hs` pairs in a row, so
// the ranges are transcribed exactly rather than derived from table sizes --
// the weapon table's ids are 101..117 and 121 while this range is 101..118, and
// both happen to have 18 members, which is precisely the coincidence that let
// an earlier count-based check pass while being wrong.
int Inventory::IdType(int32_t id) {
    if (id >= 1   && id <= 37)  return 1;   // items, cmp #0x25 = 37
    if (id >= 101 && id <= 118) return 2;   // weapons
    if (id >= 201 && id <= 206) return 4;   // helms
    if (id >= 301 && id <= 309) return 5;   // armour
    if (id >= 401 && id <= 409) return 6;   // accessories
    if (id >= 501 && id <= 508) return 7;   // magic
    return 0;
}

int Inventory::BagOf(int32_t id) {
    switch (IdType(id)) {
        case 1:  return kItems;
        case 2:  return kWeapons;
        case 4: case 5: case 6: return kArmour;   // one shared bag
        case 7:  return kMagic;
        default: return -1;
    }
}

Inventory::Slot* Inventory::bag(int b, int* n) {
    switch (b) {
        case kItems:   *n = kSlots;      return items;
        case kWeapons: *n = kSlots;      return weapons;
        case kArmour:  *n = kSlots;      return armour;
        case kMagic:   *n = kMagicSlots; return magic;
        default:       *n = 0;           return nullptr;
    }
}

const Inventory::Slot* Inventory::bag(int b, int* n) const {
    return const_cast<Inventory*>(this)->bag(b, n);
}

int32_t Inventory::Count(int32_t id) const {
    int b = BagOf(id);
    if (b < 0) return 0;
    // Magic is not searched: IsHaveItem addresses it straight off the id with
    // `add x8, x19, w20, sxtw #3` / `sub x8, x8, #0xc80`, which for id 501
    // lands on GameParameter+0x328 -- the slot index is id - 501.
    if (b == kMagic) return magic[id - kMagicFirstId].id ? 1 : 0;
    int n = 0;
    const Slot* s = bag(b, &n);
    int32_t held = 0;
    for (int i = 0; i < n; ++i)
        if (s[i].id == id) ++held;
    return held;
}

bool Inventory::Add(int32_t id, bool commit) {
    int b = BagOf(id);
    if (b < 0) {
        lucent::warn("inv", "id {} is in no table, so it has no bag", id);
        return false;
    }
    if (b == kMagic) {
        Slot& m = magic[id - kMagicFirstId];
        if (m.id) return false;
        if (commit) { m.id = id; m.seq = seq_counter++; }
        return true;
    }
    int n = 0;
    Slot* s = bag(b, &n);
    for (int i = 0; i < n; ++i) {
        if (s[i].id) continue;              // first FREE slot; nothing stacks
        if (commit) {
            s[i].id = id;
            s[i].seq = seq_counter++;
            // `kind` is deliberately left 0. The engine writes
            // DataTableGetItem(id)+0x4 here (zeroed when it is 1), but that
            // field's MEANING is still open -- docs/re-frontier.md -- and its
            // only consumer is the item-list UI the port does not have.
            // Filling it with a number nobody can interpret would look like
            // knowledge the project does not have.
        }
        return true;
    }
    return false;                            // bag full
}

bool Inventory::Del(int32_t id) {
    int b = BagOf(id);
    if (b < 0) return false;
    if (b == kMagic) {
        Slot& m = magic[id - kMagicFirstId];
        if (!m.id) return false;
        m = Slot{};
        return true;
    }
    int n = 0;
    Slot* s = bag(b, &n);
    for (int i = 0; i < n; ++i) {
        if (s[i].id != id) continue;
        s[i] = Slot{};
        return true;
    }
    return false;
}

void Inventory::NewGame() {
    *this = Inventory{};
    for (int32_t id : {kStartingWeaponId, kStartingHelmId,
                       kStartingArmorId, kStartingAccessoryId})
        if (!Add(id))
            lucent::warn("inv", "new game could not grant id {}", id);
}

RoomSize FindRoomSize(const Archive& ar, const std::string& room) {
    RoomSize rs;
    // Best source: the room's own ground-attribute grid. Load_GroundAttribute @
    // 0x2e6cec computes the grid from the room size (`ceil(size / 30)` chips,
    // four cells per chip, cell = the literal 7.5) and REFUSES the file unless
    // its header matches what it computed -- so the header is the size.
    auto gp = std::format("sk1/{}.gdt", room);
    if (ar.Has(gp)) {
        auto b = ar.Read(gp);
        if (b.size() >= 0x14) {
            std::span<const uint8_t> s(b);
            uint32_t ver = RdU32(s, 0), cols = RdU32(s, 4), rows = RdU32(s, 8);
            float cw, ch;
            std::memcpy(&cw, b.data() + 0x0C, 4);
            std::memcpy(&ch, b.data() + 0x10, 4);
            if (ver == 1 && cw == 7.5f && ch == 7.5f && cols && rows) {
                rs.w = float(cols) * cw;
                rs.h = float(rows) * ch;
                rs.source = RoomSize::kGdt;
                return rs;
            }
        }
    }
    // No .gdt. No attested source for this room's size has been found, so the
    // fallback is an inference and is scored as one. Both known sizes put the
    // room's low corner at size*grid_index; where the room's collision AABB
    // agrees with exactly one of them, that one is taken. A grid index of 0
    // decides nothing (both give 0), and neither does a room whose geometry
    // does not reach its own corner, so those keep the 300x240 default.
    auto sp = std::format("sk1/{}.scol", room);
    if (ar.Has(sp)) {
        try {
            Collision c = ParseScol(ar.Read(sp));
            int gx = 0, gy = 0;
            auto us = room.rfind('_'), us2 = room.rfind('_', us - 1);
            if (us != std::string::npos && us2 != std::string::npos) {
                gx = std::atoi(room.substr(us2 + 1, us - us2 - 1).c_str());
                gy = std::atoi(room.substr(us + 1).c_str());
            }
            if (gx > 0 && std::fabs(c.aabb_lo[0] - 330.f * float(gx)) < 1.f) rs.w = 330.f;
            if (gy > 0 && std::fabs(c.aabb_lo[2] - 270.f * float(gy)) < 1.f) rs.h = 270.f;
            rs.source = RoomSize::kAabb;
        } catch (const std::exception&) {
        }
    }
    return rs;
}

// UTF8_OctBytes @ 0x3db5f0: how many bytes the lead byte claims. The engine
// copies plain text a whole code point at a time, so a multi-byte character can
// never be split across the '@' scan.
static int Utf8OctBytes(unsigned char c) {
    if (c < 0x80) return 1;
    if (c < 0xc0) return 1;  // stray continuation byte: consume it, don't stall
    if (c < 0xe0) return 2;
    if (c < 0xf0) return 3;
    return 4;
}

// The '@N' argument. The engine calls GetIntFromString, a general
// operator-precedence expression evaluator, but every one of the 577 @N
// occurrences in str_en.bin and str_ja.bin is a parenthesised literal, so this
// parses that form and refuses anything else rather than half-evaluating it.
// Returns the number of source bytes consumed, or 0 on no parse.
static size_t ParseIntArg(const std::string& s, size_t i, int* out) {
    size_t j = i;
    while (j < s.size() && (s[j] == ' ' || s[j] == '\t')) ++j;
    bool paren = j < s.size() && s[j] == '(';
    if (paren) ++j;
    size_t digits = j;
    int v = 0;
    while (j < s.size() && s[j] >= '0' && s[j] <= '9') { v = v * 10 + (s[j] - '0'); ++j; }
    if (j == digits) return 0;
    if (paren) {
        if (j >= s.size() || s[j] != ')') return 0;
        ++j;
    }
    *out = v;
    return j - i;
}

std::string CnvFormatString(const std::string& src, const StringTable* tbl,
                            const FormatParams& prm) {
    std::string out;
    out.reserve(src.size());
    auto append = [&](const std::string* t, const char* what) {
        if (t) { out += *t; return; }
        // The engine logs "NoStringID:[%s]" and then memcpys from the null it
        // just logged. Here the code is left visible instead, so a gap in the
        // port reads as a gap and not as a line the game never wrote.
        lucent::warn("text", "control code {} has no value", what);
        out += what;
    };
    for (size_t i = 0; i < src.size();) {
        if (src[i] != '@') {
            int n = Utf8OctBytes((unsigned char)src[i]);
            out.append(src, i, size_t(n));
            i += size_t(n);
            continue;
        }
        if (i + 1 >= src.size()) break;  // trailing '@': the engine drops it
        char c = src[i + 1];
        switch (c) {
        case '@': out += '@'; i += 2; continue;
        case 'H': case 'h': out += prm.hero; i += 2; continue;
        case 'G': case 'g': out += prm.girl; i += 2; continue;
        case 'P': append(&prm.prm[0], "@P"); i += 2; continue;
        case 'i': append(&prm.prm[1], "@i"); i += 2; continue;
        case 'I': append(&prm.prm[2], "@I"); i += 2; continue;
        case 'S': append(&prm.prm[3], "@S"); i += 2; continue;
        case 'N': case 'n': {
            int id = 0;
            size_t used = ParseIntArg(src, i + 2, &id);
            if (!used) {  // not the literal form; leave it visible
                lucent::warn("text", "@{} argument is not a literal: \"{}\"",
                             c, src.substr(i, 12));
                ++i;
                continue;
            }
            auto key = std::format("CHARACTER_NAME_{}", id);
            const std::string* t = tbl ? tbl->Find(key) : nullptr;
            if (t) out += *t;
            else { lucent::warn("text", "{} is not in the table", key); out += key; }
            i += 2 + used;
            continue;
        }
        default: break;
        }
        if (c >= '0' && c <= '9') {
            size_t j = i + 1;
            int n = 0;
            while (j < src.size() && src[j] >= '0' && src[j] <= '9')
                { n = n * 10 + (src[j] - '0'); ++j; }
            if (size_t(n) < prm.args.size()) out += prm.args[size_t(n)];
            else {
                lucent::warn("text", "@{} but only {} arguments were supplied",
                             n, prm.args.size());
                out.append(src, i, j - i);
            }
            i = j;
            continue;
        }
        // Unknown code: the engine swallows the '@' and reprocesses the letter.
        ++i;
    }
    return out;
}

int32_t EquipDefence(int32_t id) {
    struct Row { int32_t id, def; };
    static const Row kTable[] = {
#define WEAPON(i, lo, hi)
#define DEFENCE(i, d) {i, d},
#include "engine/weapon_table.inc"
#undef DEFENCE
#undef WEAPON
    };
    for (const auto& r : kTable)
        if (r.id == id) return r.def;
    return 0;
}

const WeaponStats* FindWeapon(int32_t id) {
    static const WeaponStats kTable[] = {
#define WEAPON(i, lo, hi) {i, lo, hi},
#define DEFENCE(i, d)
#include "engine/weapon_table.inc"
#undef DEFENCE
#undef WEAPON
    };
    for (const auto& w : kTable)
        if (w.id == id) return &w;
    return nullptr;
}

}  // namespace mcf
