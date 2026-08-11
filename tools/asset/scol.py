#!/usr/bin/env python3
"""Parse `.scol` collision meshes (magic `SCol`).

SiCollisionMesh::SetBinary is only 39 instructions -- it copies the file, then
reads a 12-byte-stride array at [0x2C] and pulls two entries out of it, indexed
by [0x10] and [0x14], storing 8 bytes plus a trailing u32 from each. Those
entries decode as float triples, and the two it picks are the AABB min and max:
for M0000_00_00 they are (0, -15, 0) and (300, 150, 240).

Header:
  0x04 u32 header size (20)         0x1C u32 grid width  (80 in every file)
  0x08 u32 node count               0x20 u32 grid height (50 in every file)
  0x0C s32 node array offset        0x24 u32 total file size
  0x10 u32 index of AABB min vec3   0x28 u32 vec3 count
  0x14 u32 index of AABB max vec3   0x2C s32 vec3 pool offset
  0x18 u32 ?

Two independent tilings hold across all 992 files and pin the layout:
  [0x0C] + [0x08]*40 == [0x2C]      node array runs exactly to the vec3 pool
  [0x2C] + [0x28]*12 == filesize    vec3 pool runs exactly to EOF

The 40-byte node record's internals are NOT reversed; SetBinary never touches
them (they are consumed by the collision query code, not the loader).
"""
import struct, sys

MAGIC = b'SCol'
NODE = 40
VEC3 = 12


def parse(buf, strict=True):
    if buf[:4] != MAGIC:
        raise ValueError("not an SCol collision mesh (magic %r)" % buf[:4])
    (hdrsize, nodes, nodes_off, aabb_min_i, aabb_max_i, unk18,
     gw, gh, total, vec3s, vec3_off, _) = struct.unpack_from('<12i', buf, 4)
    if strict:
        if total != len(buf):
            raise ValueError("header size %d != file size %d" % (total, len(buf)))
        if nodes_off + nodes * NODE != vec3_off:
            raise ValueError("node array %d..%d does not abut vec3 pool at %d"
                             % (nodes_off, nodes_off + nodes * NODE, vec3_off))
        if vec3_off + vec3s * VEC3 != len(buf):
            raise ValueError("vec3 pool ends at %d, file is %d"
                             % (vec3_off + vec3s * VEC3, len(buf)))
        for nm, i in (('min', aabb_min_i), ('max', aabb_max_i)):
            if not 0 <= i < vec3s:
                raise ValueError("AABB %s index %d out of range (%d vec3s)"
                                 % (nm, i, vec3s))
    vec = lambda i: struct.unpack_from('<fff', buf, vec3_off + i * VEC3)
    lo, hi = vec(aabb_min_i), vec(aabb_max_i)
    if strict and any(a > b for a, b in zip(lo, hi)):
        raise ValueError("AABB min %s exceeds max %s" % (lo, hi))
    return dict(nodes=nodes, nodes_off=nodes_off, vec3s=vec3s, vec3_off=vec3_off,
                grid=(gw, gh), unk18=unk18, aabb=(lo, hi),
                vec=vec)


if __name__ == '__main__':
    import glob, collections
    files = sys.argv[1:] or sorted(glob.glob('scratch/dump/sk1/*.scol'))
    ok, errs = 0, []
    grids, unk = collections.Counter(), collections.Counter()
    span = [0, 0, 0]
    for p in files:
        try:
            m = parse(open(p, 'rb').read())
            grids[m['grid']] += 1
            unk[m['unk18']] += 1
            lo, hi = m['aabb']
            span = [max(s, h - l) for s, l, h in zip(span, lo, hi)]
            ok += 1
        except Exception as ex:
            errs.append("%s: %s" % (p.split('/')[-1], ex))
    print("parsed %d/%d collision meshes; %d FAILED" % (ok, len(files), len(errs)))
    for e in errs[:12]:
        print("  FAIL", e)
    print("grid dims  :", dict(grids))
    print("[0x18]     :", dict(sorted(unk.items())[:12]))
    print("largest AABB span seen (x,y,z):", [round(s, 1) for s in span])
