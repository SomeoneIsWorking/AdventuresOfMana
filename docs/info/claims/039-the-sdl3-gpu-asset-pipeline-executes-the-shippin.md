---
id: C039
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,skinning
depends: src/host/render_pose.cpp#BuildJointPalette, src/host/gpu_asset_pipeline.cpp#AssetPipeline::Draw, shaders/src/skinned.vert.hlsl#main, src/tools/gpu_asset_selftest.cpp#RunAssetPipelineSelfTest, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 14:11:08
---

## Claim

The SDL3 GPU asset pipeline executes the shipping two-bone skinning formula with a backend-independent 80-bone pose palette and portable shader artifacts.

## Evidence

Commit 7a2b1ed: shipping C0000_00 renders 2812 non-clear pixels; translating every joint changes 3723 pixels; missing 960-float palette is rejected; all 15 SPIR-V/DXIL/MSL artifacts regenerate byte-exact; full verify passes.

## What would falsify it

Any change to pose construction, skinned vertex attributes, shader packing/formula, shader selection, uniform upload, shipping asset loading, or the positive/shifted/missing-palette discriminator; or a shipping actor render that ignores motion palettes.

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

## Re-confirmed 2026-08-14

Reverified at implementation commit 51cc938 by complete tools/verify.sh in scratch/logs/verify-directional-shading-51cc938.log: generated-normal positives/negatives, directional/equal-ambient static and skinned controls, distinct A/B captures, 21/21 portable shaders, both 21,961-frame story runs offscreen with zero audio frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact source tree committed as abd18ce: all focused positives/negatives, unpaced offscreen story through sccnt=20 with zero audio frames, room census, API/frontier/table checks, and ALL PARSERS PASSED
