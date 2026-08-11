#!/usr/bin/env python3
"""Parse `.stex` texture containers (magic `SMDI`).

Layout reversed from SiSurfaceTextureArray::SetBinary, whose loop reads 32-byte
descriptors and forwards them to SiSurfaceTexture::Create(w, h, mips, format,
pixels, size, ..., name) -- the 11 call arguments pin every field.

Header:
  0x00 char[4] "SMDI"      0x08 u32 descriptor table offset
  0x04 u32 ?               0x0C u32 texture count

Descriptor (32 bytes):
  0x00 u32 ?               0x10 u32 mip levels
  0x04 u32 format code     0x14 u32 offset to pixel data
  0x08 u32 width           0x18 u32 pixel data size
  0x0C u32 height          0x1C u32 offset to name string

Format code maps through a table to SiTextureFormat: 2->3, 3->2, 4->4, 5->5,
anything else -> 1.
"""
import struct, sys

FMT_MAP = {2: 3, 3: 2, 4: 4, 5: 5}


def mip_texel_count(w, h, mips):
    return sum(max(1, w >> i) * max(1, h >> i) for i in range(mips))


def parse(buf):
    if buf[:4] != b'SMDI':
        raise ValueError("not a SMDI texture container (magic %r)" % buf[:4])
    tbl, count = struct.unpack_from('<II', buf, 8)
    out = []
    for i in range(count):
        d = tbl + i * 32
        _, fmtcode, w, h, mips, data_off, data_sz, name_off = \
            struct.unpack_from('<IIIIIIII', buf, d)
        if data_off + data_sz > len(buf):
            raise ValueError("entry %d: pixel data %d..%d exceeds file (%d)"
                             % (i, data_off, data_off + data_sz, len(buf)))
        name = buf[name_off:buf.index(b'\0', name_off)].decode('ascii', 'replace')
        out.append(dict(index=i, name=name, w=w, h=h, mips=mips,
                        fmtcode=fmtcode, fmt=FMT_MAP.get(fmtcode, 1),
                        data_off=data_off, data_size=data_sz,
                        texels=mip_texel_count(w, h, mips)))
    return out


if __name__ == '__main__':
    import glob, collections
    files = sys.argv[1:]
    if not files:
        sys.exit("usage: stex.py <file.stex ...>")
    bpp = collections.defaultdict(collections.Counter)
    ok = bad = 0
    errs = []
    for p in files:
        try:
            for t in parse(open(p, 'rb').read()):
                r = t['data_size'] / t['texels'] if t['texels'] else 0
                bpp[t['fmtcode']][round(r, 4)] += 1
                ok += 1
        except Exception as ex:
            bad += 1
            errs.append("%s: %s" % (p, ex))
    print("parsed %d descriptors across %d files; %d files FAILED"
          % (ok, len(files) - bad, bad))
    for e in errs[:10]:
        print("  FAIL", e)
    print("\nbytes-per-texel by format code (exact integers => layout is right):")
    for code in sorted(bpp):
        dist = ", ".join("%s x%d" % (v, n) for v, n in bpp[code].most_common(6))
        print("  fmtcode %d (SiTextureFormat %s): %s"
              % (code, FMT_MAP.get(code, 1), dist))
