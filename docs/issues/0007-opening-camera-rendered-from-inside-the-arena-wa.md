---
id: 7
title: Opening camera rendered from inside the arena wall
status: resolved
symptom: M0001_00_00 opening frames show only close-up masonry instead of the scripted arena target
tags: gameplay,camera,scripting
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

The Lua bridge had no `CamSetTargetPos` branch. Its `CamSetPos` branch wrote the
arguments into a field named as a target, and the renderer never read that
field. The opening script therefore requested an arena look target while the
host kept orbiting the stationary player from behind the arena wall.

## What was tried / dead ends

Running the opening for 30, 60, 90, 120 and 180 frames produced the same
wall-only view at every point, ruling out a slow camera interpolation. The
binary wrappers distinguish `CamSetTargetPos`, `CamSetTargetPosSub`, and
`CamSetPos`; treating the last two as aliases would preserve the original bug.

## Resolution

Model fixed target, target offset, character target, and explicit eye position
separately; dispatch each shipping command to its own state and consume that
state in the live view calculation. At frame 30 the corrected view shows the
arena gate and floor and differs in 516051 of 518400 pixels from the wall-only
frame. `--camera-selftest` executes the commands through the shipping Lua bridge
and checks six positive and negative state transitions.
