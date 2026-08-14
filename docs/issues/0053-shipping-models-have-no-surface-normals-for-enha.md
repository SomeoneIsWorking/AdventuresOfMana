---
id: 53
title: Shipping models have no surface normals for enhanced shading
status: resolved
symptom: SDL3 currently reproduces the vanilla texture-times-vertex-color baseline, so model surfaces remain uniformly lit and washed out; the shipping vertex declarations contain no normal attribute
tags: renderer,sdl3,shading,normals,enhancement
created: 2026-08-14
updated: 2026-08-14
---

Root cause: the original renderer never stored normals. Its `mLight` block
provides positional colour attenuation plus direct/ambient colour, not surface
orientation. Any directional surface shading must first derive normals from
indexed geometry and label the result as a port enhancement.

Acceptance: a backend-neutral owner derives finite unit normals from both
16-bit and 32-bit indexed models, reports degenerate geometry explicitly, and a
shipping SDL3 offscreen discriminator proves lit, vanilla, and equal-ambient
output differ without opening a window or audio device.

### Resolution (2026-08-14)
Added backend-neutral area-weighted normal generation with explicit 16/32-bit positives and degenerate zero-vector negatives. SDL3 interleaves the derived stream, transforms static/skinned normals, and applies centralized DirectionalLight shading. Root-caused the initial zero-directional result to Shadercross compacting the static normal to attribute location 3 while the pipeline supplied location 5. The shipping room now derives 1,094 triangles with 0 degenerates/unsupported vertices; enhanced differs from vanilla in 3,868 drawn pixels and equal ambient in 3,675, while the skinned hero differs from equal ambient in 2,750. Full tools/verify.sh passed offscreen, unpaced, and silent in scratch/logs/verify-directional-shading.log.
