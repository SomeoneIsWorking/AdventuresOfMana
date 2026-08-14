---
id: C045
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3-gpu,shading
depends: src/host/render_normals.cpp#GenerateNormals, src/host/gpu_asset.cpp#Asset::Asset, src/host/gpu_asset_pipeline.cpp#AssetPipeline::DrawPass, src/host/render_lighting.cpp#DirectionalLight::ForModelYaw, shaders/src/textured.vert.hlsl, shaders/src/skinned.vert.hlsl, src/tools/gpu_asset_selftest.cpp#RunAssetPipelineSelfTest
reconfirmed: 2026-08-14
verified_at: 2026-08-14 14:11:09
---

## Claim

SDL3 GPU derives surface normals and applies directional shading to static and skinned shipping models

## Evidence

Full tools/verify.sh pass in scratch/logs/verify-directional-shading.log: 16/32-bit normal positives and degenerate negatives pass; M0001_00_00 derives 1,094 triangles with 0 degenerate/unsupported vertices and enhanced differs from equal ambient in 3,675 pixels; C0000_00 differs from equal ambient in 2,750; A/B captures are distinct; both story runs remain offscreen with 0 audio frames.

## What would falsify it

Changes to render_normals, RenderAsset normal ownership, GPU vertex layout, textured/skinned shaders, DirectionalLight defaults, shader compilation, or the SDL3 shipping-asset selftest

## Re-confirmed 2026-08-14

Reverified at implementation commit 51cc938 by complete tools/verify.sh in scratch/logs/verify-directional-shading-51cc938.log: generated-normal positives/negatives, directional/equal-ambient static and skinned controls, distinct A/B captures, 21/21 portable shaders, both 21,961-frame story runs offscreen with zero audio frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact source tree committed as abd18ce: all focused positives/negatives, unpaced offscreen story through sccnt=20 with zero audio frames, room census, API/frontier/table checks, and ALL PARSERS PASSED
