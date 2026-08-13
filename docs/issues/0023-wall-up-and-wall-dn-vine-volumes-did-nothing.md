---
id: 23
title: WALL_UP and WALL_DN vine volumes did nothing
status: resolved
symptom: player cannot climb authored overworld vines and remains on the lower floor
tags: gameplay,event-box,collision,reverse-engineering
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The script bridge retained flag bits 1/2 but the player mover never consumed them. Two follow-on defects hid behind that: traversal stopped at the source volume rather than the paired destination, and ordinary floor/room-edge queries always chose the highest overlapping triangle instead of the engine's highest triangle at or below the query Y.

## What was tried / dead ends

The `M0000_06_05` southern entrance is a disconnected lower basin, so approaching Bogard directly from below cannot reach the elevated `in_01`. Selecting the east branch in `M0000_07_05` also reaches a disconnected ledge. The authored route climbs the east branch to y=150, crosses to the west branch for y=180, then continues west and south.

## Resolution

The runtime pairs overlapping `WALL_UP`/`WALL_DN` volumes, edge-triggers character-volume contact, lands beyond the paired destination, inverts `sk1.lua`'s authored floor offsets, and hands an out-of-room landing directly to the normal transition owner. `Collision::GetFloorBelow` preserves `SiCollisionMesh::GetFloor`'s query-Y contract on stacked rooms. The mandatory uncapped/silent run now climbs 0→90→150→180, crosses `M0000_07_04` and `M0000_06_04`, enters the elevated `in_01`, and reaches Bogard's house `M0010_00_01`.
