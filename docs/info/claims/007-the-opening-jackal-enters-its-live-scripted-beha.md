---
id: C007
kind: claim
status: holds
created: 2026-08-13
tags: bosses,scripting,combat
depends: src/engine/script.cpp#Dispatch, src/engine/script.h#motion_duration, src/engine/world.cpp#TickLookTargets, src/engine/world.cpp#TickScriptMoves, src/host/main.cpp#resolveMotionDuration, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 15:04:24
---

## Claim

The opening Jackal enters its live scripted behavior loop and moves under the map's _BOSS coroutine.

## Evidence

The shipping 600-frame gate logs scripted movement beginning for _BOSS; a 900-frame trace changes closest distance from the formerly fixed 105.0 to 70.8; movement selftest covers 13 math/status/motion/movement cases; full verify.sh passes.

## What would falsify it

Any change to script HP slots, math_LerpSin/math_atan2/bit_and, ChrLookTarget, synchronous motion duration lookup, scripted movement accounting, or an opening run without _BOSS movement.

## Re-confirmed 2026-08-13

Shipping 600-frame gate logged _BOSS scripted movement; 900-frame closest distance changed 105.0 to 70.8; SELFTEST 13/13 and full verify.sh passed.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; the opening Jackal entered its scripted loop, moved, collided with the arena wall, and executed live attack phases.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; scripted boss movement and live attack path passed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after strict parser and cmd-API instrumentation; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after exact MPK corpus identity gating; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after mandatory runtime preflight; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full read-only tools/verify.sh passed on 2026-08-13; live scripted Jackal behavior remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after boss-death progression; live scripted Jackal behavior and its death coroutine both ran.

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

## Re-confirmed 2026-08-14

Reverified after commits adedb5d/addb897 by full ./tools/verify.sh: scratch/logs/verify-sdl3-gpu-negative.log ALL PARSERS PASSED on 2026-08-14; mandatory story remained fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.

## Re-confirmed 2026-08-14

Reverified after commit f5b4048 by full ./tools/verify.sh: scratch/logs/verify-sdl3-gpu-pipeline-final.log ALL PARSERS PASSED on 2026-08-14; mandatory story remained fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.

## Re-confirmed 2026-08-14

Reverified after commit bff6051 by the complete mandatory ./tools/verify.sh run in scratch/logs/verify-sdl3-gpu-assets-final.log on 2026-08-14; ALL PARSERS PASSED, the continuous story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, and audio decoded 0 sounds / 0 frames.

## Re-confirmed 2026-08-14

Reverified after commit 7a2b1ed by the complete tools/verify.sh gate in scratch/logs/verify-sdl3-gpu-skinning-final.log: all focused positives and negatives passed, the unseeded story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, audio decoded 0 frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after commit 9dea2fe by the complete tools/verify.sh gate in scratch/logs/verify-sdl3-gpu-scene-final.log: all focused positives and negatives passed, the SDL3 GPU shipping scene wrote its checked capture, the unseeded story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, audio decoded 0 frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after commit 7b7e22c by the complete tools/verify.sh gate in scratch/logs/verify-render-snapshot-final.log: all focused positives and negatives passed, CameraTracker reported 7/7 and RenderSnapshot 3/3, both 21961-frame story runs stayed fixed-step uncapped/offscreen with 0 audio frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after b07f16a and the external-pass changes by the complete tools/verify.sh gate in scratch/logs/verify-external-gpu-pass-final.log: all focused positives and negatives passed, both 21,961-frame story runs stayed fixed-step uncapped/offscreen with 0 audio frames, the live SDL3 pair and external-pass parity passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 3bee232 by complete tools/verify.sh in scratch/logs/verify-sdl3-fade-final.log: all focused positives and negatives passed, 21/21 portable shaders regenerated, the 0.500 scene/fade pair stayed offscreen with 0 audio frames, both 21,961-frame story runs passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 51cc938 by complete tools/verify.sh in scratch/logs/verify-directional-shading-51cc938.log: generated-normal positives/negatives, directional/equal-ambient static and skinned controls, distinct A/B captures, 21/21 portable shaders, both 21,961-frame story runs offscreen with zero audio frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact source tree committed as abd18ce: all focused positives/negatives, unpaced offscreen story through sccnt=20 with zero audio frames, room census, API/frontier/table checks, and ALL PARSERS PASSED

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact source tree committed as 088030a: structure gate 81 files/0 violations, 33/33 shader artifacts reproduced exactly, SDL3 GPU title omission controls measured sprite and text contributions, running ModeTitle completed offscreen with zero decoded audio frames, uncapped story verification reached sccnt=20, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact shared-frame-compositor tree: 87 source files/0 structure violations, 33/33 shaders, live offscreen GLES/SDL3 scene UI fade pair with zero audio, uncapped story through sccnt=20, and ALL PARSERS PASSED.
