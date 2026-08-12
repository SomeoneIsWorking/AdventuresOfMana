#!/usr/bin/env python3
"""Extract `tblWeapon` -- the player's weapon stats -- from the game binary.

`DataTableGetWeapon(int)` @ 0x2c3a1c is a FULLY UNROLLED linear search: it
compares word 0 of each record against the argument at hard-coded displacements
0x00, 0x28, 0x50, 0x78 ... 0x2a8. That gives the stride (0x28 = 40 bytes) and
the count (0x2a8 / 0x28 + 1 = 18) directly from the code, and 18 * 40 = 720
matches the dynamic symbol's size for `tblWeapon` exactly.

THE TRAP THIS TOOL EXISTS TO AVOID: `.data` is NOT mapped at its file offset.
.rodata happens to be (VMA 0x8ca90 == offset 0x8ca90), so reading .rodata blobs
by virtual address works and lulls you into doing the same for .data -- where
VMA 0x40e490 lives at offset 0x406490, a 0x8000 delta. Reading tblWeapon at its
VMA produces plausible-looking integers that are simply the wrong bytes. The
delta is computed from the section header here, never assumed.

Emits `docs/weapon-table.md` and `src/engine/weapon_table.inc`.
"""

import os
import struct
import subprocess
import sys

TBL_STRIDE = 0x28          # from DataTableGetWeapon's unrolled displacements
TBL_COUNT = 18             # last displacement 0x2a8 / 0x28 + 1
TBL_BYTES = TBL_STRIDE * TBL_COUNT


def data_delta(so_path):
    """VMA - file offset for the section holding .data. Read, never assumed."""
    out = subprocess.run(["llvm-readelf", "-S", so_path],
                         capture_output=True, text=True).stdout
    for line in out.split("\n"):
        parts = line.split()
        # "[22] .data PROGBITS <addr> <off> <size> ..."
        if len(parts) > 6 and parts[1] == ".data" and parts[2] == "PROGBITS":
            return int(parts[3], 16) - int(parts[4], 16)
    raise SystemExit(f"{so_path}: no .data section header found")


def symbol(so_path, name):
    """-> (vaddr, size) for a defined dynamic symbol."""
    out = subprocess.run(["llvm-nm", "--dynamic", "--defined-only", "-S", so_path],
                         capture_output=True, text=True).stdout
    for line in out.split("\n"):
        p = line.split()
        if len(p) == 4 and p[3] == name:
            return int(p[0], 16), int(p[1], 16)
    raise SystemExit(f"{so_path}: dynamic symbol '{name}' not found")


# tblHelm / tblArmor: DataTableGetDefence @ 0x2c3bd8 indexes them with a stride
# of 20 (`mov w9, #0x14`). Ids are 201.. for helms and 301.. for armor, which is
# what GameParameter::Init grants a new game (AddItem 0xc9 and 0x12d).
# tblShield is the FOURTH slot GameParameter::Init grants (AddItem 0x191 = 401),
# and DataTableGetDefence resolves ids 401..409 out of it with the same stride.
# GameParameter::Update does NOT add it to the player's defence -- it adds only
# the helm and the armour -- so it is extracted and documented, not applied.
DEFENCE_TABLES = (("tblHelm", 201), ("tblArmor", 301))
# The FOURTH slot GameParameter::Init grants (AddItem 0x191 = 401), resolved by
# DataTableGetDefence with the same stride -- but every record's defence word is
# ZERO, and what varies is +0x10: 1, 3, 7, 15, 31, 55, 87, 127. That is a bit
# mask, not a number, so a shield does not reduce damage; it blocks kinds of it.
# Which is exactly why GameParameter::Update adds the helm and the armour to the
# player's defence and NOT the shield. Extracted and documented, never applied.
SHIELD_TABLE = ("tblShield", 401)
DEF_STRIDE = 20


def extract_defence(so_path):
    """-> {name: [(id, defence), ...]}. Stride from DataTableGetDefence."""
    data = open(so_path, "rb").read()
    delta = data_delta(so_path)
    out = {}
    for name, _first in DEFENCE_TABLES:
        va, size = symbol(so_path, name)
        if size % DEF_STRIDE:
            raise SystemExit(f"{name} is {size} bytes, not a multiple of the "
                             f"{DEF_STRIDE}-byte stride DataTableGetDefence uses")
        rows = []
        for i in range(size // DEF_STRIDE):
            o = va - delta + i * DEF_STRIDE
            f = struct.unpack_from("<5i", data, o)
            rows.append((f[0], f[1]))
        out[name] = rows
    return out


def extract_shield(so_path):
    """-> [(id, defence_word, mask)]. Same stride; the mask is at +0x10."""
    data = open(so_path, "rb").read()
    delta = data_delta(so_path)
    name, _first = SHIELD_TABLE
    va, size = symbol(so_path, name)
    if size % DEF_STRIDE:
        raise SystemExit(f"{name} is {size} bytes, not a multiple of {DEF_STRIDE}")
    rows = []
    for i in range(size // DEF_STRIDE):
        f = struct.unpack_from("<5i", data, va - delta + i * DEF_STRIDE)
        rows.append((f[0], f[1], f[4]))
    return rows


def extract(so_path):
    data = open(so_path, "rb").read()
    delta = data_delta(so_path)
    va, size = symbol(so_path, "tblWeapon")
    # The symbol's own size must agree with the stride and count the code
    # implies. If it does not, one of the two is wrong and guessing which
    # would silently produce garbage.
    if size != TBL_BYTES:
        raise SystemExit(f"tblWeapon is {size} bytes but DataTableGetWeapon's "
                         f"unrolled search implies {TBL_COUNT} x {TBL_STRIDE} "
                         f"= {TBL_BYTES}")
    off = va - delta
    if off + size > len(data):
        raise SystemExit(f"tblWeapon at file offset {off:#x} runs past the file")
    rows = []
    for i in range(TBL_COUNT):
        o = off + i * TBL_STRIDE
        f = struct.unpack_from("<10i", data, o)
        rows.append({"id": f[0], "atk_lo": f[1], "atk_hi": f[2],
                     "buy": f[3], "sell": f[4], "kind": f[5], "raw": f})
    return delta, va, rows


def verify(rows):
    """Checks that can actually fail, not restatements of the parse."""
    ok = True
    ids = [r["id"] for r in rows]
    if len(set(ids)) != len(ids):
        print(f"  FAIL: duplicate weapon ids {ids}")
        ok = False
    if any(i <= 0 for i in ids):
        print(f"  FAIL: non-positive weapon id in {ids}")
        ok = False
    print(f"  ids: {min(ids)}..{max(ids)}, {len(set(ids))} distinct")
    # The attack range must be a range, and it must broadly climb with id --
    # a wrong stride or a wrong .data delta destroys both properties.
    bad = [r["id"] for r in rows if r["atk_hi"] < r["atk_lo"]]
    if bad:
        print(f"  FAIL: atk_hi < atk_lo for ids {bad}")
        ok = False
    else:
        print(f"  all {len(rows)} records have atk_hi >= atk_lo")
    real = [r for r in rows if r["atk_hi"] > 0]
    first, last = real[0], real[-1]
    if last["atk_hi"] <= first["atk_hi"]:
        print(f"  FAIL: attack does not climb ({first['atk_hi']} -> {last['atk_hi']})")
        ok = False
    else:
        print(f"  attack climbs with id: {first['atk_lo']}-{first['atk_hi']} "
              f"(id {first['id']}) .. {last['atk_lo']}-{last['atk_hi']} "
              f"(id {last['id']})")
    return ok


def emit(delta, va, weapons, defence, shields):
    rows = weapons
    with open("docs/weapon-table.md", "w") as f:
        f.write("# `tblWeapon` — player weapon stats\n\n")
        f.write("Generated by `tools/asset/weapon_table.py`. Do not edit by hand.\n\n")
        f.write(f"From the game binary's `.data` at VMA `{va:#x}` "
                f"(file offset `{va - delta:#x}`; `.data` is mapped at a "
                f"`{delta:#x}` delta from its file offset — reading it by VMA "
                f"yields the wrong bytes).\n\n")
        f.write(f"{TBL_COUNT} records of `{TBL_STRIDE:#x}` bytes, both taken from "
                f"`DataTableGetWeapon`'s fully unrolled search.\n\n")
        f.write("| id | attack | buy | sell | kind |\n|---|---|---|---|---|\n")
        for r in rows:
            f.write(f"| {r['id']} | {r['atk_lo']}–{r['atk_hi']} | {r['buy']} | "
                    f"{r['sell']} | {r['kind']} |\n")
        f.write("\nFields at +0x1C, +0x20 and +0x24 are not identified; "
                "+0x24 is 8 in every record.\n")
        for name, _ in DEFENCE_TABLES:
            f.write(f"\n## `{name}` — defence\n\n")
            f.write("Stride 20, from `DataTableGetDefence` @ `0x2c3bd8`.\n\n")
            f.write("| id | defence |\n|---|---|\n")
            for i, d in defence[name]:
                f.write(f"| {i} | {d} |\n")
        f.write("\n## `tblShield` — the fourth equipment slot\n\n")
        f.write("`GameParameter::Init` grants id 401 alongside the weapon, helm "
                "and armour, and `DataTableGetDefence` resolves 401..409 out of "
                "this table with the same stride 20. But every record's defence "
                "word is **zero**; what varies is `+0x10`. That is a bit mask, "
                "so a shield blocks KINDS of damage rather than reducing it — "
                "which is why `GameParameter::Update` adds the helm and the "
                "armour to the player's defence and not the shield.\n\n")
        f.write("| id | defence | mask `+0x10` |\n|---|---|---|\n")
        for i, d, m in shields:
            f.write(f"| {i} | {d} | `0x{m:02x}` |\n")
        f.write("\nWhat the mask's bits mean is NOT established.\n")
    with open("src/engine/weapon_table.inc", "w") as f:
        f.write("// Generated by tools/asset/weapon_table.py -- do not edit.\n")
        f.write("// id, attack low, attack high; see docs/weapon-table.md.\n")
        for r in rows:
            f.write(f"WEAPON({r['id']}, {r['atk_lo']}, {r['atk_hi']})\n")
        f.write("// id, defence -- tblHelm then tblArmor.\n")
        for name, _ in DEFENCE_TABLES:
            for i, d in defence[name]:
                f.write(f"DEFENCE({i}, {d})\n")


def main(argv):
    so = argv[1] if len(argv) > 1 else "scratch/raw/libmcfandroid.so"
    if not os.path.exists(so):
        print(f"FAIL: {so} not found -- extracted NOTHING", file=sys.stderr)
        return 2
    delta, va, rows = extract(so)
    print(f"tblWeapon: {len(rows)} records from {so} "
          f"(VMA {va:#x}, .data delta {delta:#x})")
    ok = verify(rows)
    dfn = extract_defence(so)
    for name, first in DEFENCE_TABLES:
        drows = dfn[name]
        ids = [i for i, _ in drows]
        defs = [d for _, d in drows]
        # Deliberately NOT "defence ascends": tblArmor id 309 has defence 18
        # after 308's 44, which is real data (a late alternative piece), and an
        # ascending check would have called correct extraction a failure.
        # What must hold is that the ids are the contiguous run the accessor's
        # bounds checks imply, and that every piece has positive defence.
        good = (ids == list(range(first, first + len(ids)))
                and all(d > 0 for d in defs))
        print(f"  {name}: {len(drows)} records, ids {ids[0]}..{ids[-1]}, "
              f"defence {min(defs)}..{max(defs)}"
              + ("" if good else "   FAIL: ids not contiguous from "
                                 f"{first}, or a non-positive defence"))
        if not good:
            ok = False
    shields = extract_shield(so)
    sids = [i for i, _, _ in shields]
    sdef = [d for _, d, _ in shields]
    masks = [m for _, _, m in shields]
    # The claim being tested is "a shield's defence word is dead and the mask is
    # the live field". Both halves must hold, and both can fail.
    if any(d != 0 for d in sdef):
        print(f"  FAIL: tblShield defence words are not all zero: {sdef}")
        ok = False
    elif len(set(masks)) < 2:
        print(f"  FAIL: tblShield masks are all {masks[0]} -- nothing varies, so "
              f"the field is not what distinguishes shields")
        ok = False
    else:
        print(f"  tblShield: {len(shields)} records, ids {sids[0]}..{sids[-1]}, "
              f"defence all 0, +0x10 mask {sorted(set(masks))} -- shields block "
              f"kinds of damage, they do not reduce it")
    emit(delta, va, weapons=rows, defence=dfn, shields=shields)
    print("  wrote docs/weapon-table.md and src/engine/weapon_table.inc")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
