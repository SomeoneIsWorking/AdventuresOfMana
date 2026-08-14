---
id: 31
title: Hydra-dungeon route replans forever and pressure switch no-ops
status: resolved
symptom: After reaching M0013_03_01, the fixed-step headless route repeatedly rebuilt at a normal 7.5-unit slope mismatch; once that was bypassed, entering authored switch sw_01 left switch_result at the generic stub value 0 and never enabled down_1.
tags: tooling,navigation,event-box,switch,hydra
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The route's component discriminator treated any 5-unit planned/live floor difference as a disconnect, even though the collision lattice is 7.5 units and shipping movement accepts steps through 30 units. It therefore rejected valid triangle interpolation and rebuilt the same route forever. Separately, `EvBoxSwitch` creates event boxes with flags `0x1c`, whose handler reads the engine-supplied global `switch_result`; the host started the callback without setting that payload, so the generic zero value always selected the released branch.

## What was tried / dead ends

Routing west from `M0013_03_01` entered the disconnected 60-unit puzzle component in `M0013_02_01`; its diagnostic found 0 reachable west-band samples. Routing south from that component reached only x=307.5, outside the authored centered free door. These are measured dead ends, not the main dungeon spine. The connected route goes north through `M0013_03_00`, then west into the `M0013_02_00` switch room.

## Resolution

Use the shipping 30-unit step boundary for component mismatch, set `switch_result=1` before dispatching an entering `0x1c` pressure-switch callback, and route to the enabled `down_1` box after `sw_01` completes. A fixed-step uncapped, SDL-offscreen, no-audio run now enters `sw_01`, enables and enters `down_1`, mapjumps to `M0013_00_04`, exits 0 after 12,250 frames, and reports zero decoded audio.
