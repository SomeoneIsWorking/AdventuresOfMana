// MPK archive, .stex textures and .smdl models. See docs/mpk-format.md and
// docs/assets.md; mirrors tools/asset/{mpk,stex,smdl}.py.
#include "mcf/mcf.h"

#include <cstdio>
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

}  // namespace mcf
