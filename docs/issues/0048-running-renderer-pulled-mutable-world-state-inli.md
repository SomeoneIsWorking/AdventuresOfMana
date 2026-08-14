---
id: 48
title: Running renderer pulled mutable world state inline
status: resolved
symptom: The gameplay loop resolved script camera targets, interpolated the eye, selected visible room objects, and walked mutable actors directly inside the GLES draw block, leaving SDL3 GPU no stable same-frame scene input
tags: renderer,scene,snapshot,camera,architecture,dusklight
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The gameplay loop performed two separable jobs inside its GLES block: it
interpreted mutable game state (script camera slots, room origins, object
visibility, actor positions/motions) and submitted GL resources. SDL3 GPU
could not reuse the first job without depending on `main.cpp` or reimplementing
its policy, and the two renderers could not be compared at a defined frame
boundary.

## What was tried / dead ends

Passing `World` directly to `gpu_scene` would merely move the coupling: the GPU
backend would then own script-camera defaults, local-to-world conversion, eye
interpolation, actor naming, and visibility. That contradicts the established
asset and scene ownership boundaries.

## Resolution

`host/render_camera` now owns resolution of the live script camera into one
`CameraFrame`, retaining only eye interpolation state. Its seven-case test
covers the player default, spherical eye, fixed target plus room origin and
offset, explicit eye, character target, missing-character fallback, and the
exact 30% one-frame step at shipping speed 0.3.

`host/render_snapshot` freezes that camera plus backend-independent
`RenderAsset`, transform, motion, and motion-time references for each room,
visible object, actor, and hero instance. The running GLES path now builds and
consumes that snapshot. Its three-case test checks camera retention, static and
animated instance fields, and a one-static/one-skinned discriminator. A real
30-frame fixed-step, windowless, audio-free capture of `M0001_00_00` renders
the room, authored objects, and animated hero through this path at
`scratch/screenshots/render-snapshot.png`.

### Resolution (2026-08-14)
Extracted CameraTracker and RenderSnapshot owners; the running GLES draw now consumes one frozen camera/room/object/actor frame, verified by focused cases and a real windowless shipping-room capture.
