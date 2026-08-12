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

# The AI block is built out of a repeating 24-byte STATE DESCRIPTOR:
#
#     +0x00 .. +0x0c   four int32 parameters
#     +0x10 .. +0x14   {base, range} -- the state's duration in frames,
#                      realised as base + GameRandom(range)
#
# Two groups of four, one per 140-byte record, and the bases are not guessed:
# each is pinned by SetAITblFromEnemyTbl copying that exact offset into the
# matching actor slot. The four leading words of descriptor N land at actor
# +0x379c + N*0x10 (record 0), and the pair lands in the record itself, where
# UpdateAI indexes it as `rec + state*8`.
DESC_BASES = ((0x80, 0x98, 0xB0, 0xC8),        # record 0
              (0x10C, 0x124, 0x13C, 0x154))    # record 1
DESC_SIZE = 0x18
# Record +0x88 (from enemy +0xe8) is an AI PROBABILITY, in percent. The engine
# reads it as `GameRandom(100)` vs `rec[+0x88]`. What it gates varies by AI type:
# type 15 (0x2a90bc) rolls it to reach mode 9, type 7 (0x2a9138) to reach mode 6.
# It is the chance of taking the case's ALTERNATIVE branch, not of one fixed
# mode -- an earlier version of this note said "the wander" and was over-general. It is the most-read field
# of the record (25 reads off the record pointer in UpdateAI).
#
# "0..100" alone would be a weak check, since a small enum passes it too. What
# identifies this as a designer-authored percentage is the VOCABULARY: the only
# values in the whole corpus are 0, 10, 20, 30, 60, 80, 90, 95 and 100.
AI_PROB_OFF = 0xE8
# The four params are TRANSITION WEIGHTS, one per destination state, and
# UpdateAI @ 0x2a8d50 picks the next state by weighted roulette:
#
#     q0   = the four weights of the CURRENT state   ldr q0, [rec + state*0x10 + 0x20]
#     sum  = addv s0, v0.4s
#     if (sum < 1) -> no transition at all, the state stands
#     roll = GameRandom(sum)
#     roll -= w0; if (roll < 0) next = 0
#     roll -= w1; if (roll < 0) next = 1
#     roll -= w2; if (roll < 0) next = 2
#     next = (roll < w3) ? 3 : unchanged
#
# All FOUR states are real. An earlier note here said the state word was only
# ever stored as 0, 1 or 3, from a backward scan of the store sites -- but the
# four branches above converge on ONE store (0x2a8e0c), so a linear backward
# walk only ever sees the nearest `mov`, which is #3. The corpus disagreed with
# that scan (167 of 214 records carry a populated state-2 descriptor) and the
# corpus was right.
STATE_COUNT = 4
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

    # The descriptors are the part of the block whose SHAPE is understood, so
    # they get a real check: a duration must be a duration.
    bad_desc = []
    nonzero = 0
    for r in range(len(rows)):
        for rec, bases in enumerate(DESC_BASES):
            for st, b in enumerate(bases):
                base, rng = struct.unpack_from("<2i", raw, r * STRIDE + b + 0x10)
                params = struct.unpack_from("<4i", raw, r * STRIDE + b)
                if (base, rng) == (0, 0) and not any(params):
                    continue
                nonzero += 1
                # This test was WEAKER at first and a deliberate sabotage --
                # shifting a base by 4 -- sailed through it, because "1..3600
                # is a plausible frame count" stays true after a shift. What
                # actually separates a right base from a wrong one was measured
                # over the corpus rather than assumed:
                #
                #   at the correct offset : 0 of 2688 params exceed 20, and no
                #                           timer base lands in 1..14
                #   shifted by +4/-4/+8   : 671 / 416 / 1229 params exceed 20
                #
                # so those two conditions are the discriminator, and both
                # sabotage directions now fail as they must.
                if max(params) > 20 or 1 <= base <= 14:
                    bad_desc.append((rows[r]["id"], rec, st, params, base, rng))
    total_desc = len(rows) * 8
    print(f"  AI state descriptors: {DESC_SIZE}-byte, 4 per record x 2 records; "
          f"{nonzero} of {total_desc} carry a nonzero timer")
    if bad_desc:
        print(f"    FAIL: {len(bad_desc)} timers are not plausible frame counts, "
              f"so a descriptor base is wrong: {bad_desc[:5]}")
        ok = False
    else:
        print(f"    all {nonzero} descriptors pass: every param <= 20 and no "
              f"timer base in 1..14 (the measured discriminator)")
    per_state = collections.Counter()
    for r in range(len(rows)):
        for rec, bases in enumerate(DESC_BASES):
            for st, b in enumerate(bases):
                if struct.unpack_from("<2i", raw, r * STRIDE + b + 0x10) != (0, 0):
                    per_state[st] += 1
    print("    populated per state: " + ", ".join(
        f"{st}={per_state[st]}" for st in range(STATE_COUNT)))

    # The weights are a probability distribution, so they must be non-negative;
    # a negative one would mean the param block is not what it is claimed to be.
    # `sum == 0` is meaningful rather than missing: UpdateAI's `cmp w23, #1 /
    # b.lt` skips the roll entirely, so the state simply stands.
    neg = terminal = rolling = 0
    machines = set()
    for r in range(len(rows)):
        for rec, bases in enumerate(DESC_BASES):
            m = []
            for b in bases:
                w = struct.unpack_from("<4i", raw, r * STRIDE + b)
                pair = struct.unpack_from("<2i", raw, r * STRIDE + b + 0x10)
                m.append((w, pair))
                if min(w) < 0:
                    neg += 1
                if sum(w) < 1:
                    terminal += 1
                else:
                    rolling += 1
            machines.add(tuple(m))
    if neg:
        print(f"    FAIL: {neg} descriptors carry a negative transition weight, "
              f"so the param block is not a weight table")
        ok = False
    else:
        print(f"    transition weights: {rolling} descriptors roll for a next "
              f"state, {terminal} have sum 0 and stand; 0 negative weights")
    print(f"    {len(machines)} distinct state machines across "
          f"{len(rows) * len(DESC_BASES)} descriptor sets")

    probs = [struct.unpack_from("<i", raw, r * STRIDE + AI_PROB_OFF)[0]
             for r in range(len(rows))]
    out_of_range = [p for p in probs if not 0 <= p <= 100]
    if out_of_range:
        print(f"    FAIL: {len(out_of_range)} AI probabilities outside 0..100 "
              f"({sorted(set(out_of_range))[:6]}), so +{AI_PROB_OFF:#x} is not a "
              f"percentage")
        ok = False
    else:
        nz = sum(1 for p in probs if p)
        print(f"    AI probability +{AI_PROB_OFF:#x}: {nz}/{len(rows)} enemies "
              f"have one; values {sorted(set(probs))}")

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
