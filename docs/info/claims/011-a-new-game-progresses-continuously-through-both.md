---
id: C011
kind: claim
status: holds
created: 2026-08-13
tags: gameplay,progression,cutscenes
depends: src/engine/script.cpp#ClearRoomScript, src/engine/script.cpp#GlobalNumber, src/host/main.cpp#opening_story, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 11:07:54
---

## Claim

A new game progresses continuously through both opening Jackal fights and the escape into M0001_01_03 with scenario count 4

## Evidence

2026-08-13 --opening-story live run killed exactly two _BOSS actors, showed distinct second-fight intro and escape dialogue, completed Will's scene, crossed FREE and ordinary edges, entered out_01, stopped in M0001_01_03, and logged sccnt=4; mandatory verifier carries the same assertions

## What would falsify it

Any change to Lua coroutine/global lifetime, input gating, fades, room transitions, door/edge traversal, combat death progression, event boxes, opening scripts, story driver, or its verifier assertions

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

## Re-confirmed 2026-08-14

Commit fe20918 passed the complete fixed-step uncapped offscreen/dummy-audio tools/verify.sh gate through Hydra's authored sccnt=16 defeat transition on 2026-08-14; evidence is scratch/logs/verify-hydra-defeat.log.

## Re-confirmed 2026-08-14

Commit a80ff75: the complete mandatory ./tools/verify.sh passed in scratch/logs/verify-steward-wolf.log after the landed changes, exercising this claim's positive and negative checks offscreen; the extended continuous run settled at sccnt=19 with zero decoded audio frames.

## Re-confirmed 2026-08-14

Reverified unchanged behavior after commit 6e5d104 by full ./tools/verify.sh: scratch/logs/verify-chain-flail.log ALL PARSERS PASSED on 2026-08-14; mandatory story run was fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.
