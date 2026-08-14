---
id: C045
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3-gpu,shading
depends: src/host/render_normals.cpp#GenerateNormals, src/host/gpu_asset.cpp#Asset::Asset, src/host/gpu_asset_pipeline.cpp#AssetPipeline::DrawPass, src/host/render_lighting.cpp#DirectionalLight::ForModelYaw, shaders/src/textured.vert.hlsl, shaders/src/skinned.vert.hlsl, src/tools/gpu_asset_selftest.cpp#RunAssetPipelineSelfTest
---

## Claim

SDL3 GPU derives surface normals and applies directional shading to static and skinned shipping models

## Evidence

Full tools/verify.sh pass in scratch/logs/verify-directional-shading.log: 16/32-bit normal positives and degenerate negatives pass; M0001_00_00 derives 1,094 triangles with 0 degenerate/unsupported vertices and enhanced differs from equal ambient in 3,675 pixels; C0000_00 differs from equal ambient in 2,750; A/B captures are distinct; both story runs remain offscreen with 0 audio frames.

## What would falsify it

Changes to render_normals, RenderAsset normal ownership, GPU vertex layout, textured/skinned shaders, DirectionalLight defaults, shader compilation, or the SDL3 shipping-asset selftest
