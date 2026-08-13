---
id: 19
title: Opening boss death never advanced the story
status: resolved
symptom: Jackal could attack, but player swings never damaged it and defeating the last enemy had no path into the room EnemyDead scene
tags: gameplay,combat,scripting,bosses,progression,diagnostics
created: 2026-08-13
updated: 2026-08-13
---

## Root causes

1. `seedCombat()` runs every frame to initialize late-spawned actors, but it also unconditionally set the existing player attack volume `valid=false`. The swing logic enabled the volume and the next frame disabled it before the middle-of-swing live window could reach collision testing. Player combat therefore tested zero volume pairs.
2. A kill immediately set every actor `alive=false`, removing script-owned bosses before their authored death presentation.
3. The host never reproduced `ModeGame::Process`'s transition from at least one live type-4 character to zero, which calls the room-global `EnemyDead`. The opening script keeps all post-fight dialogue and `mapjump(1,0,2,...)` behind that callback.

## Fix

Player attack/damage volume setup is idempotent. Boss HP zero now marks `defeated` while retaining the actor until the script calls `DeadEnemy`; ordinary enemies retain immediate removal until their native death presentation is ported. `World::ConsumeEnemyWaveCleared` observes the armed nonzero-to-zero hostile transition, fires once, and rearms for later waves. The host starts `EnemyDead` when present. The headless driver now separates authored EX_1 positioning from combat, acquires/faces the nearest hostile, closes through normal collision, and reports player-origin pairs/overlaps separately.

## Evidence

The shipping opening positive produces one player-origin pair and overlap, kills `_BOSS`, starts `EnemyDead` exactly once, runs all four post-fight messages, mapjumps to `M0001_00_02`, starts its Init coroutine, and displays `Sumo: Don't die on me, Will!`. The nonlethal 600-frame boundary run still shows Jackal attacks and map collision and contains no EnemyDead start. The wave selftest covers empty, armed-live, exactly-once, no-refire, and later-wave rearm classes; full verify passes.
