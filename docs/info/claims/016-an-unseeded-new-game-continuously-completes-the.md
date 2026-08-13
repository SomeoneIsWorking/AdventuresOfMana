---
id: C016
kind: claim
status: holds
created: 2026-08-14
tags: gameplay,progression,verification
depends: src/host/main.cpp#main, src/engine/script.cpp#Dispatch, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 01:59:37
---

## Claim

An unseeded new game continuously completes the heroine encounter at scenario state 12

## Evidence

2026-08-14 full ./tools/verify.sh pass: offscreen fixed-step --opening-story --continue-story run completed in 4124 frames, traversed five WALL_UP and three WALL_DN pairs, spawned and engine-placed three Myconids through AddEnemyZaco, seeded all three from enemydat.bin, killed all three, fired EnemyDead, completed Hasim dialogue, and stopped settled at sccnt=12 with zero live coroutines and zero decoded audio frames.

## What would falsify it

Falsified if the mandatory heroine gate no longer starts from an unseeded new game, does not observe exactly five upward and three downward vine traversals, does not create/place/seed/kill all three Myconids and fire EnemyDead, fails to settle at sccnt=12, opens a visible window, or decodes audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after inverse-vine routing, AddEnemyZaco, repeatable late placement, and offscreen story-gate changes; all focused self-tests, negative discriminators, continuous story gates, exact asset corpus, and room census passed. The heroine gate settled at sccnt=12 after five WALL_UP and three WALL_DN traversals with zero decoded audio frames.
