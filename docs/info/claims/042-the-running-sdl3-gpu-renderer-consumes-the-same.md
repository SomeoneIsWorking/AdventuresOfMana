---
id: C042
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3-gpu,snapshot
depends: src/host/gpu_snapshot_renderer.cpp, src/host/scene_pair_capture.cpp, src/host/main.cpp
reconfirmed: 2026-08-14
verified_at: 2026-08-14 12:49:13
---

## Claim

The running SDL3 GPU renderer consumes the same resolved RenderSnapshot as GLES and produces a same-frame offscreen scene capture with three shipping instances, including two skinned actors, while audio remains entirely disabled

## Evidence

tools/verify.sh in scratch/logs/verify-live-snapshot-final.log: snapshot adapter differs from the manual SDL3 scene in 0/76800 pixels; the live frame-30 pair reports 3 instances, 2 skinned, 3 cached assets, offscreen video, two nonempty PNGs, 0 decoded audio frames, and exit 0. The two captures were visually inspected and have aligned camera, geometry, actors, and authored textures.

## What would falsify it

A shipping paired run fails to render every snapshot instance, produces a structurally different camera/scene, initializes or decodes audio, creates a visible window, or cannot exit cleanly

## Re-confirmed 2026-08-14

Reverified after registry creation by the complete tools/verify.sh gate in scratch/logs/verify-live-snapshot-final.log: adapter parity 0/76800, live frame 30 contains 3 instances / 2 skinned / 3 cached, offscreen captures are nonempty, audio decoded 0 frames, clean exit, and ALL PARSERS PASSED.
