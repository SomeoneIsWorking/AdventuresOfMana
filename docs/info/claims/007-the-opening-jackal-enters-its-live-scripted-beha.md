---
id: C007
kind: claim
status: holds
created: 2026-08-13
tags: bosses,scripting,combat
depends: src/engine/script.cpp#Dispatch, src/engine/script.h#motion_duration, src/engine/world.cpp#TickLookTargets, src/engine/world.cpp#TickScriptMoves, src/host/main.cpp#resolveMotionDuration, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 01:59:36
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

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; scripted boss movement and live attack path passed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after strict parser and cmd-API instrumentation; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after exact MPK corpus identity gating; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after mandatory runtime preflight; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full read-only tools/verify.sh passed on 2026-08-13; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after boss-death progression; live scripted Jackal behavior and its death coroutine both ran.

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass on 2026-08-13 after the opening-router, actor mapping, coordinate, scripted-transition, and silent-test changes; every claim-specific runtime/self-test gate passed on the shipping corpus.

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass against the source landed in 37bda36 on 2026-08-13; every claim-specific runtime/self-test gate passed on the shipping corpus.

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass whose stronger playable-overworld gate landed in b50191c on 2026-08-13.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh pass on 2026-08-14 after stacked-floor and Bogard-route changes; all parsers passed, all focused self-tests passed, both continuous unseeded story gates passed, and gameplay gates decoded 0 audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after inverse-vine routing, AddEnemyZaco, repeatable late placement, and offscreen story-gate changes; all focused self-tests, negative discriminators, continuous story gates, exact asset corpus, and room census passed. The heroine gate settled at sccnt=12 after five WALL_UP and three WALL_DN traversals with zero decoded audio frames.
