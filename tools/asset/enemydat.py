#!/usr/bin/env python3
"""`sk1/enemydat.bin` -- one 408-byte record per enemy.

The stride and the destination both come from the engine, not from staring at
the bytes: `AppCharacterEnemy::SetEnemyId` @ 0x2b2344 calls
`DataTableGetEnemy(id)` and then `memcpy`s `0x198` bytes into the actor at
`+0x3a24`. So record offset R is actor offset R + 0x3a24, and a field is
identified by finding who reads that actor offset:

    +0x00  id
    +0x04  max HP        actor +0x3a28
    +0x08  attack        actor +0x3a2c
    +0x0C  defence       actor +0x3a30   AppCharacterEnemy::Damage reads it here
    +0x10  EXP           actor +0x3a34
    +0x14  money         actor +0x3a38
    +0x60  shadow size   actor +0xaf8    AppCharacterBase::GetShadowSize
    +0x64  AI type       actor +0x3930   the 27-case switch in UpdateAI
    +0x68  move speed    actor +0xc64    UpdateAI, _UpdateGroundAttribute
    +0x6c  thrown weapon actor +0x3938   AppCharacterBase::WeaponThrow

Run directly to census the table. Every check states its denominator, and the
negative case is explicit: a missing or wrongly-sized file exits non-zero rather
than reporting a clean table it never read.
"""

import collections
import os
import struct
import sys

STRIDE = 0x198
# The switch in AppCharacterBase::UpdateAI @ 0x2a894c is `cmp w8, #0x1a` then
# `b.hi` to the default, so 0..26 are real cases.
AI_MAX = 26


def parse(data):
    """-> [dict]. Raises rather than returning a partial table."""
    if not data:
        raise ValueError("enemydat.bin is empty")
    if len(data) % STRIDE:
        raise ValueError(f"{len(data)} bytes is not a multiple of the "
                         f"{STRIDE}-byte stride SetEnemyId memcpys")
    out = []
    for o in range(0, len(data), STRIDE):
        eid, hp, atk, dfn, exp, money = struct.unpack_from("<6i", data, o)
        shadow, = struct.unpack_from("<f", data, o + 0x60)
        ai, = struct.unpack_from("<i", data, o + 0x64)
        speed, = struct.unpack_from("<f", data, o + 0x68)
        throw, = struct.unpack_from("<i", data, o + 0x6C)
        out.append({"id": eid, "hp": hp, "attack": atk, "defence": dfn,
                    "exp": exp, "money": money, "shadow": shadow,
                    "ai": ai, "speed": speed, "throw": throw})
    return out


def main(argv):
    path = argv[1] if len(argv) > 1 else "scratch/dump/sk1/enemydat.bin"
    if not os.path.exists(path):
        print(f"FAIL: {path} not found -- censused NOTHING", file=sys.stderr)
        return 2
    rows = parse(open(path, "rb").read())
    print(f"enemydat.bin: {len(rows)} records of {STRIDE} bytes")

    ok = True
    # Deliberately NOT "ids are 0..n-1". They are not: the table runs 0..73,
    # then 100..123, then 201..209 -- three blocks, which is real data (regular
    # enemies, then bosses, then a third group) and an assumed-contiguous check
    # called a correct parse a failure. What must hold for a table
    # DataTableGetEnemy looks up by id is that ids are unique and ascending.
    ids = [r["id"] for r in rows]
    if len(set(ids)) != len(ids) or ids != sorted(ids):
        print(f"  FAIL: ids are not unique and ascending: {ids[:12]}...")
        ok = False
    else:
        blocks = []
        start = prev = ids[0]
        for i in ids[1:]:
            if i != prev + 1:
                blocks.append((start, prev))
                start = i
            prev = i
        blocks.append((start, prev))
        print("  ids unique and ascending, in " + str(len(blocks)) + " blocks: "
              + ", ".join(f"{a}..{b}" for a, b in blocks))

    # Every field below must VARY, or it is not the field it is claimed to be:
    # a constant would read the same whether the offset were right or wrong.
    for key, label in (("hp", "max HP"), ("attack", "attack"),
                       ("defence", "defence"), ("exp", "EXP"),
                       ("money", "money"), ("speed", "move speed"),
                       ("ai", "AI type")):
        vals = {r[key] for r in rows}
        if len(vals) < 2:
            print(f"  FAIL: {label} is {vals.pop()} in all {len(rows)} records "
                  f"-- a constant, so the offset proves nothing")
            ok = False

    ai = collections.Counter(r["ai"] for r in rows)
    bad_ai = [a for a in ai if not 0 <= a <= AI_MAX]
    print(f"  AI type: {len(ai)} of the switch's {AI_MAX + 1} cases are used; "
          f"most common is {ai.most_common(1)[0][0]} in "
          f"{ai.most_common(1)[0][1]} records")
    if bad_ai:
        print(f"  FAIL: AI types {sorted(bad_ai)} fall outside the 0..{AI_MAX} "
              f"the UpdateAI switch accepts, so +0x64 is not the AI type")
        ok = False

    speeds = collections.Counter(r["speed"] for r in rows)
    still = sum(n for s, n in speeds.items() if s == 0.0)
    print("  move speed: " + ", ".join(f"{s:g}x{n}" for s, n in
                                       sorted(speeds.items())))
    print(f"    {still} enemies have speed 0 and do not move")
    if any(s < 0 or s > 200 for s in speeds):
        print(f"  FAIL: a move speed outside 0..200 units/s -- "
              f"{sorted(speeds)} -- so +0x68 is not a speed")
        ok = False

    shadows = collections.Counter(round(r["shadow"], 2) for r in rows)
    print("  shadow size: " + ", ".join(f"{s:g}x{n}" for s, n in
                                        sorted(shadows.items())))
    throwers = [r for r in rows if r["throw"]]
    print(f"  {len(throwers)} of {len(rows)} enemies carry a thrown-weapon id "
          f"({sorted({r['throw'] for r in throwers})})")

    print("ENEMYDAT OK" if ok else "ENEMYDAT FAILURES ABOVE")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
