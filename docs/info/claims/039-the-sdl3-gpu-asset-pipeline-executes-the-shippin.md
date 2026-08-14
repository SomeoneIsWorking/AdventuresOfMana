---
id: C039
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,skinning
depends: src/host/render_pose.cpp#BuildJointPalette, src/host/gpu_asset_pipeline.cpp#AssetPipeline::Draw, shaders/src/skinned.vert.hlsl#main, src/tools/gpu_asset_selftest.cpp#RunAssetPipelineSelfTest, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 12:17:17
---

## Claim

The SDL3 GPU asset pipeline executes the shipping two-bone skinning formula with a backend-independent 80-bone pose palette and portable shader artifacts.

## Evidence

Commit 7a2b1ed: shipping C0000_00 renders 2812 non-clear pixels; translating every joint changes 3723 pixels; missing 960-float palette is rejected; all 15 SPIR-V/DXIL/MSL artifacts regenerate byte-exact; full verify passes.

## What would falsify it

Any change to pose construction, skinned vertex attributes, shader packing/formula, shader selection, uniform upload, shipping asset loading, or the positive/shifted/missing-palette discriminator; or a shipping actor render that ignores motion palettes.

## Re-confirmed 2026-08-14

Reverified after commit 7a2b1ed by the complete tools/verify.sh gate in scratch/logs/verify-sdl3-gpu-skinning-final.log: all focused positives and negatives passed, the unseeded story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, audio decoded 0 frames, and ALL PARSERS PASSED.
