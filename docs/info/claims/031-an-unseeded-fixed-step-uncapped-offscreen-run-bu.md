---
id: C031
kind: claim
status: holds
created: 2026-08-14
tags:
depends: src/host/story_driver.cpp, src/host/interaction.cpp, src/host/navigation.cpp, src/host/main.cpp, src/engine/event_box.cpp, src/engine/script.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 12:17:16
---

## Claim

An unseeded fixed-step uncapped offscreen run buys and equips Motie's Keyring item 18 in slot 4, equips Silver Key item 30 in slot 5, consumes Keyring 18 at the Hydra main-route door, completes the recovery spring, and reaches M0013_09_00 with zero decoded audio frames.

## Evidence

tools/verify.sh passed; scratch/logs/silver-key-story.log records the Keyring purchase, both slots, Keyring use, M0013_09_00 semantic stop at frame 15325, offscreen driver, and 0 sounds / 0 frames.

## What would falsify it

A continuous unseeded shipping run fails to reach M0013_09_00, does not preserve the two authored keys in distinct slots, consumes Silver Key 30 at that door, creates a desktop window, or decodes any audio frame.

## Re-confirmed 2026-08-14

Commit af6e3c5 passed the complete offscreen/dummy-audio fixed-step tools/verify.sh gate; scratch/logs/silver-key-story.log reached M0013_09_00 at frame 15325 with both authored key slots and zero decoded audio frames.

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
