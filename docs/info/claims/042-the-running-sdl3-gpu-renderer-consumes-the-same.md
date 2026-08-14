---
id: C042
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3-gpu,snapshot
depends: src/host/gpu_snapshot_renderer.cpp, src/host/scene_pair_capture.cpp, src/host/main.cpp
reconfirmed: 2026-08-14
verified_at: 2026-08-14 14:42:04
---

## Claim

The running SDL3 GPU renderer consumes the same resolved RenderSnapshot as GLES and produces a same-frame offscreen scene capture with three shipping instances, including two skinned actors, while audio remains entirely disabled

## Evidence

tools/verify.sh in scratch/logs/verify-live-snapshot-final.log: snapshot adapter differs from the manual SDL3 scene in 0/76800 pixels; the live frame-30 pair reports 3 instances, 2 skinned, 3 cached assets, offscreen video, two nonempty PNGs, 0 decoded audio frames, and exit 0. The two captures were visually inspected and have aligned camera, geometry, actors, and authored textures.

## What would falsify it

A shipping paired run fails to render every snapshot instance, produces a structurally different camera/scene, initializes or decodes audio, creates a visible window, or cannot exit cleanly

## Re-confirmed 2026-08-14

Reverified after registry creation by the complete tools/verify.sh gate in scratch/logs/verify-live-snapshot-final.log: adapter parity 0/76800, live frame 30 contains 3 instances / 2 skinned / 3 cached, offscreen captures are nonempty, audio decoded 0 frames, clean exit, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after b07f16a and the external-pass changes by the complete tools/verify.sh gate in scratch/logs/verify-external-gpu-pass-final.log: all focused positives and negatives passed, both 21,961-frame story runs stayed fixed-step uncapped/offscreen with 0 audio frames, the live SDL3 pair and external-pass parity passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 180133b by the complete tools/verify.sh gate in scratch/logs/verify-external-gpu-pass-final.log: external target parity 0/76800, missing-target negative passed, live pair stayed offscreen with 0 audio frames, both 21,961-frame story runs passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 3bee232 by complete tools/verify.sh in scratch/logs/verify-sdl3-fade-final.log: all focused positives and negatives passed, 21/21 portable shaders regenerated, the 0.500 scene/fade pair stayed offscreen with 0 audio frames, both 21,961-frame story runs passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 51cc938 by complete tools/verify.sh in scratch/logs/verify-directional-shading-51cc938.log: generated-normal positives/negatives, directional/equal-ambient static and skinned controls, distinct A/B captures, 21/21 portable shaders, both 21,961-frame story runs offscreen with zero audio frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact source tree committed as abd18ce: all focused positives/negatives, unpaced offscreen story through sccnt=20 with zero audio frames, room census, API/frontier/table checks, and ALL PARSERS PASSED

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact source tree committed as 088030a: structure gate 81 files/0 violations, 33/33 shader artifacts reproduced exactly, SDL3 GPU title omission controls measured sprite and text contributions, running ModeTitle completed offscreen with zero decoded audio frames, uncapped story verification reached sccnt=20, and ALL PARSERS PASSED.
