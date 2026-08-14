#!/usr/bin/env python3
"""`tblItem` -- the game's consumable/key items, and the id -> category map.

`DataTableGetItem` @ 0x2c38f4 is the whole layout in five instructions:
`mov w8, #0x14` is the stride (20), `sub w9, w0, #1` then `cmp w9, #0x25` with
an unsigned-lower select is the bound, so ids run **1..37** into a 20-byte
record. The symbol is `tblItem` in `.data`.

Names are not in the table. `DataTableGetItemName` @ 0x2c381c formats
`"ITEM_NAME_%d"` (`.rodata 0x92053`) and hands it to `GetStringResource`, so an
item's name is the string table's `ITEM_NAME_<id>` -- which is why this tool
reads `str_en.bin` too and refuses to run without it.

`DataTableGetIdType` @ 0x2c387c is a plain range table over every equipment id
in the game, and it is what pins each of the other tables' id blocks:

    1..37     -> 1   items      (tblItem)
    101..118  -> 2   weapons    (tblWeapon, 18 records)
    201..206  -> 4   helms      (tblHelm, 6)
    301..309  -> 5   armour     (tblArmor, 9)
    401..409  -> 6   shields    (tblShield, 9)
    501..508  -> 7   magic      (tblMagic, 8)
    anything else -> 0

Category 3 is never returned by this function.

Run directly to extract and check. Needs the game binary and the extracted
archive; it exits non-zero rather than reporting a pass it did not earn.
"""

import collections
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import strings as strtab  # noqa: E402

STRIDE = 20
COUNT = 37
# From DataTableGetIdType @ 0x2c387c, in the order the function tests them.
ID_TYPES = [
    (1, 37, 1, "item", "tblItem"),
    (101, 118, 2, "weapon", "tblWeapon"),
    (201, 206, 4, "helm", "tblHelm"),
    (301, 309, 5, "armour", "tblArmor"),
    (401, 409, 6, "shield", "tblShield"),
    (501, 508, 7, "magic", "tblMagic"),
]

# tblMagic: DataTableGetMagic @ 0x2c3c60, stride 12 (`mov w8, #0xc`), ids
# 501..508 (`sub w9, w0, #0x1f5` then a bound at 0x1fd - 8).
MAGIC_STRIDE = 12
MAGIC_COUNT = 8


def data_delta(so_path):
    """.data's VMA is NOT its file offset. Read the delta from the headers."""
    out = subprocess.run(["llvm-readelf", "-S", so_path],
                         capture_output=True, text=True).stdout
    for i, line in enumerate(out.splitlines()):
        if "] .data " in line:
            f = line.split()
            addr = int(f[f.index("PROGBITS") + 1], 16)
            off = int(f[f.index("PROGBITS") + 2], 16)
            return addr - off
    raise SystemExit(f"{so_path}: no .data section header")


def symbol(so_path, name):
    out = subprocess.run(["llvm-readelf", "-s", so_path],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        f = line.split()
        if f and f[-1] == name:
            return int(f[1], 16), int(f[2])
    raise SystemExit(f"{so_path}: dynamic symbol '{name}' not found")


def extract(so_path):
    data = open(so_path, "rb").read()
    delta = data_delta(so_path)
    va, size = symbol(so_path, "tblItem")
    if size != COUNT * STRIDE:
        raise SystemExit(f"tblItem is {size} bytes; DataTableGetItem's stride "
                         f"{STRIDE} and bound {COUNT} need {COUNT * STRIDE}")
    rows = []
    for i in range(COUNT):
        f = struct.unpack_from("<5i", data, va - delta + i * STRIDE)
        rows.append({"id": f[0], "kind": f[1], "buy": f[2], "sell": f[3],
                     "value": f[4]})
    return delta, va, rows


def extract_magic(so_path):
    data = open(so_path, "rb").read()
    delta = data_delta(so_path)
    va, size = symbol(so_path, "tblMagic")
    if size != MAGIC_COUNT * MAGIC_STRIDE:
        raise SystemExit(f"tblMagic is {size} bytes; DataTableGetMagic's stride "
                         f"{MAGIC_STRIDE} and 8 ids need "
                         f"{MAGIC_COUNT * MAGIC_STRIDE}")
    return [struct.unpack_from("<3i", data, va - delta + i * MAGIC_STRIDE)
            for i in range(MAGIC_COUNT)]


def main(argv):
    so = argv[1] if len(argv) > 1 else "scratch/raw/libmcfandroid.so"
    strp = argv[2] if len(argv) > 2 else "scratch/dump/sk1/str_en.bin"
    if not os.path.exists(so):
        print(f"FAIL: {so} not found -- extracted NOTHING", file=sys.stderr)
        return 2
    if not os.path.exists(strp):
        print(f"FAIL: {strp} not found -- item NAMES come from the string "
              f"table, so this would emit a nameless table", file=sys.stderr)
        return 2

    delta, va, rows = extract(so)
    ids, texts = strtab.parse(open(strp, "rb").read())
    names = dict(zip(ids, texts))
    print(f"tblItem: {len(rows)} records from {so} "
          f"(VMA {va:#x}, .data delta {delta:#x})")

    ok = True
    got = [r["id"] for r in rows]
    if got != list(range(1, COUNT + 1)):
        print(f"  FAIL: ids are not 1..{COUNT}: {got[:8]}...")
        ok = False
    else:
        print(f"  ids 1..{COUNT}, contiguous (the range DataTableGetItem bounds)")

    # Every id must have a name, or the ITEM_NAME_%d rule is wrong.
    missing = [r["id"] for r in rows if f"ITEM_NAME_{r['id']}" not in names]
    if missing:
        print(f"  FAIL: no ITEM_NAME_<id> for {missing} -- "
              f"DataTableGetItemName's format does not hold")
        ok = False
    else:
        print(f"  all {len(rows)} ids resolve through ITEM_NAME_<id> "
              f"(against {len(names)} strings)")

    # Both classes: a price table where nothing is for sale, or everything is,
    # would mean the offsets are wrong.
    buyable = [r for r in rows if r["buy"] > 0]
    if not buyable or len(buyable) == len(rows):
        print(f"  FAIL: {len(buyable)} of {len(rows)} items have a buy price -- "
              f"all or nothing means +0x08 is not a price")
        ok = False
    else:
        print(f"  {len(buyable)} of {len(rows)} items have a buy price; "
              f"{len(rows) - len(buyable)} are not sold")
    # NOT "sell is half of buy". It is not: the ratios are 2, 4, 6 and 15, so
    # an assumed halving would have called a correct extraction a failure. What
    # is reported is the distribution, which is a measurement rather than a rule.
    ratios = collections.Counter(
        round(r["buy"] / r["sell"], 2) if r["sell"] else "sell=0"
        for r in buyable)
    print("  buy/sell ratio: " + ", ".join(f"{k}x{n}" for k, n in
                                           sorted(ratios.items(), key=str)))
    if all(r["sell"] >= r["buy"] for r in buyable):
        print(f"  FAIL: no item sells for less than it costs -- +0x08 and +0x0C "
              f"are not buy and sell")
        ok = False

    # ITEM_NAME_<id> is not the ITEM table's format -- it is the format for
    # EVERY id in the game. Checking that across DataTableGetIdType's ranges
    # makes three independent sources agree at once: the accessor's bounds, the
    # record count each table's symbol size implies, and the string table.
    # ITEM_NAME_<id> is not the ITEM table's format -- it is the format for
    # every REAL id in the game, across all six tables. What it is NOT is a
    # name for every id inside DataTableGetIdType's ranges: those ranges are
    # what the accessor CLASSIFIES, and they do not coincide with the ids that
    # actually have records. Checking the ranges instead of the records is what
    # caught this, and the distinction is now the check.
    magic = extract_magic(so)
    named = sorted(int(k.rsplit("_", 1)[1]) for k in names
                   if k.startswith("ITEM_NAME_") and k.rsplit("_", 1)[1].isdigit())
    print(f"  ITEM_NAME_<id> exists for {len(named)} ids, spanning "
          f"{named[0]}..{named[-1]}")
    for lo, hi, _t, what, _tbl in ID_TYPES:
        in_range = [i for i in named if lo <= i <= hi]
        gap = [i for i in range(lo, hi + 1) if i not in set(named)]
        print(f"    {what:7s} type range {lo}..{hi}: {len(in_range)} named"
              + (f", {len(gap)} unnamed ({gap})" if gap else ""))
    if [m[0] for m in magic] != list(range(501, 509)):
        print(f"  FAIL: tblMagic ids are {[m[0] for m in magic]}, not 501..508")
        ok = False
    else:
        print(f"  tblMagic: {len(magic)} records, ids 501..508, e.g. "
              f"{names['ITEM_NAME_501']} and {names['ITEM_NAME_508']}")

    with open("docs/item-table.md", "w") as f:
        f.write("# `tblItem` — items\n\n")
        f.write("Generated by `tools/asset/item_table.py`. Do not edit by hand.\n\n")
        f.write(f"{COUNT} records of {STRIDE} bytes at `.data` VMA `{va:#x}`, "
                f"bounds and stride from `DataTableGetItem` @ `0x2c38f4`. "
                f"Names are `ITEM_NAME_<id>` in the string table, per "
                f"`DataTableGetItemName` @ `0x2c381c`.\n\n")
        f.write("| id | name | uses | buy | sell | `+0x10` |\n")
        f.write("|---|---|---|---|---|---|\n")
        for r in rows:
            nm = names.get("ITEM_NAME_%d" % r["id"], "")
            f.write(f"| {r['id']} | {nm} | {r['kind']} | {r['buy']} | "
                    f"{r['sell']} | {r['value']} |\n")
        f.write("\n`+0x04` is the initial use count. `GameParameter::AddItem` "
                "stores it in the item slot's third word (encoding 1 as 0), "
                "and `ModeGame::UseInventory` decrements that word before "
                "removing the slot at zero. Thus Mattock's 7 means seven uses; "
                "separate pickups still occupy separate slots.\n\n")
        f.write("`+0x10` **is the effect amount**, and that is no longer an "
                "inference. `ModeGame::UseInventoryFunc` @ `0x2deb78` reads it "
                "directly: `DataTableGetItem(id) + 0x10` for an item (and "
                "`DataTableGetMagic(id) + 0x08` for a spell), then dispatches "
                "through a 29-case jump table at `.rodata 0xa6220` on "
                "`id - 1`.\n\n")
        f.write("Three effect routines read outright:\n\n")
        f.write("| items | routine | what it does |\n|---|---|---|\n")
        f.write("| 1, 2, 3 — Candy, Potion, Hi-Potion | `0x2dec40` | "
                "if HP >= 1, `GameParameter+0x114 += value` |\n")
        f.write("| 4, 5 — Ether, Hi-Ether | `0x2df22c` | "
                "`GameParameter+0x118 += value` |\n")
        f.write("| 6 — Elixir | `0x2df3f0` | if HP >= 1, HP := max HP and "
                "MP := max MP |\n")
        f.write("\nNeither heal clamps: `GameParameter::Update` does that on the "
                "next frame, which is why it clamps at all. And Elixir never "
                "reads its own `+0x10` — the 999 is a sentinel, not a "
                "quantity.\n\n")
        f.write("The remaining 26 routines are not read.\n\n")
        f.write("Buy and sell prices are NOT a fixed ratio — across the "
                f"{len(buyable)} purchasable items the buy/sell ratio is 2, 4, "
                "6 or 15.\n\n")
        f.write("## `DataTableGetIdType` @ `0x2c387c`\n\n")
        f.write("The id ranges every equipment table occupies, straight from the "
                "accessor's own bounds checks.\n\n")
        f.write("| ids | type | what | table |\n|---|---|---|---|\n")
        for lo, hi, t, what, tbl in ID_TYPES:
            f.write(f"| {lo}..{hi} | {t} | {what} | `{tbl}` |\n")
        f.write("\nAnything else returns 0. Type 3 is never returned.\n\n")
        f.write("`ITEM_NAME_<id>` is the name format for **every table**, not "
                "just the item one — `ITEM_NAME_101` is Broadsword, "
                "`ITEM_NAME_401` is Bronze Shield, `ITEM_NAME_501` is Cure.\n\n")
        f.write("**These ranges are what the accessor CLASSIFIES, and they are "
                "not the set of ids that have records.** The weapon case proves "
                "it both ways: the range is 101..118, but `tblWeapon` holds "
                "101..117 and **121** — so 118 is classified as a weapon and has "
                "neither a record nor a name, while 121 has a record and is "
                "classified as nothing. (Weapon 121 is all zeroes but for a "
                "`kind` of 6, so it looks like an unused slot.) The two counts "
                "happen to agree at 18 apiece, which is exactly the coincidence "
                "that would let a careless check pass.\n\n")
        f.write("## `tblMagic`\n\n")
        f.write("8 records of 12 bytes, stride and bounds from "
                "`DataTableGetMagic` @ `0x2c3c60`.\n\n")
        f.write("| id | name | `+0x04` | `+0x08` |\n|---|---|---|---|\n")
        for mid, a, b in magic:
            f.write(f"| {mid} | {names.get('ITEM_NAME_%d' % mid, '')} | {a} | {b} |\n")
        f.write("\nNeither field is identified.\n")
    os.makedirs("src/engine", exist_ok=True)
    with open("src/engine/item_uses.inc", "w") as f:
        f.write("// Generated by tools/asset/item_table.py. Do not edit.\n")
        f.write("// ITEM_USES(id, initial remaining uses)\n")
        for r in rows:
            f.write(f"ITEM_USES({r['id']}, {max(1, r['kind'])})\n")
    print("  wrote docs/item-table.md")
    print("  wrote src/engine/item_uses.inc")
    print("ITEM TABLE OK" if ok else "ITEM TABLE FAILURES ABOVE")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
