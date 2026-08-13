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

The script bridge retained flag bits 1/2 but the player mover never consumed them. Further defects hid behind that: traversal stopped at the source volume rather than the paired destination, left the character origin inside the thin vine strip, and ordinary floor/room-edge queries always chose the highest overlapping triangle instead of the engine's highest triangle at or below the query Y. The first return router also rounded a live position across a floor seam, snapped both axes on cardinal edges, and admitted unrelated room boundaries as shortcuts.

## What was tried / dead ends

The `M0000_06_05` southern entrance is a disconnected lower basin, so approaching Bogard directly from below cannot reach the elevated `in_01`. Selecting the east branch in `M0000_07_05` also reaches a disconnected ledge. The authored route climbs the east branch to y=150, crosses to the west branch for y=180, then continues west and south.

## Resolution

The runtime pairs overlapping `WALL_UP`/`WALL_DN` volumes, edge-triggers character-volume contact, lands beyond the paired destination, then moves one established character radius onto the destination floor when that floor exists. An out-of-room summit has no such floor and is handed directly to the normal transition owner. `Collision::GetFloorBelow` preserves `SiCollisionMesh::GetFloor`'s query-Y contract on stacked rooms. The router attaches the live character to the nearest clear sample on the same floor, preserves the continuous axis across cardinal edges, and excludes every non-target boundary.

The mandatory uncapped, silent, offscreen run now proves both directions: it climbs 0→90→150→180 outbound, reaches Bogard's house, descends from its isolated ledge, climbs the two-stage western return vine to the upper room, then descends the three-stage eastern vine to y=0 and continues through the heroine encounter. The verifier requires exactly five `WALL_UP` and three `WALL_DN` traversals.
