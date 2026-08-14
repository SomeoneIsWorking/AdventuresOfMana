---
id: 46
title: SDL3 GPU rejected shipping skinned actors
status: resolved
symptom: The SDL3 GPU asset owner could upload room geometry but had no skinning shader, joint-palette uniform, or portable shipping-actor discriminator, so routing actors through it would render bind data incorrectly or fail
tags: renderer,sdl3,gpu,skinning,architecture,tooling
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The portable asset pipeline only described position, color, and texture
coordinates and selected the static vertex shader for every model. Pose
evaluation and joint-palette construction were also implemented inside the
GLES renderer, so using them from SDL3 GPU would have inverted the intended
ownership boundary.

## What was tried / dead ends

The existing static-asset positive could not detect absent skinning: room
models deliberately contain no weights. Rendering a character once in bind
pose would also be insufficient because a shader that ignored the palette
could produce a plausible non-empty image.

## Resolution

Pose evaluation now has a backend-independent `host/render_pose` owner shared
by GLES and SDL3 GPU. The GPU asset copies the model's skinning classification;
the pipeline adds the shipping weight and incidence attributes, selects a
portable skinned vertex shader, and requires the exact 80-bone, three-row
palette before submission. Shadercross generates byte-identical SPIR-V, DXIL,
and normalized MSL artifacts from that HLSL source.

The mandatory windowless, audio-free discriminator loads shipping hero
`C0000_00` and scans all 2,641 vertices and 6,912 output pixels. Its bind pose
changes 2,812 pixels from clear, translating all 80 palette entries changes
3,723 pixels, and omitting the 960-float palette throws the expected explicit
error. Thus both image classes and the invalid-input class execute the
shipping pipeline.

### Resolution (2026-08-14)
Extracted backend-independent pose evaluation; added portable two-bone SDL3 GPU skinning and a shipping C0000_00 positive/translated/missing-palette discriminator.
