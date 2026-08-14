---
id: C035
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,gpu,tooling
depends: src/host/gpu_device.cpp#Device::ClearAndReadback, src/host/gpu_device.cpp#RunDeviceSelfTest, src/tools/gpu_selftest.cpp#main, tools/verify.sh
---

## Claim

The SDL3 GPU foundation creates a backend-selected device under SDL's offscreen video driver with zero windows and no audio initialization, clears and synchronously reads an RGBA8 target, distinguishes black from magenta across 24 pixels, and rejects all 12 pixels in its wrong-color negative

## Evidence

Commits adedb5d and addb897; full ./tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-negative.log on 2026-08-14, including the required failing negative in scratch/logs/gpu-readback-negative.log and ALL PARSERS PASSED

## What would falsify it

if either exact-color run fails, the wrong-color control succeeds or reports fewer than 12 mismatches, an SDL window exists, audio initializes, subsystem ownership leaks, or the mandatory full gate omits this test
