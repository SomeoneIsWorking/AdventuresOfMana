---
id: C033
kind: claim
status: holds
created: 2026-08-14
tags: progression,hydra,kett,verification
depends: src/host/story_driver.cpp#StoryDriver::Target, src/host/story_driver.cpp#StoryDriver::EquipAcquiredSubItem, src/host/render.cpp#ActorModelName, src/host/main.cpp#main, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 11:34:23
---

## Claim

An unseeded new game continuously defeats Hydra, opens Fire then Mirror, runs AfterBossEvent, returns to Kett, exposes and defeats Steward Wolf, and settles at scenario state 19

## Evidence

Commit a80ff75: ./tools/verify.sh passed; its one fixed-step uncapped SDL-offscreen run logged Fire item 505, equipped Mirror 31, AfterBossEvent mapjump, south/west Kett return, enemy123_1 loaded as B0023_00, its defeat, settled sccnt=19 at frame 19546, and audio decoded 0 sounds / 0 frames.

## What would falsify it

Any mandatory continuous run from a new game that fails to reach settled sccnt=19 through those authored transitions, opens the rewards out of order, loads enemy123_1 with a model other than B0023_00, creates a window, decodes audio, or requires seeded story state.

## Re-confirmed 2026-08-14

Commit a80ff75: the complete mandatory ./tools/verify.sh passed in scratch/logs/verify-steward-wolf.log after the landed changes, exercising this claim's positive and negative checks offscreen; the extended continuous run settled at sccnt=19 with zero decoded audio frames.

## Re-confirmed 2026-08-14

Reverified unchanged behavior after commit 6e5d104 by full ./tools/verify.sh: scratch/logs/verify-chain-flail.log ALL PARSERS PASSED on 2026-08-14; mandatory story run was fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.

## Re-confirmed 2026-08-14

Reverified after commits adedb5d/addb897 by full ./tools/verify.sh: scratch/logs/verify-sdl3-gpu-negative.log ALL PARSERS PASSED on 2026-08-14; mandatory story remained fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.

## Re-confirmed 2026-08-14

Reverified after commit f5b4048 by full ./tools/verify.sh: scratch/logs/verify-sdl3-gpu-pipeline-final.log ALL PARSERS PASSED on 2026-08-14; mandatory story remained fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.
