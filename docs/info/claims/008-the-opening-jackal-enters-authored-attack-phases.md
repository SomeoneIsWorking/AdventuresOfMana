---
id: C008
kind: claim
status: holds
created: 2026-08-13
tags: bosses,combat,collision,scripting
depends: src/mcf/assets.cpp#ParseGdt, src/mcf/assets.cpp#Collision::BlockedXZ, src/engine/script.cpp#ChrAttackBoneValid, src/engine/world.cpp#TickScriptMoves, src/host/main.cpp#ground_attribute, src/host/main.cpp#hit_this_swing, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 22:15:28
---

## Claim

The opening Jackal enters authored attack phases, damages the player once per enabled phase, and stops scripted movement on the arena wall.

## Evidence

The 600-frame shipping boundary gate reads M0001 GDT EX_1, logs _BOSS map collision, 109 overlaps, 2 landed phases and 6 player damage; movement selftest covers 19 GDT/wall/ISHITMAP/swing cases; full verify.sh passes.

## What would falsify it

Any change to GDT parsing/query, GetGroundAttribute, BlockedXZ or wall candidates, script movement collision, boss attack ownership, attack-bone fallback, swing edge/dedup, or the opening-boss attack gate.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; the 600-frame opening-boss boundary produced a _BOSS scripted map collision, 2 landed hits, 6 player damage over 2 hits, and 155 measured bone-0 fallbacks; movement selftest was 19/19.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; opening attack path again produced landed hits, player damage, and _BOSS map collision.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; landed hits, player damage, and _BOSS collision remained observed.
