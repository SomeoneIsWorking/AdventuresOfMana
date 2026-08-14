---
id: C034
kind: claim
status: holds
created: 2026-08-14
tags: progression,kett,lua,headless
depends: src/host/story_driver.cpp#StoryDriver::Target, src/engine/script.cpp#Script::StartTreasureCallback, src/host/main.cpp#main, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 12:37:34
---

## Claim

An unseeded fixed-step uncapped offscreen run continues after Steward Wolf through Kett's authored hidden-floor route, spends a third Keyring use, opens the Chain Flail chest with item 104, and reaches settled sccnt=20 at frame 21961 with zero decoded audio frames

## Evidence

Commit 6e5d104; full ./tools/verify.sh passed in scratch/logs/verify-chain-flail.log on 2026-08-14; scratch/logs/silver-key-story.log records SDL offscreen, item 104, sccnt=20, frame 21961, and 0 sounds / 0 frames; treasure callback selftest proves handler-present and handler-absent cases

## What would falsify it

if an unseeded mandatory run fails to reach sccnt=20, does not acquire item 104 through the live box, opens a window or audio device, or any callback selftest class fails

## Re-confirmed 2026-08-14

Tracked baseline established after commit 73f540a; behavior was verified by full ./tools/verify.sh in scratch/logs/verify-chain-flail.log on 2026-08-14 with ALL PARSERS PASSED, SDL offscreen, sccnt=20 at frame 21961, and zero decoded audio frames.

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
