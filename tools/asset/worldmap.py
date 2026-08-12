#!/usr/bin/env python3
"""The engine's world map: which room file sits in which grid cell, and how big it is.

`ModeGame::Process_Room` @ 0x2e3858 is the only caller of `Load_RoomDat` and
`ObjFileLoad`, and it gets the room's name by copying it out of a table that
lives inside ModeGame itself. `ModeGame::RoomSizeW` @ 0x2e3654 is the clearest
statement of the layout, because it is nine instructions long and does the whole
lookup:

    w10 = this[0xa5c]                      // world width, in cells
    w9  = this[0x9b1c]                     // current row
    w8  = this[0x9b18]                     // current column
    w8  = w10 * w9 + w8                    // linear cell index
    x8  = this + 0xa64 + w8 * 0x88         // the cell table, stride 0x88
    w8  = (int32)x8[0]                     // record+0: a size index
    s0  = *(float *)(this + 0x9dc + w8*16) // the size table, stride 16

So a cell record is `{int32 size_index; int32 unknown; char name[0x78]}` and the
name is at +8 -- which is what `Process_Room` @ 0x2e4a40 passes to `__strcpy_chk`.

Two offsets fix the block layout exactly. In memory the size table is at +0x9dc
and the cell table at +0xa64, so the header is 0xa64-0x9dc = 0x88 bytes: eight
16-byte size entries (0x80) then `int32 cols` at +0x80 and `int32 rows` at +0x84.
A block is therefore 0x88 of header plus at most 272 records of 0x88 -- and
273*0x88 = 0x9108, four bytes under the 0x910C stride measured between blocks, so
each block carries four bytes of tail padding.

The `.rodata` copy starts at 0xbd564 and there are 32 world blocks. 272 is the
largest grid (M0000 is 16x17), which is why every block is padded to that size.

Validation, both directions, printed by --check:
  - every non-empty cell's name must encode its own world id and (col,row);
  - the size the table gives must match the room's own `.gdt` header, which is
    independent ground truth (`Load_GroundAttribute` REFUSES a `.gdt` whose
    header disagrees with the size it computed, so the header IS the size);
  - a deliberately-wrong control (always 300x240) is scored on the same corpus,
    because a check that cannot separate right from wrong would score both alike.

Emits `docs/world-map.md` and `src/engine/world_table.inc`.
"""

import collections
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mpk  # noqa: E402

SO = "scratch/raw/libmcfandroid.so"
ARCHIVE = "scratch/raw/assets/sk1/sk1.mpk"

BASE = 0xBD564      # first world block (header), in .rodata
STRIDE = 0x910C     # measured between consecutive blocks
REC = 0x88          # cell record, and the header, are the same size
WORLDS = 32
MAX_CELLS = 272     # (STRIDE - REC) // REC, and M0000 is exactly 16x17


def load_blocks(data):
    """-> [(world_id, cols, rows, [size entries], {cell index: (size_idx, name)})]"""
    out = []
    for wi in range(WORLDS):
        h = BASE + wi * STRIDE
        if h + STRIDE > len(data):
            raise ValueError(f"world {wi} block at {h:#x} runs past the file")
        sizes = [struct.unpack_from("<4f", data, h + i * 16) for i in range(8)]
        cols, rows = struct.unpack_from("<ii", data, h + 0x80)
        if not (1 <= cols and 1 <= rows and cols * rows <= MAX_CELLS):
            raise ValueError(f"world {wi}: implausible grid {cols}x{rows}")
        cells = {}
        for c in range(cols * rows):
            o = h + REC + c * REC
            name = data[o + 8:o + REC].split(b"\0")[0].decode("latin1")
            if not name:
                continue                      # an empty cell -- a hole in the grid
            cells[c] = (struct.unpack_from("<i", data, o)[0], name)
        out.append((wi, cols, rows, sizes, cells))
    return out


def gdt_size(f, data_off, ents, room):
    """The room's own ground-attribute header, or None. Independent ground truth."""
    p = room + ".gdt"
    if p not in ents:
        return None
    e = ents[p]
    b = mpk.extract_one(f, data_off, e[2], e[3], e[4])
    if len(b) < 0x14:
        return None
    ver, cols, rows = struct.unpack_from("<III", b, 0)
    cw, ch = struct.unpack_from("<ff", b, 0xC)
    if ver != 1 or cw != 7.5 or ch != 7.5 or not cols or not rows:
        return None
    return (cols * cw, rows * ch)


def check(blocks):
    """Both-directions validation. Returns True only if every gate passes."""
    ok = True
    named = sum(len(b[4]) for b in blocks)
    print(f"{len(blocks)} world blocks, "
          f"{sum(b[1] * b[2] for b in blocks)} cells, {named} named")
    if named == 0:
        print("FAIL: no named cells -- validated NOTHING")
        return False

    # Gate 1: the name must encode its own world id and (col, row).
    bad = []
    for wi, cols, rows, _s, cells in blocks:
        for c, (_i, name) in cells.items():
            m = re.fullmatch(r"sk1/M(\d{4})_(\d{2})_(\d{2})", name)
            if not m or int(m.group(1)) != wi or \
               (int(m.group(2)), int(m.group(3))) != (c % cols, c // cols):
                bad.append((wi, c, name))
    print(f"  name encodes its world id and (col,row): {named - len(bad)}/{named}")
    if bad:
        ok = False
        for b in bad[:5]:
            print(f"    FAIL world {b[0]} cell {b[1]}: {b[2]!r}")

    if not os.path.exists(ARCHIVE):
        print(f"  FAIL: {ARCHIVE} not found -- size validation checked NOTHING, and")
        print("  the name gate above cannot stand in for it (it never reads a room)")
        return False

    with open(ARCHIVE, "rb") as f:
        count, dir_csize, data_off = mpk.read_header(f)
        ents = {e[0]: e for e in mpk.read_directory(f, count, dir_csize)}

        # Gate 2: rooms named by the table should exist on disk.
        missing = [n for _w, _c, _r, _s, cells in blocks
                   for _i, n in cells.values() if n + ".dat" not in ents]
        print(f"  has a .dat in the archive: {named - len(missing)}/{named}"
              + (f"   missing: {missing[:4]}" if missing else ""))

        # Gate 3: the size the table gives vs the room's own .gdt.
        agree = dis = 0
        ctl = 0
        ex = collections.Counter()
        for _w, _c, _r, sizes, cells in blocks:
            for idx, name in cells.values():
                truth = gdt_size(f, data_off, ents, name)
                if truth is None:
                    continue
                w, h = sizes[idx][0], sizes[idx][1]
                if abs(truth[0] - w) < 0.01 and abs(truth[1] - h) < 0.01:
                    agree += 1
                else:
                    dis += 1
                    ex[f"table {w:g}x{h:g} vs gdt {truth[0]:g}x{truth[1]:g}"] += 1
                if abs(truth[0] - 300) < 0.01 and abs(truth[1] - 240) < 0.01:
                    ctl += 1
        n = agree + dis
        print(f"  size matches the room's own .gdt: {agree}/{n} "
              f"({named - n} rooms have no usable .gdt)")
        for k, v in ex.most_common(5):
            print(f"    {v:4d}  {k}")
        if n == 0:
            print("  FAIL: no room had a .gdt -- the size gate checked NOTHING")
            return False
        if dis:
            ok = False
        # The control exists so a passing score means something. If a always-wrong
        # table scored the same, the corpus could not tell them apart.
        print(f"  control (always 300x240) on the same {n} rooms: {ctl}/{n} -- the "
              f"corpus discriminates" if ctl < agree else
              f"  FAIL: control scored {ctl}/{n}, no better discriminated than the table")
        if ctl >= agree:
            ok = False
    return ok


def emit(blocks):
    with open("src/engine/world_table.inc", "w") as f:
        f.write("// Generated by tools/asset/worldmap.py -- do not edit.\n")
        f.write("// The engine's world grid; see docs/world-map.md.\n")
        f.write("// WORLD(id, cols, rows)  then  CELL(world, col, row, w, h, name)\n")
        for wi, cols, rows, sizes, cells in blocks:
            f.write(f"WORLD({wi}, {cols}, {rows})\n")
            for c in sorted(cells):
                idx, name = cells[c]
                w, h = sizes[idx][0], sizes[idx][1]
                f.write(f'CELL({wi}, {c % cols}, {c // cols}, '
                        f'{w:.1f}f, {h:.1f}f, "{name}")\n')

    named = sum(len(b[4]) for b in blocks)
    with open("docs/world-map.md", "w") as f:
        f.write("# World map\n\n")
        f.write("Generated by `tools/asset/worldmap.py`. Read out of "
                "`libmcfandroid.so` .rodata at `0xbd564`,\n32 world blocks of "
                "`0x910c`. See the tool's docstring for how the layout was "
                "established.\n\n")
        f.write(f"{len(blocks)} worlds, {named} rooms.\n\n")
        f.write("| world | grid | rooms | sizes used |\n|---|---|---|---|\n")
        for wi, cols, rows, sizes, cells in blocks:
            used = collections.Counter(
                f"{sizes[i][0]:g}x{sizes[i][1]:g}" for i, _n in cells.values())
            f.write(f"| M{wi:04d} | {cols}x{rows} | {len(cells)} | "
                    f"{', '.join(f'{k} ({v})' for k, v in used.most_common())} |\n")


def main(argv):
    if not os.path.exists(SO):
        print(f"FAIL: {SO} not found -- extracted NOTHING", file=sys.stderr)
        return 2
    data = open(SO, "rb").read()
    blocks = load_blocks(data)
    ok = check(blocks)
    if "--check" in argv:
        print("WORLD MAP OK" if ok else "WORLD MAP FAILURES ABOVE")
        return 0 if ok else 1
    if not ok:
        print("refusing to emit from a table that failed validation", file=sys.stderr)
        return 1
    emit(blocks)
    print("  wrote docs/world-map.md and src/engine/world_table.inc")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
