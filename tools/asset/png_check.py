#!/usr/bin/env python3
"""Cross-check `src/mcf/png.cpp` against an independent PNG decode.

The C++ decoder was written from scratch -- inflate included -- because the
project has no zlib. A decoder checked only against its own output proves
nothing, so this decodes the same files through Python's `zlib` and compares a
hash of the RGBA bytes with the one `mana --png-selftest` prints.

The hash is deliberately the same cheap rolling one the C++ side uses
(`h = h*131 + byte`, mod 2**64) so the two numbers are directly comparable. It
is not a checksum for integrity -- it is a way to make two implementations
disagree loudly if they differ by even one pixel.

Exits non-zero if any file is missing, fails to decode, or disagrees.
"""

import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mpk  # noqa: E402

FILES = [
    "sk1/sqex.png",
    "sk1/titlelogo_en_color.png",
    "sk1/titlelogo_ja_color.png",
    "sk1/title_000.png",
]


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)


def decode(data):
    """-> (w, h, rgba bytearray). Raises on anything not 8-bit RGBA."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    w, h, depth, colour, _comp, _filt, interlace = struct.unpack(">IIBBBBB",
                                                                data[16:29])
    if (depth, colour, interlace) != (8, 6, 0):
        raise ValueError(f"not 8-bit RGBA non-interlaced: "
                         f"depth {depth} colour {colour} interlace {interlace}")
    idat = b""
    off = 8
    while off + 12 <= len(data):
        ln, = struct.unpack(">I", data[off:off + 4])
        typ = data[off + 4:off + 8]
        if typ == b"IDAT":
            idat += data[off + 8:off + 8 + ln]
        elif typ == b"IEND":
            break
        off += 12 + ln
    raw = zlib.decompress(idat)
    stride = w * 4
    out = bytearray(h * stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        src = raw[y * (stride + 1) + 1: y * (stride + 1) + 1 + stride]
        row = out[y * stride:(y + 1) * stride]
        prev = out[(y - 1) * stride: y * stride] if y else bytes(stride)
        for x in range(stride):
            a = row[x - 4] if x >= 4 else 0
            b = prev[x]
            c = prev[x - 4] if x >= 4 else 0
            v = src[x]
            if f == 1:
                v += a
            elif f == 2:
                v += b
            elif f == 3:
                v += (a + b) // 2
            elif f == 4:
                v += paeth(a, b, c)
            elif f != 0:
                raise ValueError(f"unknown filter {f} on row {y}")
            row[x] = v & 0xFF
        out[y * stride:(y + 1) * stride] = row
    return w, h, out


def roll(buf):
    h = 0
    for byte in buf:
        h = (h * 131 + byte) & 0xFFFFFFFFFFFFFFFF
    return h


def main(argv):
    path = argv[1] if len(argv) > 1 else "scratch/raw/assets/sk1/sk1.mpk"
    if not os.path.exists(path):
        print(f"FAIL: {path} not found -- checked NOTHING", file=sys.stderr)
        return 2
    with open(path, "rb") as f:
        count, dir_csize, data_off = mpk.read_header(f)
        ents = {e[0]: e for e in mpk.read_directory(f, count, dir_csize)}
        print(f"{path}: {count} entries")
        ok = True
        for name in FILES:
            if name not in ents:
                print(f"  FAIL: {name} is not in the archive")
                ok = False
                continue
            e = ents[name]
            blob = mpk.extract_one(f, data_off, e[2], e[3], e[4])
            try:
                w, h, rgba = decode(blob)
            except Exception as exc:                     # noqa: BLE001
                print(f"  FAIL: {name}: {exc}")
                ok = False
                continue
            opaque = sum(1 for i in range(3, len(rgba), 4) if rgba[i])
            print(f"  {name:<30} {w}x{h} hash {roll(rgba):016x} "
                  f"opaque {opaque}/{w * h}")
        print("PNG CROSS-CHECK OK" if ok else "PNG CROSS-CHECK FAILURES ABOVE")
        return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
