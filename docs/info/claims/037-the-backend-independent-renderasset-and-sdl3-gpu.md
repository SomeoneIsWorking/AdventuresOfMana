---
id: C037
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,gpu,assets,headless
depends: src/host/render_asset.cpp#LoadRenderAsset, src/host/gpu_asset.cpp#Asset::Asset, src/tools/gpu_asset_selftest.cpp#RunAssetPipelineSelfTest, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 13:07:21
---

## Claim

The backend-independent RenderAsset and SDL3 GPU asset pipeline load shipping room M0001_00_00, upload its vertex/index data and material textures, and submit all 12 draw ranges offscreen without initializing audio or creating a window.

## Evidence

Commit bff6051; full tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-assets-final.log. The positive scanned 6912 pixels, changed 3868 with 202 red-value classes, and all 3868 differed from a forced-white control; no-draw changed 0. Story remained fixed-step uncapped/offscreen with 0 decoded audio frames.

## What would falsify it

if a shipping-asset run creates an SDL window or initializes/decodes audio, any draw range or material texture is not uploaded/submitted, the textured output matches forced-white, the no-draw class changes a pixel, or the mandatory verifier omits these checks

## Re-confirmed 2026-08-14

Reverified after commit 8481aa2 by full tools/verify.sh in scratch/logs/verify-sdl3-gpu-depth-blend-final.log: M0001_00_00 uploaded 12 draws, 3868 textured pixels differed from forced-white, no-draw changed 0, depth control discriminated 3868 pixels, and ALL PARSERS PASSED.

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
