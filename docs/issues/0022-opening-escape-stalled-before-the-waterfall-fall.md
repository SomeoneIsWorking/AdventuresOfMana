---
id: 22
title: Opening escape stalled before the waterfall fall
status: resolved
symptom: continuous opening stopped in M0001_01_03 or M0001_00_03; Julius and Shadow were invisible; scripted PlMoveToChip beyond a room edge did not transition
tags: tooling,progression,pathfinding,actors,movement,transitions,verification
created: 2026-08-13
updated: 2026-08-13
---

## Root causes

Four independent defects became visible only after extending the continuous opening. The headless driver aimed straight through room collision and then its first lattice router recomputed every frame, admitted unrelated boundary shortcuts, and cut corners between validated nodes. `npc()` actor ids in the tagged `eNPC.ENEMY=100` and `eNPC.BOSS=1000` ranges were incorrectly fed through the ordinary NPC id-minus-10 mapping, producing nonexistent N1000/N1010 models and a permanent Julius TELEPORT motion wait. `mapjump` stored world coordinates inside the otherwise room-local MainPlayer actor. Finally, only manual movement owned world-table edge transitions, although authored PlMoveToChip deliberately targets the first chip beyond a room.

## Resolution

The test driver now caches a 7.5-unit collision/floor route, excludes unrelated boundaries, validates edge midpoints, lands exactly on waypoints, reports unreachable denominators, disables audio, and runs without swap pacing. Tagged npc ids map to E#### and B#### with self-tests. mapjump preserves the room-local actor invariant. Manual and scripted movement share room-exit destination/door resolution, and scripted exits require the authored target and live position to cross the same side. The unseeded opening reaches M0001_01_04 through the full Julius/Shadow scene and both scripted exits in 1,748 simulated frames.
