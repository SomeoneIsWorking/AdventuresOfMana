"""LHA static-Huffman (-lh*-) decoder, as used by libmcfandroid's uncompress().

Identified from uncompress_int()'s allocation fingerprint: NC=510, NT=19,
left/right as u16[2*NC-1], c_table[4096]/pt_table[256], and five ring buffers
of 2^11..2^15 bytes. See docs/mpk-format.md.
"""
UCHAR_MAX, THRESHOLD, NC, NT, CBIT, TBIT = 255, 3, 510, 19, 9, 5


class BitReader:
    def __init__(self, data):
        self.d, self.p = data, 0
        self.bitbuf = self.subbitbuf = self.bitcount = 0
        self.fillbuf(16)

    def _getc(self):
        if self.p < len(self.d):
            c = self.d[self.p]; self.p += 1; return c
        return 0

    def fillbuf(self, n):
        self.bitbuf = (self.bitbuf << n) & 0xFFFF
        while n > self.bitcount:
            n -= self.bitcount
            self.bitbuf |= (self.subbitbuf << n) & 0xFFFF
            self.subbitbuf = self._getc()
            self.bitcount = 8
        self.bitcount -= n
        self.bitbuf |= self.subbitbuf >> self.bitcount

    def getbits(self, n):
        if n == 0:
            return 0
        x = self.bitbuf >> (16 - n)
        self.fillbuf(n)
        return x


class Decoder:
    def __init__(self, data, dicbit):
        self.br = BitReader(data)
        self.dicbit = dicbit
        self.dicsiz = 1 << dicbit
        self.np = dicbit + 1
        self.pbit = 4 if dicbit <= 13 else 5
        self.npt = max(NT, self.np)
        self.c_len = bytearray(NC)
        self.pt_len = bytearray(self.npt)
        self.c_table = [0] * 4096
        self.pt_table = [0] * 256
        self.left = [0] * (2 * NC - 1)
        self.right = [0] * (2 * NC - 1)
        self.blocksize = 0

    # --- canonical LHA make_table -------------------------------------------
    def make_table(self, nchar, bitlen, tablebits, table):
        count = [0] * 17
        weight = [0] * 17
        start = [0] * 18
        for i in range(nchar):
            count[bitlen[i]] += 1
        for i in range(1, 17):
            start[i + 1] = start[i] + (count[i] << (16 - i))
        if start[17] != 0x10000:
            raise ValueError("bad Huffman table (start[17]=%#x)" % start[17])
        jutbits = 16 - tablebits
        for i in range(1, tablebits + 1):
            start[i] >>= jutbits
            weight[i] = 1 << (tablebits - i)
        for i in range(tablebits + 1, 17):
            weight[i] = 1 << (16 - i)
        i = start[tablebits + 1] >> jutbits
        if i != 0:
            for k in range(i, 1 << tablebits):
                table[k] = 0
        avail = nchar
        mask = 1 << (15 - tablebits)
        for ch in range(nchar):
            ln = bitlen[ch]
            if ln == 0:
                continue
            nextcode = start[ln] + weight[ln]
            if ln <= tablebits:
                for i in range(start[ln], min(nextcode, 1 << tablebits)):
                    table[i] = ch
            else:
                k = start[ln]
                idx = k >> jutbits
                pt, pi = table, idx          # (container, index) cursor
                for _ in range(ln - tablebits):
                    if pt[pi] == 0:
                        self.right[avail] = self.left[avail] = 0
                        pt[pi] = avail
                        avail += 1
                    pt, pi = ((self.right, pt[pi]) if (k & mask)
                              else (self.left, pt[pi]))
                    k = (k << 1) & 0xFFFF
                pt[pi] = ch
            start[ln] = nextcode

    def read_pt_len(self, nn, nbit, i_special):
        br = self.br
        n = br.getbits(nbit)
        if n == 0:
            c = br.getbits(nbit)
            for i in range(nn):
                self.pt_len[i] = 0
            for i in range(256):
                self.pt_table[i] = c
            return
        i = 0
        while i < n:
            c = br.bitbuf >> 13
            if c == 7:
                mask = 1 << 12
                while mask & br.bitbuf:
                    mask >>= 1
                    c += 1
            br.fillbuf(3 if c < 7 else c - 3)
            self.pt_len[i] = c
            i += 1
            if i == i_special:
                c = br.getbits(2)
                while c > 0 and i < nn:
                    self.pt_len[i] = 0
                    i += 1
                    c -= 1
        while i < nn:
            self.pt_len[i] = 0
            i += 1
        self.make_table(nn, self.pt_len, 8, self.pt_table)

    def read_c_len(self):
        br = self.br
        n = br.getbits(CBIT)
        if n == 0:
            c = br.getbits(CBIT)
            for i in range(NC):
                self.c_len[i] = 0
            for i in range(4096):
                self.c_table[i] = c
            return
        i = 0
        while i < n:
            c = self.pt_table[br.bitbuf >> 8]
            if c >= NT:
                mask = 1 << 7
                while True:
                    c = self.right[c] if (br.bitbuf & mask) else self.left[c]
                    mask >>= 1
                    if c < NT:
                        break
            br.fillbuf(self.pt_len[c])
            if c <= 2:
                if c == 0:
                    c = 1
                elif c == 1:
                    c = br.getbits(4) + 3
                else:
                    c = br.getbits(CBIT) + 20
                while c > 0:
                    self.c_len[i] = 0
                    i += 1
                    c -= 1
            else:
                self.c_len[i] = c - 2
                i += 1
        while i < NC:
            self.c_len[i] = 0
            i += 1
        self.make_table(NC, self.c_len, 12, self.c_table)

    def decode_c(self):
        br = self.br
        if self.blocksize == 0:
            self.blocksize = br.getbits(16)
            self.read_pt_len(NT, TBIT, 3)
            self.read_c_len()
            self.read_pt_len(self.np, self.pbit, -1)
        self.blocksize -= 1
        j = self.c_table[br.bitbuf >> 4]
        if j >= NC:
            mask = 1 << 3
            while True:
                j = self.right[j] if (br.bitbuf & mask) else self.left[j]
                mask >>= 1
                if j < NC:
                    break
        br.fillbuf(self.c_len[j])
        return j

    def decode_p(self):
        br = self.br
        j = self.pt_table[br.bitbuf >> 8]
        if j >= self.np:
            mask = 1 << 7
            while True:
                j = self.right[j] if (br.bitbuf & mask) else self.left[j]
                mask >>= 1
                if j < self.np:
                    break
        br.fillbuf(self.pt_len[j])
        if j != 0:
            j = (1 << (j - 1)) + br.getbits(j - 1)
        return j

    def decode(self, outsize):
        out = bytearray(outsize)
        buf = bytearray(self.dicsiz)
        r = 0
        n = 0
        while n < outsize:
            c = self.decode_c()
            if c <= UCHAR_MAX:
                buf[r] = c
                out[n] = c
                n += 1
                r = (r + 1) & (self.dicsiz - 1)
            else:
                j = c - (UCHAR_MAX + 1 - THRESHOLD)
                i = (r - self.decode_p() - 1) & (self.dicsiz - 1)
                for _ in range(j):
                    if n >= outsize:
                        break
                    b = buf[i]
                    buf[r] = b
                    out[n] = b
                    n += 1
                    i = (i + 1) & (self.dicsiz - 1)
                    r = (r + 1) & (self.dicsiz - 1)
        return bytes(out)


def decompress(data, outsize, dicbit):
    return Decoder(data, dicbit).decode(outsize)
