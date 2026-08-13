---
id: C009
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/host/main.cpp#seedCombat, src/engine/world.cpp#World::ConsumeEnemyWaveCleared, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 02:39:17
---

## Claim

The first Jackal fight advances through EnemyDead into Will's room

## Evidence

2026-08-13 full tools/verify.sh: opening positive records MainPlayer killed _BOSS, exactly one EnemyDead coroutine start, mapjump to M0001_00_02, destination Init, and Will dialogue; nonlethal boundary negative has no EnemyDead start; movement selftest 24/24 covers five wave-transition states.

## What would falsify it

Any change to player volume seeding, Actor alive/defeated lifecycle, World::ConsumeEnemyWaveCleared, combat kill handling, coroutine start, mapjump, or the opening death verifier.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13: lethal opening path recorded one player-origin pair/overlap, killed _BOSS, started EnemyDead exactly once, mapjumped to M0001_00_02, started destination Init, and displayed Will dialogue; nonlethal boundary path contained no EnemyDead start; wave selftest was 24/24.

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

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after persistent binary-named party identities and the sccnt=12-to-14 return route landed; all continuous story gates, focused self-tests, negative discriminators, exact asset corpus checks, and the 993-room census passed. The newest gate restored live PARTY_HEROINE across nine room loads and settled at sccnt=14 after 5202 offscreen fixed-step frames with zero decoded audio.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 with the repository-owned RE-frontier validator and its zero-entry negative enabled. All continuous story gates, self-tests, negative discriminators, exact 9886-member corpus checks, and 993-room census passed; PARTY_HEROINE remained live at settled sccnt=14 after 5202 offscreen frames with zero decoded audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the windowless render bypass, OpenDoor, inventory bridge, and Matock chest changes; all gameplay, self-test, corpus, frontier, and generated-artifact gates passed.
