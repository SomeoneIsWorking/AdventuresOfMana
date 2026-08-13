---
id: C006
kind: claim
status: holds
created: 2026-08-13
tags: animation,scripting,bosses
depends: src/engine/world.h#Actor, src/engine/world.cpp#TickMotions, src/engine/script.cpp#IsChrMotionFinish, src/host/main.cpp#missing_actor_models, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 21:43:39
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
