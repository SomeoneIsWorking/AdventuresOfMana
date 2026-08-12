// PNG decode, for the boot-path art the engine loads by name:
// ModeMakerLogo's `sk1/sqex%s.png`, ModeTitle's title logos, and the UI atlases.
//
// Scope is set by the shipping data, not by the spec: every PNG in sk1.mpk that
// the boot path touches is 8-bit RGBA (colour type 6), non-interlaced. So this
// decodes exactly that and REFUSES anything else by name rather than guessing --
// a silently mishandled colour type would produce a plausible-looking wrong
// image, which is worse than not loading it.
//
// The inflate is written here because the project has no zlib: WritePng emits
// only stored blocks, so nothing in the tree could read a real deflate stream.

#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include <lucent/log.h>

#include "mcf/mcf.h"

namespace mcf {
namespace {

struct BitReader {
    std::span<const uint8_t> d;
    size_t pos = 0;
    uint32_t bit = 0, acc = 0;
    bool bad = false;

    int Bits(int n) {
        while (bit < uint32_t(n)) {
            if (pos >= d.size()) { bad = true; return 0; }
            acc |= uint32_t(d[pos++]) << bit;
            bit += 8;
        }
        int v = int(acc & ((1u << n) - 1));
        acc >>= n;
        bit -= n;
        return v;
    }
    void Align() { acc = 0; bit = 0; }
};

// Canonical Huffman, built from code lengths -- the same shape as the LHA
// decoder in lha.cpp, but deflate's ordering rather than LHA's.
struct Huff {
    std::vector<uint16_t> counts, symbols;
    bool Build(const uint8_t* lens, int n) {
        counts.assign(16, 0);
        for (int i = 0; i < n; ++i) counts[lens[i]]++;
        counts[0] = 0;
        int left = 1;
        for (int i = 1; i < 16; ++i) {
            left <<= 1;
            left -= counts[i];
            if (left < 0) return false;      // over-subscribed
        }
        std::vector<uint16_t> offs(16, 0);
        for (int i = 1; i < 15; ++i) offs[i + 1] = uint16_t(offs[i] + counts[i]);
        symbols.assign(size_t(n), 0);
        for (int i = 0; i < n; ++i)
            if (lens[i]) symbols[offs[lens[i]]++] = uint16_t(i);
        return true;
    }
    int Decode(BitReader& br) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= br.Bits(1);
            if (br.bad) return -1;
            int count = counts[len];
            if (code - first < count) return symbols[size_t(index + code - first)];
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        return -1;
    }
};

const uint16_t kLenBase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,
                               59,67,83,99,115,131,163,195,227,258};
const uint8_t  kLenExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,
                                5,5,5,5,0};
const uint16_t kDistBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,
                                385,513,769,1025,1537,2049,3073,4097,6145,8193,
                                12289,16385,24577};
const uint8_t  kDistExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,
                                 10,11,11,12,12,13,13};

bool Inflate(std::span<const uint8_t> in, std::vector<uint8_t>& out) {
    BitReader br{in};
    Huff fixed_lit, fixed_dist;
    {
        uint8_t l[288];
        for (int i = 0; i < 144; ++i) l[i] = 8;
        for (int i = 144; i < 256; ++i) l[i] = 9;
        for (int i = 256; i < 280; ++i) l[i] = 7;
        for (int i = 280; i < 288; ++i) l[i] = 8;
        fixed_lit.Build(l, 288);
        uint8_t d5[30];
        for (int i = 0; i < 30; ++i) d5[i] = 5;
        fixed_dist.Build(d5, 30);
    }
    for (;;) {
        int last = br.Bits(1);
        int type = br.Bits(2);
        if (br.bad) return false;
        if (type == 0) {                       // stored
            br.Align();
            if (br.pos + 4 > in.size()) return false;
            uint16_t n = uint16_t(in[br.pos] | (in[br.pos + 1] << 8));
            br.pos += 4;
            if (br.pos + n > in.size()) return false;
            out.insert(out.end(), in.begin() + long(br.pos), in.begin() + long(br.pos + n));
            br.pos += n;
        } else if (type == 1 || type == 2) {
            Huff lit, dist;
            if (type == 1) { lit = fixed_lit; dist = fixed_dist; }
            else {
                int hlit = br.Bits(5) + 257, hdist = br.Bits(5) + 1, hclen = br.Bits(4) + 4;
                static const uint8_t ord[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                uint8_t cl[19] = {0};
                for (int i = 0; i < hclen; ++i) cl[ord[i]] = uint8_t(br.Bits(3));
                Huff clh;
                if (!clh.Build(cl, 19)) return false;
                std::vector<uint8_t> lens(size_t(hlit + hdist), 0);
                for (size_t i = 0; i < lens.size();) {
                    int s = clh.Decode(br);
                    if (s < 0) return false;
                    if (s < 16) lens[i++] = uint8_t(s);
                    else if (s == 16) {
                        if (i == 0) return false;
                        uint8_t p = lens[i - 1];
                        for (int r = 3 + br.Bits(2); r && i < lens.size(); --r) lens[i++] = p;
                    } else if (s == 17) {
                        for (int r = 3 + br.Bits(3); r && i < lens.size(); --r) lens[i++] = 0;
                    } else {
                        for (int r = 11 + br.Bits(7); r && i < lens.size(); --r) lens[i++] = 0;
                    }
                }
                if (!lit.Build(lens.data(), hlit)) return false;
                if (!dist.Build(lens.data() + hlit, hdist)) return false;
            }
            for (;;) {
                int s = lit.Decode(br);
                if (s < 0) return false;
                if (s < 256) out.push_back(uint8_t(s));
                else if (s == 256) break;
                else {
                    s -= 257;
                    if (s >= 29) return false;
                    int len = kLenBase[s] + br.Bits(kLenExtra[s]);
                    int ds = dist.Decode(br);
                    if (ds < 0 || ds >= 30) return false;
                    size_t d = size_t(kDistBase[ds] + br.Bits(kDistExtra[ds]));
                    if (d > out.size()) return false;
                    size_t from = out.size() - d;
                    for (int k = 0; k < len; ++k) out.push_back(out[from + size_t(k)]);
                }
                if (br.bad) return false;
            }
        } else return false;
        if (last) break;
    }
    return !br.bad;
}

inline uint32_t Be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
inline uint8_t Paeth(int a, int b, int c) {
    int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return uint8_t(pa <= pb && pa <= pc ? a : (pb <= pc ? b : c));
}

}  // namespace

bool DecodePng(std::span<const uint8_t> file, int* out_w, int* out_h,
               std::vector<uint8_t>* rgba) {
    static const uint8_t kSig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (file.size() < 33 || std::memcmp(file.data(), kSig, 8) != 0) {
        lucent::warn("png", "not a PNG (signature mismatch, {} bytes)", file.size());
        return false;
    }
    uint32_t w = Be32(&file[16]), h = Be32(&file[20]);
    uint8_t depth = file[24], colour = file[25], interlace = file[28];
    // Refuse by name rather than mis-decoding into something plausible.
    if (depth != 8 || colour != 6 || interlace != 0) {
        lucent::warn("png", "unsupported: {}x{} depth {} colour {} interlace {} "
                     "-- this decoder handles 8-bit RGBA non-interlaced only",
                     w, h, depth, colour, interlace);
        return false;
    }
    if (!w || !h || w > 8192 || h > 8192) {
        lucent::warn("png", "implausible dimensions {}x{}", w, h);
        return false;
    }
    std::vector<uint8_t> idat;
    size_t off = 8;
    while (off + 12 <= file.size()) {
        uint32_t len = Be32(&file[off]);
        if (off + 12 + len > file.size()) break;
        const uint8_t* typ = &file[off + 4];
        if (!std::memcmp(typ, "IDAT", 4))
            idat.insert(idat.end(), file.begin() + long(off + 8),
                        file.begin() + long(off + 8 + len));
        else if (!std::memcmp(typ, "IEND", 4)) break;
        off += 12 + len;
    }
    if (idat.size() < 2) {
        lucent::warn("png", "no IDAT data");
        return false;
    }
    std::vector<uint8_t> raw;
    raw.reserve(size_t(h) * (size_t(w) * 4 + 1));
    // Skip the 2-byte zlib header; the Adler-32 trailer is not checked because
    // the inflate already fails on a malformed stream.
    if (!Inflate({idat.data() + 2, idat.size() - 2}, raw)) {
        lucent::warn("png", "inflate failed");
        return false;
    }
    const size_t stride = size_t(w) * 4;
    if (raw.size() < size_t(h) * (stride + 1)) {
        lucent::warn("png", "short image data: {} bytes for {}x{}", raw.size(), w, h);
        return false;
    }
    rgba->assign(size_t(h) * stride, 0);
    for (uint32_t y = 0; y < h; ++y) {
        uint8_t f = raw[size_t(y) * (stride + 1)];
        const uint8_t* src = &raw[size_t(y) * (stride + 1) + 1];
        uint8_t* dst = &(*rgba)[size_t(y) * stride];
        const uint8_t* up = y ? &(*rgba)[size_t(y - 1) * stride] : nullptr;
        for (size_t x = 0; x < stride; ++x) {
            int a = x >= 4 ? dst[x - 4] : 0;
            int b = up ? up[x] : 0;
            int c = (up && x >= 4) ? up[x - 4] : 0;
            int v = src[x];
            switch (f) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += Paeth(a, b, c); break;
                default:
                    lucent::warn("png", "unknown filter {} on row {}", f, y);
                    return false;
            }
            dst[x] = uint8_t(v);
        }
    }
    *out_w = int(w);
    *out_h = int(h);
    return true;
}

}  // namespace mcf
