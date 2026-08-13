---
id: C002
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/host/main.cpp#main, src/engine/script.cpp#Script::ResumeCoroutines, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 02:39:17
---

## Claim

The default new-game room executes its opening Init sequence through the shipping host path: time advances past wait(600), Arena Guard dialogue appears, and Jackal is seeded from enemydat.bin.

## Evidence

SDL_AUDIODRIVER=dummy ./tools/verify.sh: opening-room lifecycle requires the Init-start, dialogue, and one-enemy-stat log lines after 600 fixed frames; M0000_00_00 matched 0/3 of the same assertions.

## What would falsify it

Any change to room loading, Script coroutine scheduling/game time, AddBoss naming, combat seeding, or the lifecycle verifier; or a 600-frame M0001_00_00 run missing any of the three required observations.

## Re-confirmed 2026-08-13

Re-ran the positive shipping path: M0001_00_00 matched 3/3 assertions after 600 frames. Ran M0000_00_00 as the negative class: 0/3 assertions matched.

## Re-confirmed 2026-08-13

Full SDL_AUDIODRIVER=dummy ./tools/verify.sh passed after camera command integration; opening lifecycle still matched all three required observations.

## Re-confirmed 2026-08-13

Full verify.sh passed after asynchronous scripted movement; the opening lifecycle still produced all three required observations.

## Re-confirmed 2026-08-13

Full verify.sh re-ran the 600-frame opening lifecycle after the boss-AI ownership change and matched Init start, Arena Guard dialogue, and Jackal combat seeding.

## Re-confirmed 2026-08-13

Full verify.sh re-ran the strengthened 600-frame opening gate and observed Init, dialogue, combat seeding, late boss asset load, and BGM 2.

## Re-confirmed 2026-08-13

Final full verify.sh re-ran the strengthened opening gate after decoupling motion resolution from rendering.

## Re-confirmed 2026-08-13

Full verify.sh passed the strengthened opening gate through live _BOSS scripted movement.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; opening lifecycle reached the late boss behavior and completed the new attack-path boundary without coroutine failures.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; opening lifecycle reached dialogue, late boss spawn, BGM 2, movement, and attack.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; opening lifecycle retained all required observations.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after strict parser and cmd-API instrumentation; opening lifecycle retained every required observation.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after exact MPK corpus identity gating; opening lifecycle retained every required observation.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after mandatory runtime preflight; opening lifecycle retained every required observation.

## Re-confirmed 2026-08-13

Full read-only tools/verify.sh passed on 2026-08-13 with mandatory game/assets/source inputs; opening lifecycle retained every required observation.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after boss-death progression; opening lifecycle and the subsequent Will-room transition both passed.

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
