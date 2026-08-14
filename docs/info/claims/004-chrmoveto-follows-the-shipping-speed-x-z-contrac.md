---
id: C004
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/engine/world.h#Actor, src/engine/world.cpp#World::TickScriptMoves, src/engine/script.cpp#Dispatch, src/host/main.cpp#main, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 10:18:29
---

## Claim

ChrMoveTo follows the shipping speed/x/z contract and its coroutine-visible automatic movement advances over time instead of completing immediately.

## Evidence

Wrapper 0x2cb550 forwards speed,x,currentY,z to ChrMoveYTo; --movement-selftest verifies a 3-4-5 path at half and full duration, active/inactive polling, Y preservation, and speed-zero look-at; opening screenshots show hero at frame 30 and Jackal at frame 60; full verify.sh passes.

## What would falsify it

Any change to Actor scripted movement state, ChrMoveTo/ChrMoveYTo/IsChrAutoMove dispatch, gameplay ticking/player synchronization, or movement verifier; or opening actors ceasing to enter at the measured frames.

## Re-confirmed 2026-08-13

Re-ran full verify.sh with movement SELFTEST 6/6 and visually inspected the frame-30 hero and frame-60 Jackal opening captures.

## Re-confirmed 2026-08-13

Full verify.sh re-ran movement SELFTEST 6/6 and the opening shipping path after the boss-AI ownership change.

## Re-confirmed 2026-08-13

Full verify.sh re-ran movement/motion SELFTEST 9/9 and the opening shipping path after per-actor motion changes.

## Re-confirmed 2026-08-13

Final full verify.sh re-ran movement/motion SELFTEST 9/9 and the opening shipping path.

## Re-confirmed 2026-08-13

Full verify.sh re-ran movement SELFTEST 13/13 including exact path distance accounting.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; movement selftest reported 19 cases and 0 failures, including blocked scripted movement and ISHITMAP behavior.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; movement selftest remained 19/19.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; movement selftest remained 19/19.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after strict parser and cmd-API instrumentation; movement selftest remained 19/19.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after exact MPK corpus identity gating; movement selftest remained 19/19.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after mandatory runtime preflight; movement selftest remained 19/19.

## Re-confirmed 2026-08-13

Full read-only tools/verify.sh passed on 2026-08-13; movement selftest remained 19/19.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after boss-death progression; movement/wave selftest reported 24/24.

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

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the post-Matock route-planner fix; all continuous fixed-step offscreen gates, focused self-tests, negative discriminators, exact asset corpus checks, the 993-room census, and the new 6279-frame silent route to M0000_10_06 passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed 2026-08-14 after replacing the false Chocobot-detour gate: all existing discriminators passed, and the corrected unseeded continuation reached M0011_00_02 in 6785 fixed-step uncapped offscreen frames with zero gameplay audio decode.

## Re-confirmed 2026-08-14

Reconfirmed against pushed commit 61c7781; its pre-commit full ./tools/verify.sh pass exercised the identical tree, including all existing discriminators and the corrected 6785-frame offscreen silent M0011_00_02 continuation.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Silver Key doorway changes, including all asset/parser negative controls, gameplay gates, selftests, 993-room census, cmd API, world map, and the continuous offscreen/no-audio progression through M0013_03_01.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 after removing unused route branches; all parser negatives, gameplay gates, selftests, 993-room census, command/world-map checks, and continuous offscreen/no-audio progression through M0013_03_01 passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra-dungeon pressure-switch changes, including all negatives, gameplay gates through M0013_00_04, 48/48 movement cases, 993-room census, cmd API, and world-map checks.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra mountain change; all claim-specific self-tests and continuous fixed-step offscreen zero-audio progression gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the verifier was made to rebuild mana first; 62 inventory cases and all continuous offscreen zero-audio gates passed.

## Re-confirmed 2026-08-14

Full offscreen/dummy-audio fixed-step gate tools/verify.sh passed on 2026-08-14; scratch/logs/verify-keyring-structure-v4.log ends VERIFICATION OK.

## Re-confirmed 2026-08-14

Commit af6e3c5 passed the complete offscreen/dummy-audio fixed-step tools/verify.sh gate on 2026-08-14; gameplay and static/corpus evidence is recorded under scratch/logs/.
