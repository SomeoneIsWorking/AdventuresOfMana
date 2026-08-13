---
id: 20
title: Will's free door never changed rooms
status: resolved
symptom: Walking north into Will's SetDoor(UP, FREE) stopped at the static collision wall and never entered M0001_00_01
tags: gameplay,doors,progression,collision,world-map,verification
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

The Lua bridge recorded `SetDoor` but discarded its side and type, and the host only implemented scripted `MapJump`/event-box transitions. Will's room has neither: native `_SetDoor` creates a centred door object and marks the room side, so the static `.scol` boundary alone cannot express traversal.

## Resolution

Store all four authored door states in `World`, clear them on room reset, and on blocked outward body contact with a centred FREE door load the adjacent engine world-table cell while preserving world position. KEY/CLOSE/BLOCK/WALL and missing doors do not cross. The exact native door-object contact width remains a documented one-chip PORT CHOICE.

## Evidence

`tools/verify.sh` now runs both classes in the shipping executable: M0001_00_02 UP/FREE logs `door exit 0 -> M0001_00_01`; M0001_00_01 DN/KEY ends in the same room and emits no door exit. `--movement-selftest` reports 27/27 including FREE, KEY, and reset state; the full verifier passed on 2026-08-13.
