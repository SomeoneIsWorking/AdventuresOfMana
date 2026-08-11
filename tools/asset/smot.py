#!/usr/bin/env python3
"""Parse `.smot` skeletal animations (magic `Smot`).

SiModelMotion::SetBinary copies the file verbatim and runs one fixup pass, so the
data is consumed in place. That pass still pins the spine: it reads a track count
at 0x08 and a table offset at 0x0C, then walks 32-byte entries reading a name
offset at +0x14 and OR-ing bit 5 into flags at +0x00 for every track whose name
matches the wildcard `c_eye*` (eye bones get special handling downstream).

Header:
  0x00 char[4] "Smot"          0x1C f32 duration in frames
  0x08 u32     track count     0x20 u32 total file size
  0x0C s32     track table     0x24 s32 offset of name block

Track entry (32 bytes):
  0x00 u32 flags -- channel bitmask, one 16-byte channel per set bit:
           bit2 (4)  rotation (quaternion)   bit1 (2)  translation
           bit3 (8)  scale                   bit4 (16) modifier, carries NO data
           bit5 (32) set at load time by SetBinary for `c_eye*` bones
           => value_stride == 16 * popcount(flags & 0b1110), exactly, for all
              60,803 tracks in all 1721 shipped motions.
  0x04 u32 key count
  0x08 s32 offset of key VALUE array (count * value_stride)
  0x0C u32 value stride, derived from flags (see above)
  0x10 s32 offset of key TIME array (count * 4), immediately precedes the values
  0x14 s32 offset of NUL-terminated bone name
  0x18 s32 offset of a 48-byte per-track record (3x4 bind matrix)
  0x1C u32 unused in every shipped file

The layout is self-validating end to end: time array + value array are adjacent,
consecutive tracks tile with no gaps, the 48-byte records tile up to the name
block, and the name block runs to EOF.
"""
import struct, sys

MAGIC = b'Smot'
ENTRY = 32
RECORD = 48


def parse(buf, strict=True):
    if buf[:4] != MAGIC:
        raise ValueError("not an Smot motion (magic %r)" % buf[:4])
    count, tbl = struct.unpack_from('<Ii', buf, 8)
    duration, total, names_off = struct.unpack_from('<fIi', buf, 0x1C)
    if strict and total != len(buf):
        raise ValueError("header size %d != actual file size %d" % (total, len(buf)))
    tracks = []
    for i in range(count):
        e = tbl + i * ENTRY
        flags, n, voff, vstride, toff, noff, roff, _ = struct.unpack_from('<8i', buf, e)
        name = buf[noff:buf.index(b'\0', noff)].decode('ascii', 'replace')
        if strict:
            if toff + n * 4 != voff:
                raise ValueError("track %d (%s): time array %d..%d does not abut "
                                 "values at %d" % (i, name, toff, toff + n * 4, voff))
            want = 16 * bin(flags & 0b1110).count('1')
            if vstride != want:
                raise ValueError("track %d (%s): flags %#x imply stride %d but "
                                 "file says %d" % (i, name, flags, want, vstride))
        tracks.append(dict(index=i, name=name, flags=flags, keys=n,
                           value_off=voff, value_stride=vstride,
                           time_off=toff, record_off=roff,
                           has_rotation=bool(flags & 4),
                           has_translation=bool(flags & 2),
                           has_scale=bool(flags & 8)))
    if strict and tracks:
        for a, b in zip(tracks, tracks[1:]):
            end = a['value_off'] + a['keys'] * a['value_stride']
            if end != b['time_off']:
                raise ValueError("track %d ends at %d but track %d starts at %d"
                                 % (a['index'], end, b['index'], b['time_off']))
        last = tracks[-1]
        end = last['value_off'] + last['keys'] * last['value_stride']
        if end != tracks[0]['record_off']:
            raise ValueError("key data ends at %d but records start at %d"
                             % (end, tracks[0]['record_off']))
        if tracks[0]['record_off'] + count * RECORD != names_off:
            raise ValueError("records end at %d but names start at %d"
                             % (tracks[0]['record_off'] + count * RECORD, names_off))
    return dict(tracks=tracks, count=count, duration=duration, names_off=names_off)


def times(buf, t):
    return struct.unpack_from('<%df' % t['keys'], buf, t['time_off'])


def values(buf, t):
    n = t['value_stride'] // 4
    return [struct.unpack_from('<%df' % n, buf, t['value_off'] + k * t['value_stride'])
            for k in range(t['keys'])]


if __name__ == '__main__':
    import glob, collections
    files = sys.argv[1:] or sorted(glob.glob('scratch/dump/sk1/*.smot'))
    ok, errs = 0, []
    strides, flags = collections.Counter(), collections.Counter()
    tracks_total = 0
    for p in files:
        try:
            b = open(p, 'rb').read()
            m = parse(b)
            for t in m['tracks']:
                strides[t['value_stride']] += 1
                flags[t['flags']] += 1
                tracks_total += 1
            ok += 1
        except Exception as ex:
            errs.append("%s: %s" % (p.split('/')[-1], ex))
    print("parsed %d/%d motions (%d tracks); %d FAILED"
          % (ok, len(files), tracks_total, len(errs)))
    for e in errs[:12]:
        print("  FAIL", e)
    print("value strides:", dict(sorted(strides.items())))
    print("flag values  :", dict(sorted(flags.items())))
