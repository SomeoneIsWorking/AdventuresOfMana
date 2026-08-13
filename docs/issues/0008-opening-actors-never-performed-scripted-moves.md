---
id: 8
title: Opening actors never performed scripted moves
status: resolved
symptom: Opening camera frames the arena but the hero and Jackal do not follow their scripted entrances
tags: gameplay,scripting,movement
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

The bridge interpreted `ChrMoveTo(name,speed,x,z)` as
`ChrMoveTo(name,x,y,z)`. It wrote speed into X, X into Y, and Z into Z, and
created no automatic-movement state. The prelude's `MoveTo` helper therefore
saw the generic false return from `IsChrAutoMove` and stopped yielding
immediately.

## What was tried / dead ends

Fixing the camera exposed the missing actors but did not cause it. Static
inspection of the wrapper shows it forwards `(speed,x,currentY,z)` to
`ChrMoveYTo`; `_plSpeed=110` in the shipping prelude confirms the second
argument is a rate, not a coordinate. Teleporting to the destination would
still skip every coroutine wait and collapse the scene timing.

## Resolution

Preserve current Y for `ChrMoveTo`, consume explicit Y for `ChrMoveYTo`, advance
actors by `speed * dt`, expose the active state through `IsChrAutoMove`, and
treat speed zero as look-at without translation. Scripted `MainPlayer` motion
is synchronized back to the host controller so ordinary input cannot overwrite
it mid-step. The live opening now shows the hero at frame 30 and Jackal entering
at frame 60; `--movement-selftest` covers six positive and negative cases and
the full verifier passes.
