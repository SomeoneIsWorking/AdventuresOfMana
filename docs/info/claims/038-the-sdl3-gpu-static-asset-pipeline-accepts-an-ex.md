---
id: C038
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,gpu,depth,blending
depends: src/host/gpu_device.cpp#Device::RenderAndReadback, src/host/gpu_asset_pipeline.cpp#AssetPipeline::Draw, src/tools/gpu_asset_selftest.cpp#RunAssetPipelineSelfTest, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 13:07:21
---

## Claim

The SDL3 GPU static-asset pipeline accepts an explicit camera transform, depth-tests opaque geometry, preserves opaque depth while drawing authored blended materials without depth writes, and produces distinct shipping-room outputs for both disabled controls.

## Evidence

Commit 8481aa2; full tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-depth-blend-final.log on 2026-08-14. M0001 depth control differed in 3868/6912 pixels only when depth was disabled; M0000_00_03's two blended draws differed in 128/6912 pixels from opaque control; all runs were SDL-offscreen and audio remained disabled.

## What would falsify it

if the depth-enabled far redraw changes a pixel, the depth-disabled control changes none, the authored-water blend output matches opaque control, a required GPU run creates a window or initializes audio, or the mandatory verifier omits these checks

## Re-confirmed 2026-08-14

Tracked baseline established after commit 065f52a; the complete mandatory tools/verify.sh run remains scratch/logs/verify-sdl3-gpu-depth-blend-final.log with 0 depth-enabled differences, 3868 depth-disabled differences, 128 blend-control differences, offscreen video, zero decoded audio, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after commit 7a2b1ed by the complete tools/verify.sh gate in scratch/logs/verify-sdl3-gpu-skinning-final.log: all focused positives and negatives passed, the unseeded story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, audio decoded 0 frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after commit 9dea2fe by the complete tools/verify.sh gate in scratch/logs/verify-sdl3-gpu-scene-final.log: all focused positives and negatives passed, the SDL3 GPU shipping scene wrote its checked capture, the unseeded story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, audio decoded 0 frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after commit 7b7e22c by the complete tools/verify.sh gate in scratch/logs/verify-render-snapshot-final.log: all focused positives and negatives passed, CameraTracker reported 7/7 and RenderSnapshot 3/3, both 21961-frame story runs stayed fixed-step uncapped/offscreen with 0 audio frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after b07f16a and the external-pass changes by the complete tools/verify.sh gate in scratch/logs/verify-external-gpu-pass-final.log: all focused positives and negatives passed, both 21,961-frame story runs stayed fixed-step uncapped/offscreen with 0 audio frames, the live SDL3 pair and external-pass parity passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 180133b by the complete tools/verify.sh gate in scratch/logs/verify-external-gpu-pass-final.log: external target parity 0/76800, missing-target negative passed, live pair stayed offscreen with 0 audio frames, both 21,961-frame story runs passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 3bee232 by complete tools/verify.sh in scratch/logs/verify-sdl3-fade-final.log: all focused positives and negatives passed, 21/21 portable shaders regenerated, the 0.500 scene/fade pair stayed offscreen with 0 audio frames, both 21,961-frame story runs passed, and ALL PARSERS PASSED.
