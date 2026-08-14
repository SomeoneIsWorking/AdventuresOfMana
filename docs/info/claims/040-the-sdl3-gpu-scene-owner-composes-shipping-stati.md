---
id: C040
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,scene
depends: src/host/gpu_scene.cpp#SceneRenderer::DrawAndReadback, src/host/gpu_asset_pipeline.cpp#AssetPipeline::DrawPass, src/tools/gpu_asset_selftest.cpp#RunAssetPipelineSelfTest, src/host/image_write.cpp#WritePng, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 12:54:26
---

## Claim

The SDL3 GPU scene owner composes shipping static and skinned assets under one camera and depth target with scene-wide opaque-before-blended ordering.

## Evidence

Commit 9dea2fe: M0001_00_00 plus C0000_00 changes 88/76800 pixels from room-only, the off-frustum actor changes 0, and 748/6912 pixels discriminate scene-wide material ordering from per-asset ordering; full verify passes and writes a checked PNG.

## What would falsify it

Any change to scene submission order, material-pass routing, camera transforms, depth target ownership, shipping room/actor composition, PNG capture verification, or the centered/off-frustum/order discriminators; or a running-world snapshot that cannot be represented by SceneDraw.

## Re-confirmed 2026-08-14

Reverified after commit 9dea2fe by the complete tools/verify.sh gate in scratch/logs/verify-sdl3-gpu-scene-final.log: all focused positives and negatives passed, the SDL3 GPU shipping scene wrote its checked capture, the unseeded story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, audio decoded 0 frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after commit 7b7e22c by the complete tools/verify.sh gate in scratch/logs/verify-render-snapshot-final.log: all focused positives and negatives passed, CameraTracker reported 7/7 and RenderSnapshot 3/3, both 21961-frame story runs stayed fixed-step uncapped/offscreen with 0 audio frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after b07f16a and the external-pass changes by the complete tools/verify.sh gate in scratch/logs/verify-external-gpu-pass-final.log: all focused positives and negatives passed, both 21,961-frame story runs stayed fixed-step uncapped/offscreen with 0 audio frames, the live SDL3 pair and external-pass parity passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 180133b by the complete tools/verify.sh gate in scratch/logs/verify-external-gpu-pass-final.log: external target parity 0/76800, missing-target negative passed, live pair stayed offscreen with 0 audio frames, both 21,961-frame story runs passed, and ALL PARSERS PASSED.
