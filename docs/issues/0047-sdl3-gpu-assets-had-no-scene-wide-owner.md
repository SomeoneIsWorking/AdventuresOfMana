---
id: 47
title: SDL3 GPU assets had no scene-wide owner
status: resolved
symptom: Portable room and actor assets could render only in isolated passes; no owner composed them under one camera/depth target, and per-asset opaque-then-blended submission produced the wrong global material order
tags: renderer,sdl3,gpu,scene,architecture,tooling
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

`AssetPipeline` intentionally owned one asset's buffers, shaders, material
state, and draw ranges. It also submitted both of its material bands in one
call. Composing several such calls would produce opaque/blended, then
opaque/blended ordering and could not represent one scene-wide depth and
transparency pass. There was no separate owner for the higher-level policy.

## What was tried / dead ends

Rendering the hero with the per-asset top-down fitting transform proved
skinning but could not prove composition: it replaced the room and made every
asset fill the target independently. The first common top-down camera left the
shipping hero visible in only five pixels, too weak to cite as scene evidence.

## Resolution

`host/gpu_scene` now owns multi-asset submission under one camera and depth
target. `AssetPipeline` exposes one explicit material-band operation, allowing
the scene owner to submit all opaque ranges before all blended ranges. Camera
and object transform helpers remain independent of both the running game and
presentation. A paired no-depth render differs in 748 of 6,912 pixels from the
old per-asset opaque/blended order, proving the scene policy executes.

The windowless shipping discriminator uses the host's default perspective
convention at 320x240. It draws room `M0001_00_00` and a bind-pose
`C0000_00` hero at room center: the actor changes 88 of 76,800 pixels from the
room-only control, while translating the same actor beyond the frustum changes
0. The composite is written to
`scratch/screenshots/sdl3-gpu-scene.png` through the extracted
`host/image_write` owner. The writer validates dimensions and byte count,
checks the full write and close, and the verifier proves an unavailable output
directory fails rather than silently producing no capture.

### Resolution (2026-08-14)
Added a scene-wide SDL3 GPU owner with global opaque/blended ordering, a common perspective camera/depth target, a shipping room-plus-hero discriminator, and checked PNG capture output.
