---
id: C016
kind: claim
status: holds
created: 2026-08-14
tags:gameplay,progression,verification
depends: src/host/main.cpp#main, src/engine/script.cpp#Dispatch, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 02:39:18
---

## Claim

An unseeded new game continuously completes the heroine encounter at scenario state 12

## Evidence

2026-08-14 full ./tools/verify.sh pass: offscreen fixed-step --opening-story --continue-story run completed in 4124 frames, traversed five WALL_UP and three WALL_DN pairs, spawned and engine-placed three Myconids through AddEnemyZaco, seeded all three from enemydat.bin, killed all three, fired EnemyDead, completed Hasim dialogue, and stopped settled at sccnt=12 with zero live coroutines and zero decoded audio frames.

## What would falsify it

Falsified if the mandatory heroine gate no longer starts from an unseeded new game, does not observe exactly five upward and three downward vine traversals, does not create/place/seed/kill all three Myconids and fire EnemyDead, fails to settle at sccnt=12, opens a visible window, or decodes audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after inverse-vine routing, AddEnemyZaco, repeatable late placement, and offscreen story-gate changes; all focused self-tests, negative discriminators, continuous story gates, exact asset corpus, and room census passed. The heroine gate settled at sccnt=12 after five WALL_UP and three WALL_DN traversals with zero decoded audio frames.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after persistent binary-named party identities and the sccnt=12-to-14 return route landed; all continuous story gates, focused self-tests, negative discriminators, exact asset corpus checks, and the 993-room census passed. The newest gate restored live PARTY_HEROINE across nine room loads and settled at sccnt=14 after 5202 offscreen fixed-step frames with zero decoded audio.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 with the repository-owned RE-frontier validator and its zero-entry negative enabled. All continuous story gates, self-tests, negative discriminators, exact 9886-member corpus checks, and 993-room census passed; PARTY_HEROINE remained live at settled sccnt=14 after 5202 offscreen frames with zero decoded audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the windowless render bypass, OpenDoor, inventory bridge, and Matock chest changes; all gameplay, self-test, corpus, frontier, and generated-artifact gates passed.
