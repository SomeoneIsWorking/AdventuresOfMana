---
id: C041
kind: claim
status: holds
created: 2026-08-14
tags: renderer,snapshot,camera
depends: src/host/render_camera.cpp#CameraTracker::Update, src/host/render_snapshot.cpp#RenderSnapshot::Add, src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 15:04:26
---

## Claim

The running GLES scene consumes one backend-independent snapshot of resolved script camera, room, visible objects, live actors, transforms, motions, and motion times.

## Evidence

Commit 7b7e22c: CameraTracker owns target/origin/eye/interpolation resolution; RenderSnapshot owns one frame's CPU asset and instance references; GLES iterates that snapshot. 6 Lua camera, 7 tracker, and 3 snapshot cases pass, as does a real 30-frame shipping-room capture and the full verifier.

## What would falsify it

Any change to script camera commands/defaults, CameraTracker, room-transition reset, snapshot instance construction, object visibility, actor/model/motion resolution, GLES snapshot consumption, focused case counts, or a running capture that omits or misplaces room/object/actor content.

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
