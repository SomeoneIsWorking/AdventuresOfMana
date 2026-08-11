// LHA static-Huffman (-lh5-) decoder.
//
// Identified from uncompress_int()'s allocation fingerprint in libmcfandroid.so
// (NC=510, NT=19, u16[2*NC-1] tree arrays, c_table[4096]/pt_table[256], and five
// ring buffers of 2^11..2^15). See docs/mpk-format.md. Mirrors tools/asset/lha.py.
#include "mcf/mcf.h"

#include <array>
#include <cstring>

namespace mcf::lha {
namespace {

constexpr int kUCharMax = 255;
constexpr int kThreshold = 3;
constexpr int kNC = 510;
constexpr int kNT = 19;
constexpr int kCBit = 9;
constexpr int kTBit = 5;

class BitReader {
public:
    explicit BitReader(std::span<const uint8_t> d) : d_(d) { Fill(16); }

    void Fill(int n) {
        bitbuf_ = uint16_t(bitbuf_ << n);
        while (n > bitcount_) {
            n -= bitcount_;
            bitbuf_ |= uint16_t(subbitbuf_ << n);
            subbitbuf_ = p_ < d_.size() ? d_[p_++] : 0;
            bitcount_ = 8;
        }
        bitcount_ -= n;
        bitbuf_ |= uint16_t(subbitbuf_ >> bitcount_);
    }

    uint32_t Get(int n) {
        if (n == 0) return 0;
        uint32_t x = uint32_t(bitbuf_) >> (16 - n);
        Fill(n);
        return x;
    }

    uint16_t buf() const { return bitbuf_; }
    size_t consumed() const { return p_; }

private:
    std::span<const uint8_t> d_;
    size_t p_ = 0;
    uint16_t bitbuf_ = 0;
    uint8_t subbitbuf_ = 0;
    int bitcount_ = 0;
};

class Decoder {
public:
    Decoder(std::span<const uint8_t> in, int dicbit)
        : br_(in), dicsiz_(1u << dicbit), np_(dicbit + 1),
          pbit_(dicbit <= 13 ? 4 : 5), npt_(std::max(kNT, np_)) {
        pt_len_.resize(npt_);
    }

    void Run(std::span<uint8_t> out) {
        std::vector<uint8_t> ring(dicsiz_);
        uint32_t r = 0;
        size_t n = 0;
        while (n < out.size()) {
            int c = DecodeC();
            if (c <= kUCharMax) {
                ring[r] = uint8_t(c);
                out[n++] = uint8_t(c);
                r = (r + 1) & (dicsiz_ - 1);
            } else {
                int len = c - (kUCharMax + 1 - kThreshold);
                uint32_t i = (r - DecodeP() - 1) & (dicsiz_ - 1);
                for (int k = 0; k < len && n < out.size(); ++k) {
                    uint8_t b = ring[i];
                    ring[r] = b;
                    out[n++] = b;
                    i = (i + 1) & (dicsiz_ - 1);
                    r = (r + 1) & (dicsiz_ - 1);
                }
            }
        }
    }

    size_t consumed() const { return br_.consumed(); }

private:
    // Canonical LHA make_table. `table` is the 2^tablebits fast lookup; codes
    // longer than tablebits spill into the left_/right_ binary tree.
    void MakeTable(int nchar, const std::vector<uint8_t>& bitlen, int tablebits,
                   std::vector<uint16_t>& table) {
        std::array<int, 17> count{}, weight{};
        std::array<int, 18> start{};
        for (int i = 0; i < nchar; ++i) count[bitlen[i]]++;
        for (int i = 1; i < 17; ++i) start[i + 1] = start[i] + (count[i] << (16 - i));
        if (start[17] != 0x10000) throw Error("LHA: malformed Huffman table");

        int jutbits = 16 - tablebits;
        for (int i = 1; i <= tablebits; ++i) {
            start[i] >>= jutbits;
            weight[i] = 1 << (tablebits - i);
        }
        for (int i = tablebits + 1; i < 17; ++i) weight[i] = 1 << (16 - i);

        int i = start[tablebits + 1] >> jutbits;
        if (i != 0)
            for (int k = i; k < (1 << tablebits); ++k) table[k] = 0;

        int avail = nchar;
        int mask = 1 << (15 - tablebits);
        for (int ch = 0; ch < nchar; ++ch) {
            int ln = bitlen[ch];
            if (ln == 0) continue;
            int nextcode = start[ln] + weight[ln];
            if (ln <= tablebits) {
                for (int k = start[ln]; k < std::min(nextcode, 1 << tablebits); ++k)
                    table[k] = uint16_t(ch);
            } else {
                int k = start[ln];
                uint16_t* p = &table[k >> jutbits];
                for (int b = 0; b < ln - tablebits; ++b) {
                    if (*p == 0) {
                        right_[avail] = left_[avail] = 0;
                        *p = uint16_t(avail++);
                    }
                    p = (k & mask) ? &right_[*p] : &left_[*p];
                    k = uint16_t(k << 1);
                }
                *p = uint16_t(ch);
            }
            start[ln] = nextcode;
        }
    }

    void ReadPtLen(int nn, int nbit, int i_special) {
        int n = int(br_.Get(nbit));
        if (n == 0) {
            uint16_t c = uint16_t(br_.Get(nbit));
            std::fill(pt_len_.begin(), pt_len_.begin() + nn, uint8_t(0));
            std::fill(pt_table_.begin(), pt_table_.end(), c);
            return;
        }
        int i = 0;
        while (i < n) {
            int c = br_.buf() >> 13;
            if (c == 7) {
                int m = 1 << 12;
                while (m & br_.buf()) { m >>= 1; ++c; }
            }
            br_.Fill(c < 7 ? 3 : c - 3);
            pt_len_[i++] = uint8_t(c);
            if (i == i_special) {
                int z = int(br_.Get(2));
                while (z-- > 0 && i < nn) pt_len_[i++] = 0;
            }
        }
        while (i < nn) pt_len_[i++] = 0;
        MakeTable(nn, pt_len_, 8, pt_table_);
    }

    void ReadCLen() {
        int n = int(br_.Get(kCBit));
        if (n == 0) {
            uint16_t c = uint16_t(br_.Get(kCBit));
            std::fill(c_len_.begin(), c_len_.end(), uint8_t(0));
            std::fill(c_table_.begin(), c_table_.end(), c);
            return;
        }
        int i = 0;
        while (i < n) {
            int c = pt_table_[br_.buf() >> 8];
            if (c >= kNT) {
                int m = 1 << 7;
                do {
                    c = (br_.buf() & m) ? right_[c] : left_[c];
                    m >>= 1;
                } while (c >= kNT);
            }
            br_.Fill(pt_len_[c]);
            if (c <= 2) {
                if (c == 0) c = 1;
                else if (c == 1) c = int(br_.Get(4)) + 3;
                else c = int(br_.Get(kCBit)) + 20;
                while (c-- > 0) c_len_[i++] = 0;
            } else {
                c_len_[i++] = uint8_t(c - 2);
            }
        }
        while (i < kNC) c_len_[i++] = 0;
        MakeTable(kNC, c_len_, 12, c_table_);
    }

    int DecodeC() {
        if (blocksize_ == 0) {
            blocksize_ = int(br_.Get(16));
            ReadPtLen(kNT, kTBit, 3);
            ReadCLen();
            ReadPtLen(np_, pbit_, -1);
        }
        --blocksize_;
        int j = c_table_[br_.buf() >> 4];
        if (j >= kNC) {
            int m = 1 << 3;
            do {
                j = (br_.buf() & m) ? right_[j] : left_[j];
                m >>= 1;
            } while (j >= kNC);
        }
        br_.Fill(c_len_[j]);
        return j;
    }

    uint32_t DecodeP() {
        int j = pt_table_[br_.buf() >> 8];
        if (j >= np_) {
            int m = 1 << 7;
            do {
                j = (br_.buf() & m) ? right_[j] : left_[j];
                m >>= 1;
            } while (j >= np_);
        }
        br_.Fill(pt_len_[j]);
        if (j == 0) return 0;
        return (1u << (j - 1)) + br_.Get(j - 1);
    }

    BitReader br_;
    uint32_t dicsiz_;
    int np_, pbit_, npt_;
    int blocksize_ = 0;
    std::vector<uint8_t> c_len_ = std::vector<uint8_t>(kNC);
    std::vector<uint8_t> pt_len_;
    std::vector<uint16_t> c_table_ = std::vector<uint16_t>(4096);
    std::vector<uint16_t> pt_table_ = std::vector<uint16_t>(256);
    std::vector<uint16_t> left_ = std::vector<uint16_t>(2 * kNC - 1);
    std::vector<uint16_t> right_ = std::vector<uint16_t>(2 * kNC - 1);
};

}  // namespace

void Decode(std::span<const uint8_t> in, std::span<uint8_t> out, size_t* consumed,
            int dicbit) {
    Decoder d(in, dicbit);
    d.Run(out);
    if (consumed) *consumed = d.consumed();
}

}  // namespace mcf::lha
