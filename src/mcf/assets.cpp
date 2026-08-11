// MPK archive, .stex textures and .smdl models. See docs/mpk-format.md and
// docs/assets.md; mirrors tools/asset/{mpk,stex,smdl}.py.
#include "mcf/mcf.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <format>

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

}  // namespace mcf
