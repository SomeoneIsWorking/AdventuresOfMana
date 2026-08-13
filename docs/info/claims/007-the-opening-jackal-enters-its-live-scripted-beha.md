---
id: C007
kind: claim
status: holds
created: 2026-08-13
tags: bosses,scripting,combat
depends: src/engine/script.cpp#Dispatch, src/engine/script.h#motion_duration, src/engine/world.cpp#TickLookTargets, src/engine/world.cpp#TickScriptMoves, src/host/main.cpp#resolveMotionDuration, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 22:02:34
---

## Claim

The opening Jackal enters its live scripted behavior loop and moves under the map's _BOSS coroutine.

## Evidence

The shipping 600-frame gate logs scripted movement beginning for _BOSS; a 900-frame trace changes closest distance from the formerly fixed 105.0 to 70.8; movement selftest covers 13 math/status/motion/movement cases; full verify.sh passes.

## What would falsify it

Any change to script HP slots, math_LerpSin/math_atan2/bit_and, ChrLookTarget, synchronous motion duration lookup, scripted movement accounting, or an opening run without _BOSS movement.

## Re-confirmed 2026-08-13

Shipping 600-frame gate logged _BOSS scripted movement; 900-frame closest distance changed 105.0 to 70.8; SELFTEST 13/13 and full verify.sh passed.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; the opening Jackal entered its scripted loop, moved, collided with the arena wall, and executed live attack phases.
