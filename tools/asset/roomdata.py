#!/usr/bin/env python3
"""Per-room data tables: .odt (objects), .gdt (ground attributes), .edt (effects).

Every layout here comes from the engine's OWN loader, not from pattern-matching
the bytes:

  .odt  ModeGame::ObjFileLoad(char*, float, float)        @ 0x2e6904
        word0 must be 2 (`cmp w8,#2`), word1 = record count (`cmp w8,#1; b.lt`),
        records start at 0x40 with stride 0xC0 (`add x8, x27, #0xc0`). Each is an
        AppObjectModel::PARAMETERIMAGE passed to ModeGame::CreateMapObject.

  .gdt  ModeGame::Load_GroundAttribute(char*, int, int)   @ 0x2e6cf4
        The engine validates all five header fields against what it computed
        itself before memcpy'ing the payload: word0 == 1, word1 == cols,
        word2 == rows, f32@0x0C == cell width, f32@0x10 == cell height. Cell
        size is the literal 7.5 (`fmov v0.2s, #7.5`) and the grid dimensions are
        ceil(room extent / 7.5). Payload is cols*rows u32 attributes at 0x14.

  .edt  ModeGame::EffFileLoad(char*, int, int)            @ 0x2e6b54
        `cmp w0, #0x1c` rejects anything under 28 bytes and the record count is
        a divide-by-7-words (umull by 0x24924925, lsr #32 after lsr w0,#2), so
        the record is 7 u32 = 28 bytes.

Run directly to validate the whole corpus. The negative case is explicit: this
exits non-zero if a directory holds none of the three extensions, because "0
failures" over 0 files is not a pass.
"""

import glob
import os
import re
import struct
import sys

ODT_HEADER = 0x40
ODT_STRIDE = 0xC0
ODT_VERSION = 2

GDT_HEADER = 0x14
GDT_VERSION = 1
GDT_CELL = 7.5

EDT_STRIDE = 28


class FormatError(Exception):
    pass


def parse_odt(data):
    """-> list of dicts. Placements are in WORLD coordinates, not room-local."""
    if len(data) < 8:
        raise FormatError(f"too short for a header: {len(data)} bytes")
    version, count = struct.unpack_from("<Ii", data, 0)
    if version != ODT_VERSION:
        raise FormatError(f"version {version}, engine requires {ODT_VERSION}")
    want = ODT_HEADER + count * ODT_STRIDE
    if want != len(data):
        raise FormatError(f"{count} records => {want} bytes, file is {len(data)}")
    out = []
    for i in range(count):
        o = ODT_HEADER + i * ODT_STRIDE
        kind, obj_id = struct.unpack_from("<ii", data, o)
        x, y, z = struct.unpack_from("<fff", data, o + 8)
        out.append({"kind": kind, "id": obj_id, "pos": (x, y, z),
                    "raw": data[o:o + ODT_STRIDE]})
    return out


def parse_gdt(data):
    """-> (cols, rows, cell_w, cell_h, [attributes])."""
    if len(data) < GDT_HEADER:
        raise FormatError(f"too short for a header: {len(data)} bytes")
    version, cols, rows = struct.unpack_from("<Iii", data, 0)
    cw, ch = struct.unpack_from("<ff", data, 12)
    if version != GDT_VERSION:
        raise FormatError(f"version {version}, engine requires {GDT_VERSION}")
    if cols < 1 or rows < 1:
        raise FormatError(f"degenerate grid {cols}x{rows}")
    want = GDT_HEADER + cols * rows * 4
    if want != len(data):
        raise FormatError(f"{cols}x{rows} => {want} bytes, file is {len(data)}")
    attrs = list(struct.unpack_from(f"<{cols * rows}I", data, GDT_HEADER))
    return cols, rows, cw, ch, attrs


def parse_edt(data):
    """-> list of dicts. 28-byte records; word6 is 0 in every shipping file.

    Three shipping files (M0022_00_09, M0022_06_06, M0022_08_10) are a single
    byte 0x61. That is not corruption: EffFileLoad does `cmp w0, #0x1c; b.lo`
    and discards anything under one record, so the engine reads these as an
    empty table. Treated the same way here rather than counted as a failure.
    """
    if len(data) < EDT_STRIDE:
        return []
    if len(data) % EDT_STRIDE:
        raise FormatError(f"{len(data)} bytes is not a multiple of {EDT_STRIDE}")
    out = []
    for o in range(0, len(data), EDT_STRIDE):
        eff_id, flags = struct.unpack_from("<ii", data, o)
        x, y, z, scale = struct.unpack_from("<ffff", data, o + 8)
        tail = struct.unpack_from("<i", data, o + 24)[0]
        out.append({"id": eff_id, "flags": flags, "pos": (x, y, z),
                    "scale": scale, "tail": tail})
    return out


def main(argv):
    root = argv[1] if len(argv) > 1 else "scratch/dump/sk1"
    if not os.path.isdir(root):
        print(f"FAIL: {root} does not exist -- nothing was scanned", file=sys.stderr)
        return 2

    found = {ext: sorted(glob.glob(os.path.join(root, f"*.{ext}")))
             for ext in ("odt", "gdt", "edt")}
    if not any(found.values()):
        print(f"FAIL: {root} holds no .odt/.gdt/.edt at all -- scanned NOTHING, "
              f"which is not the same as 'all passed'", file=sys.stderr)
        return 2

    rc = 0
    for ext, parse in (("odt", parse_odt), ("gdt", parse_gdt), ("edt", parse_edt)):
        files = found[ext]
        ok = 0
        errs = []
        for f in files:
            try:
                parse(open(f, "rb").read())
                ok += 1
            except (FormatError, struct.error) as e:
                errs.append((os.path.basename(f), str(e)))
        if not files:
            print(f".{ext}: no files present in {root}")
            continue
        print(f".{ext}: {ok}/{len(files)} parse")
        for name, e in errs[:8]:
            print(f"    {name}: {e}")
        if errs:
            rc = 1

    # Placement check with a stated blind spot. The overworld maps pin the world
    # cell to 300x240 uniquely; several dungeon maps put objects well outside
    # their own cell and that is NOT explained -- see docs/assets.md.
    inside = outside = 0
    bad_maps = {}
    for f in found["odt"]:
        m = re.match(r"M(\d{4})_(\d{2})_(\d{2})\.odt$", os.path.basename(f))
        if not m:
            continue
        mp, gx, gy = m.group(1), int(m.group(2)), int(m.group(3))
        ox, oz = gx * 300.0, gy * 240.0
        try:
            recs = parse_odt(open(f, "rb").read())
        except (FormatError, struct.error):
            continue
        for r in recs:
            x, _, z = r["pos"]
            if ox <= x <= ox + 300 and oz <= z <= oz + 240:
                inside += 1
            else:
                outside += 1
                bad_maps[mp] = bad_maps.get(mp, 0) + 1
    if inside or outside:
        print(f"  placement: {inside} of {inside + outside} objects inside their "
              f"own 300x240 room cell; {outside} outside across "
              f"{len(bad_maps)} map(s): {','.join('M' + k for k in sorted(bad_maps))}")
        print("  (the outside ones are a KNOWN OPEN question, not a parse failure)")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
