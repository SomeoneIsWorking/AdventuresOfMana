---
id: 44
title: GLES asset loading owned backend-independent work
status: resolved
symptom: Adding SDL3 GPU uploads would have required duplicating model parsing, texture retention, bounds, and material resolution from the GLES renderer
tags: renderer,sdl3,gpu,assets,structure,dusklight
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

`Renderable` mixed backend-independent asset interpretation with GLES object
ownership. A second renderer could not consume the same authoritative result;
copying the function would create two subtly divergent asset loaders.

## Resolution

`RenderAsset` now owns model parsing, the `TextureSet` storage that keeps pixel
spans valid, flattened logical texture references, per-draw material resolution,
and bounds. The legacy GLES uploader and new SDL3 GPU `Asset` both consume that
one result. SDL3 GPU resource lifetime and draw submission remain separate
owners.

The offscreen shipping-asset test loads `M0001_00_00`, uploads all seven logical
textures plus vertex/index buffers, submits 12 draw ranges, and scans 6,912
pixels. The positive observes 3,868 non-clear pixels and 202 distinct changed
red values; all 3,868 pixels differ from the same draw with every material
forced to a white texture, proving the uploaded material textures are sampled.
Its no-draw class scans the same 6,912 pixels and observes zero changes. Both
run with no window and no audio initialization.
