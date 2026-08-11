#!/usr/bin/env python3
"""Parse `.smdl` models (magic `Smd3`).

Layout reversed from SiModelBase::SetBinary. Its prologue loads s32 values at
0x0C, 0x14, 0x1C ... 0x64 -- a table of (u32 count, s32 offset) section pairs
starting at 0x08 with stride 8. Two of those sections are forwarded to typed
constructors that pin their descriptor shape:

  SiBufferVertex::Create(void const* data, unsigned stride, unsigned count)
  SiBufferIndex::Create (void const* data, unsigned size,   SiDrawIndicesType)

giving a common descriptor {u32 count, s32 data_offset, u32 element_size}.
Index element_size is bytes-per-index and the loader accepts only 2 or 4.

Section slots (k = (offset_field - 0x0C) / 8):
  k=1  skeleton  (count = bone count; cross-checks against .smot header 0x08)
  k=6  vertex buffer
  k=7  index buffer
  k=8  string table (count = length in bytes)

Self-validating: the data regions tile the file with no gaps, so
vertex_off + count*stride == index_off and index_off + count*size == strings_off.
"""
import struct, sys

MAGIC = b'Smd3'
K_BONES, K_VERTEX, K_INDEX, K_STRINGS = 1, 6, 7, 8


def sections(buf):
    return [struct.unpack_from('<Ii', buf, 8 + k * 8) for k in range(12)]


def parse(buf):
    if buf[:4] != MAGIC:
        raise ValueError("not an Smd3 model (magic %r)" % buf[:4])
    sec = sections(buf)
    vcnt, voff = sec[K_VERTEX]
    icnt, ioff = sec[K_INDEX]
    v = dict(zip(('count', 'offset', 'stride'),
                 struct.unpack_from('<IiI', buf, voff)))
    i = dict(zip(('count', 'offset', 'size'),
                 struct.unpack_from('<IiI', buf, ioff)))
    if i['size'] not in (2, 4):
        raise ValueError("index element size %d, loader accepts only 2 or 4" % i['size'])
    vend = v['offset'] + v['count'] * v['stride']
    iend = i['offset'] + i['count'] * i['size']
    if vend != i['offset']:
        raise ValueError("vertex region ends at %d but index data starts at %d"
                         % (vend, i['offset']))
    if iend != sec[K_STRINGS][1]:
        raise ValueError("index region ends at %d but string table starts at %d"
                         % (iend, sec[K_STRINGS][1]))
    return dict(bones=sec[K_BONES][0], vertex=v, index=i,
                strings=(sec[K_STRINGS][1], sec[K_STRINGS][0]))


def positions(buf, m):
    v = m['vertex']
    o, st = v['offset'], v['stride']
    return [struct.unpack_from('<fff', buf, o + n * st) for n in range(v['count'])]


def indices(buf, m):
    i = m['index']
    f = '<%d%s' % (i['count'], 'H' if i['size'] == 2 else 'I')
    return struct.unpack_from(f, buf, i['offset'])


if __name__ == '__main__':
    import glob
    files = sys.argv[1:] or sorted(glob.glob('scratch/dump/sk1/*.smdl'))
    ok = 0
    errs = []
    strides = {}
    oob = 0
    for p in files:
        try:
            b = open(p, 'rb').read()
            m = parse(b)
            idx = indices(b, m)
            if idx and max(idx) >= m['vertex']['count']:
                oob += 1
                raise ValueError("index %d >= vertex count %d"
                                 % (max(idx), m['vertex']['count']))
            strides[m['vertex']['stride']] = strides.get(m['vertex']['stride'], 0) + 1
            ok += 1
        except Exception as ex:
            errs.append("%s: %s" % (p.split('/')[-1], ex))
    print("parsed %d/%d models; %d failed (%d of those from out-of-range indices)"
          % (ok, len(files), len(errs), oob))
    for e in errs[:12]:
        print("  FAIL", e)
    print("vertex strides seen:", dict(sorted(strides.items())))
