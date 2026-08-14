---
id: 45
title: SDL3 textured assets ignored depth and authored blend state
status: resolved
symptom: The SDL3 GPU asset test proved geometry and texture sampling but rendered every draw through one pipeline with no depth target and no material blending, so a shipping room would have wrong occlusion, opaque water, and opaque shadows
tags: renderer,sdl3,gpu,depth,blending,tooling
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The first asset pipeline described only vertex input, shaders, and a color
target. It created no depth texture, declared no depth-stencil pipeline state,
and copied every draw range through the same opaque pipeline even though the
parsed shipping material already carries its measured blend flag.

## What was tried / dead ends

The original texture and no-draw controls were necessary but insufficient:
both still pass when every fragment ignores scene depth and alpha. Color
variation therefore could not be cited as evidence of correct scene ordering.

## Resolution

The GPU device now selects a supported depth format and can attach a cleared
depth target to offscreen renders. `AssetPipeline` owns separate opaque and
alpha-blended pipeline objects, tests depth with `LESS`, writes depth only for
opaque draws, and submits the parsed material bands in opaque-then-blended
order. It also accepts an explicit model-view-projection matrix so this state
can serve the shipping camera rather than only the fitted test view.

Three shipping-data controls run windowless with audio uninitialized:

- `M0001_00_00` changes 3,868 of 6,912 pixels from clear and all 3,868 differ
  from a forced-white material render.
- Drawing that textured room near, then drawing it white and farther away,
  changes 0 pixels with depth enabled. Disabling depth changes 3,868 pixels.
- `M0000_00_03` contains two authored blended water draws; 128 of 6,912 pixels
  differ from the same scene with material blending disabled.
