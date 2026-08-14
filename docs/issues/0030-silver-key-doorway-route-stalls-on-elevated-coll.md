---
id: 30
title: Silver Key doorway route stalls on elevated collision seam
status: resolved
symptom: The continuous fixed-step headless story run acquired the Silver Key but stalled for 120 frames at local (180,90,30) in M0000_14_08 while approaching in_01; the planned diagonal reported floor 82.5 while shipping movement remained on floor 90 and rejected every step.
tags: tooling,navigation,collision,silver-key,equipment
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

Two missing or false instrument behaviors compounded. The Lua bridge stubbed binary command GetEquipID, so the authored Silver Key door could never observe equipped item 30. The route planner also validated edges from a point shifted 0.1 units backward from the real lattice node. At the elevated M0000_14_08 collision seam this placed the diagnostic on the other side of ownership and admitted a diagonal that shipping movement, which begins exactly at the node, rejects. Direction-only route compression was additionally unsafe across changing floor ownership.

## Resolution

Implemented the binary-derived eight equipment slots and GetEquipID slot bounds, equipped the acquired Silver Key into BTN_SUB slot 4, made planner edge sweeps start at the actual lattice node at the shipping fixed-step distance, retained exact BFS lattice routes whenever floor height changes, and added a 120-frame non-silent route-stall diagnostic. The unseeded offscreen/no-audio fixed-step uncapped run now enters M0000_14_08/in_01, executes mapjump to M0013_03_01, exits 0 after 11,740 frames, and reports 0 decoded sounds / 0 frames.

## Discriminator

Before the fix `scratch/logs/silver-door-route-18.log` reaches only 220/1353 nodes and emits `host:error` at the seam. After the fix `scratch/logs/silver-door-route-19.log` reaches 241/1353, enters `in_01`, reaches `M0013_03_01`, and exits 0.
