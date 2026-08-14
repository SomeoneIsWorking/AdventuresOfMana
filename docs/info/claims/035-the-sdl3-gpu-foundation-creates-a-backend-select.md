---
id: C035
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,gpu,tooling
depends: src/host/gpu_device.cpp#Device::ClearAndReadback, src/host/gpu_device.cpp#Device::RenderAndReadback, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 12:06:42
---

## Claim

The SDL3 GPU foundation creates a backend-selected device under SDL's offscreen video driver with zero windows and no audio initialization, clears and synchronously reads an RGBA8 target, distinguishes black from magenta across 24 pixels, and rejects all 12 pixels in its wrong-color negative

## Evidence

Commits adedb5d and addb897; full ./tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-negative.log on 2026-08-14, including the required failing negative in scratch/logs/gpu-readback-negative.log and ALL PARSERS PASSED

## What would falsify it

if either exact-color run fails, the wrong-color control succeeds or reports fewer than 12 mismatches, an SDL window exists, audio initializes, subsystem ownership leaks, or the mandatory full gate omits this test

## Re-confirmed 2026-08-14

Tracked baseline established after commit 8bc169f; full ./tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-negative.log on 2026-08-14, including exact two-color readback, zero windows/audio, required 12/12 wrong-color rejection, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after commit f5b4048 by full ./tools/verify.sh: scratch/logs/verify-sdl3-gpu-pipeline-final.log ALL PARSERS PASSED on 2026-08-14; mandatory story remained fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.

## Re-confirmed 2026-08-14

Reverified after commit bff6051 by the complete mandatory ./tools/verify.sh run in scratch/logs/verify-sdl3-gpu-assets-final.log on 2026-08-14; ALL PARSERS PASSED, the continuous story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, and audio decoded 0 sounds / 0 frames.

## Re-confirmed 2026-08-14

Reverified after commit 8481aa2 by full tools/verify.sh in scratch/logs/verify-sdl3-gpu-depth-blend-final.log: offscreen Vulkan device read exact black/magenta across 24 pixels, wrong-color rejected 12/12, zero SDL windows, audio remained uninitialized, and ALL PARSERS PASSED.
