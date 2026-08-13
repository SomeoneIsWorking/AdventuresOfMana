---
id: C009
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/host/main.cpp#seedCombat, src/engine/world.cpp#World::ConsumeEnemyWaveCleared, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 22:46:53
---

## Claim

The first Jackal fight advances through EnemyDead into Will's room

## Evidence

2026-08-13 full tools/verify.sh: opening positive records MainPlayer killed _BOSS, exactly one EnemyDead coroutine start, mapjump to M0001_00_02, destination Init, and Will dialogue; nonlethal boundary negative has no EnemyDead start; movement selftest 24/24 covers five wave-transition states.

## What would falsify it

Any change to player volume seeding, Actor alive/defeated lifecycle, World::ConsumeEnemyWaveCleared, combat kill handling, coroutine start, mapjump, or the opening death verifier.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13: lethal opening path recorded one player-origin pair/overlap, killed _BOSS, started EnemyDead exactly once, mapjumped to M0001_00_02, started destination Init, and displayed Will dialogue; nonlethal boundary path contained no EnemyDead start; wave selftest was 24/24.
