---
id: C032
kind: claim
status: holds
created: 2026-08-14
tags:
depends: src/host/story_driver.cpp, src/host/navigation.cpp, src/host/main.cpp, src/engine/script.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 12:17:16
---

## Claim

An unseeded fixed-step uncapped offscreen run spends Keyring uses at both Hydra KEY doors, presses both hidden-stair switches, reaches and defeats Hydra through the live boss script, and settles at sccnt=16 with zero decoded audio frames.

## Evidence

tools/verify.sh passed in scratch/logs/verify-hydra-defeat.log; it records sw_01/sw_02/down_1, the M0013_08_04 crossing with no per-waypoint rebuild, Hydra model load and kill, the victory message, sccnt=16, offscreen video, and 0 sounds / 0 frames.

## What would falsify it

A continuous unseeded shipping run fails to settle at sccnt=16, bypasses either authored switch or EnemyDead, regains per-waypoint M0013_08_04 route rebuilds, creates a desktop window, or decodes any audio frame.

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
