---
id: C006
kind: claim
status: holds
created: 2026-08-13
tags: animation,scripting,bosses
depends: src/engine/world.h#Actor, src/engine/world.cpp#TickMotions, src/engine/script.cpp#IsChrMotionFinish, src/host/main.cpp#missing_actor_models, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 23:36:13
---

## Claim

The default opening resolves late-spawned boss assets, advances the boss's shipping motion duration, and exits MotionFinishWait into combat.

## Evidence

The 600-frame shipping path logs late _BOSS model B0000_00 loading and BGM 2 after MotionFinishWait; movement selftest exercises unfinished, exact-end, and force-restart motion classes; full verify.sh passes.

## What would falsify it

Any change to actor motion clocks, Lua motion commands/queries, late actor rendering, room lifecycle verification, or a 600-frame opening that does not reach BGM 2.

## Re-confirmed 2026-08-13

Strengthened 600-frame opening gate observed late B0000_00 load and BGM 2; movement/motion SELFTEST passed 9/9; full verify.sh passed.

## Re-confirmed 2026-08-13

Final 600-frame opening gate observed late B0000_00 load and BGM 2 with motion resolution independent of rendering; SELFTEST 9/9 and full verify.sh passed.

## Re-confirmed 2026-08-13

Full verify.sh observed late boss asset load, BGM 2, and subsequent scripted boss movement.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; the opening resolved the late boss, advanced its motion, switched to BGM 2, moved the boss, and reached combat.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; late boss model, BGM 2, and scripted movement were observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after strict parser and cmd-API instrumentation; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after exact MPK corpus identity gating; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after mandatory runtime preflight; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full read-only tools/verify.sh passed on 2026-08-13; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after boss-death progression; late boss model, BGM 2, movement, death, and transition were observed.

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass on 2026-08-13 after the opening-router, actor mapping, coordinate, scripted-transition, and silent-test changes; every claim-specific runtime/self-test gate passed on the shipping corpus.

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass against the source landed in 37bda36 on 2026-08-13; every claim-specific runtime/self-test gate passed on the shipping corpus.
