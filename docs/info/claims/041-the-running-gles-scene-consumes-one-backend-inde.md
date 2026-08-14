---
id: C041
kind: claim
status: holds
created: 2026-08-14
tags: renderer,snapshot,camera
depends: src/host/render_camera.cpp#CameraTracker::Update, src/host/render_snapshot.cpp#RenderSnapshot::Add, src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 13:07:21
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
