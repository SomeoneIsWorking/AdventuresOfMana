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

    +0x80..+0x194   the AI parameter block -- see below

`AppCharacterBase::SetAITblFromEnemyTbl` @ 0x2a6cb0 is a SECOND path out of this
file, and it is the one that matters for behaviour. It calls
`DataTableGetEnemy(id)` and copies the record's `+0x80`..`+0x194` into the
actor's AI block at `+0x377c`..`+0x3894`, which is exactly two 140-byte records
-- `UpdateAI` selects between them with a toggle at actor `+0x3894` and indexes
`{base, range}` frame-count pairs inside one with the state word at `+0x38e8`.
It also seeds a `GameRandom` value into actor `+0x38f8`.

That region is 276 of the record's 408 bytes, so **two thirds of an enemy record
is AI configuration**. The copy accounts for 53 of 53 non-stack stores in the
function, so the block is filled entirely from this file; the constants the base
constructor writes there are defaults every enemy overwrites.

The block is a mix of int32 and float32. Which is which comes from the ENGINE --
whether `SetAITblFromEnemyTbl` loads a slot with `ldr w` or `ldr s` -- not from
guessing at bit patterns, and `--ai` checks that classification against all 107
records in both directions.

Run directly to census the table, `--ai` to census the AI block. Every check
states its denominator, and the negative case is explicit: a missing or
wrongly-sized file exits non-zero rather than reporting a clean table it never
read.
"""

import collections
import os
import struct
import sys

STRIDE = 0x198
# The switch in AppCharacterBase::UpdateAI @ 0x2a894c is `cmp w8, #0x1a` then
# `b.hi` to the default, so 0..26 are real cases.
AI_MAX = 26

# The AI parameter block: record +0x80..+0x194, copied to actor +0x377c..+0x3894.
AI_LO, AI_HI = 0x80, 0x194
# Slots SetAITblFromEnemyTbl loads with `ldr s` rather than `ldr w`. Transcribed
# from the disassembly, NOT inferred from the data -- inferring the type from the
# bytes and then "confirming" it against the same bytes proves nothing. These are
# checked against the corpus by ai_check(), which must be able to disagree.
AI_FLOAT_SLOTS = (0xF8, 0x100, 0x108, 0x184, 0x18C, 0x194)
# 46 of the 57 source offsets the function reads are typed by a direct `ldr w`
# or `ldr s`. The remaining 11 arrive through `ldr d`/`ldp`/`str q` SIMD moves
# that carry no type, so they are reported as untyped rather than assumed int.
AI_TYPED_SLOTS = 46
AI_SOURCE_SLOTS = 57


def ai_slots(rec):
    """-> [(offset, int_value, float_value)] over the AI block of one record."""
    out = []
    for o in range(AI_LO, AI_HI + 4, 4):
        iv, = struct.unpack_from("<i", rec, o)
        fv, = struct.unpack_from("<f", rec, o)
        out.append((o, iv, fv))
    return out


def ai_check(records, raw):
    """Cross-checks the engine's int/float split against the shipping data.

    Runs against BOTH classes: every slot the code calls a float must decode as
    one, and no slot the code calls an int may. Returns (ok, lines).
    """
    lines = []
    ok = True

    def plausible_float(off):
        good = total = 0
        for r in range(len(records)):
            iv, = struct.unpack_from("<i", raw, r * STRIDE + off)
            fv, = struct.unpack_from("<f", raw, r * STRIDE + off)
            if iv == 0:
                continue          # 0 is 0.0 either way and discriminates nothing
            total += 1
            if 1e-2 < abs(fv) < 1e6 and abs(iv) > 100000:
                good += 1
        return good, total

    for off in AI_FLOAT_SLOTS:
        good, total = plausible_float(off)
        if total == 0:
            lines.append(f"    +{off:#05x}: FAIL -- 0 nonzero records, so the "
                         f"float claim was never tested")
            ok = False
        elif good < total:
            lines.append(f"    +{off:#05x}: FAIL -- code loads it with `ldr s` "
                         f"but only {good}/{total} decode as floats")
            ok = False
        else:
            lines.append(f"    +{off:#05x} float: {good}/{total} nonzero "
                         f"records decode as plausible floats")

    # The other direction, which is what makes this a discriminator and not a
    # rubber stamp: an int slot that looks like a float means the split is wrong.
    wrong = []
    for off in range(AI_LO, AI_HI + 4, 4):
        if off in AI_FLOAT_SLOTS:
            continue
        good, total = plausible_float(off)
        if total and good >= total * 0.8:
            wrong.append(off)
    n_int = (AI_HI - AI_LO) // 4 + 1 - len(AI_FLOAT_SLOTS)
    if wrong:
        lines.append(f"    FAIL: {len(wrong)} of {n_int} int slots look like "
                     f"floats: {[hex(o) for o in wrong]}")
        ok = False
    else:
        lines.append(f"    and 0 of {n_int} int slots look like floats")
    return ok, lines


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

    raw = open(path, "rb").read()
    span = AI_HI + 4 - AI_LO
    print(f"  AI block +{AI_LO:#04x}..+{AI_HI:#05x}: {span} of {STRIDE} bytes "
          f"({span * 100 // STRIDE}%) of every record is AI configuration")
    print(f"    typed by a direct ldr w/s in SetAITblFromEnemyTbl: "
          f"{AI_TYPED_SLOTS} of {AI_SOURCE_SLOTS} source offsets; the other "
          f"{AI_SOURCE_SLOTS - AI_TYPED_SLOTS} arrive by SIMD and are untyped")
    ai_ok, lines = ai_check(rows, raw)
    for ln in lines:
        print(ln)
    if not ai_ok:
        ok = False

    if "--ai" in argv:
        # The frame-count pairs are what drive behaviour, so show their spread
        # rather than one record's numbers.
        counts = collections.Counter()
        for r in range(len(rows)):
            for off, iv, _ in ai_slots(raw[r * STRIDE:(r + 1) * STRIDE]):
                if off not in AI_FLOAT_SLOTS and iv:
                    counts[iv] += 1
        print("    most common nonzero int values across all records:")
        print("      " + ", ".join(f"{v}x{n}" for v, n in counts.most_common(10)))
        zero = sum(1 for r in range(len(rows))
                   for off, iv, _ in ai_slots(raw[r * STRIDE:(r + 1) * STRIDE])
                   if iv == 0)
        total = len(rows) * (span // 4)
        print(f"    {zero} of {total} slots are zero across the corpus")

    print("ENEMYDAT OK" if ok else "ENEMYDAT FAILURES ABOVE")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
