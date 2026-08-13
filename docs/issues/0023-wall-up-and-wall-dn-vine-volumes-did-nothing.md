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


## What was tried / dead ends


## Resolution

### Dead end (2026-08-14)
The M0000_06_05 southern entrance is a disconnected lower basin; its WALL pair is not a direct staircase to Bogard's elevated in_01 doorway. A continuous route through M0000_07_05 climbed three real vine pairs but the remaining overworld route is not yet established.

### Resolution (2026-08-14)
The script bridge retained flag bits 1/2 but the player mover never consumed them. The runtime now pairs overlapping WALL_UP/WALL_DN volumes, edge-triggers body contact, inverts sk1.lua's authored floor offsets, and clears old-floor route state after traversal. The shipping M0000_07_05 run climbed floors 0->90->150->180; the self-test also proves a real contact positive and a near-miss negative.
