---
id: C038
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,gpu,depth,blending
depends: src/host/gpu_device.cpp#Device::RenderAndReadback, src/host/gpu_asset_pipeline.cpp#AssetPipeline::Draw, src/tools/gpu_asset_selftest.cpp#RunAssetPipelineSelfTest, tools/verify.sh
---

## Claim

The SDL3 GPU static-asset pipeline accepts an explicit camera transform, depth-tests opaque geometry, preserves opaque depth while drawing authored blended materials without depth writes, and produces distinct shipping-room outputs for both disabled controls.

## Evidence

Commit 8481aa2; full tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-depth-blend-final.log on 2026-08-14. M0001 depth control differed in 3868/6912 pixels only when depth was disabled; M0000_00_03's two blended draws differed in 128/6912 pixels from opaque control; all runs were SDL-offscreen and audio remained disabled.

## What would falsify it

if the depth-enabled far redraw changes a pixel, the depth-disabled control changes none, the authored-water blend output matches opaque control, a required GPU run creates a window or initializes audio, or the mandatory verifier omits these checks
