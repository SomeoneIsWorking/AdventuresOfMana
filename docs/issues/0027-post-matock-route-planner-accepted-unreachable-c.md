---
id: 27
title: Post-Matock route planner accepted unreachable cave floors
status: resolved
symptom: The unseeded run either stalled in M0000_08_06 or selected M0000_09_06 in_2 despite arriving on the isolated lower floor.
tags: tooling,navigation,collision,post-matock
created: 2026-08-14
updated: 2026-08-14
---

The first fix correctly made point goals require a reachable sample inside the authored event box, but then followed the reachable lower strip into `M0000_10_06`. That conclusion was wrong: `M0000_10_07.lua` gates `_BRIDGE_JUMP` on player type 6 (Chocobot), so the route was a later-game detour rather than current progression.

The actual route climbs both authored `M0000_08_06` vine pairs with an `M0000_08_05` cross-room detour, enters `M0000_09_06/in_1` at y=90, and mapjumps to cave `M0011_00_00`. Additional planner root causes surfaced there: exact-Y cache invalidation rebuilt BFS on every slope frame; compressed segments checked walls but not floor continuity; event goals accepted strict box boundaries; a room-scoped event-wall latch leaked across loads; downward boundary landings were incorrectly pushed into the upper cell; and the room-1 route could fall 45 units into an irreversible pit.

The fixed instrument caches ordinary slopes, explicitly invalidates room/wall changes, validates compressed floor resolution, uses strict containment, preserves character-volume wall contact, keeps downward landings inside the lower cell, and rejects >30-unit floor edges. The mandatory unseeded gate now uses two of the Mattock's seven binary-derived uses on breakable object id 9 and reaches `M0011_00_02` in fixed-step uncapped offscreen mode with zero decoded audio. Falsified if it selects the Chocobot detour, bypasses the rocks, falls into the pit, creates a window, or decodes audio.
