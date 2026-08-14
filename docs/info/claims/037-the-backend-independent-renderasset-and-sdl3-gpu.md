---
id: C037
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,gpu,assets,headless
depends: src/host/render_asset.cpp#LoadRenderAsset, src/host/gpu_asset.cpp#Asset::Asset, src/host/gpu_asset_pipeline.cpp#RunAssetPipelineSelfTest, tools/verify.sh
---

## Claim

The backend-independent RenderAsset and SDL3 GPU asset pipeline load shipping room M0001_00_00, upload its vertex/index data and material textures, and submit all 12 draw ranges offscreen without initializing audio or creating a window.

## Evidence

Commit bff6051; full tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-assets-final.log. The positive scanned 6912 pixels, changed 3868 with 202 red-value classes, and all 3868 differed from a forced-white control; no-draw changed 0. Story remained fixed-step uncapped/offscreen with 0 decoded audio frames.

## What would falsify it

if a shipping-asset run creates an SDL window or initializes/decodes audio, any draw range or material texture is not uploaded/submitted, the textured output matches forced-white, the no-draw class changes a pixel, or the mandatory verifier omits these checks
