---
id: C018
kind: claim
status: holds
created: 2026-08-14
tags: progression,inventory,tooling
depends: src/engine/script.cpp, src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 11:53:28
---

## Claim

The unseeded story acquires Matock item 17 from Bogard's authored chest through live AddBox/OpenDoor/inventory semantics in a renderless offscreen run.

## Evidence

tools/verify.sh continuous Matock chest gate: room exit into M0010_00_00, opened box and acquired item 17, settled requested item 17, offscreen driver, zero decoded audio.

## What would falsify it

Any mandatory run fails to enter M0010_00_00, fails to fire _BOX/acquire item 17, decodes audio, or uses a visible/rendered non-capture path.

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
