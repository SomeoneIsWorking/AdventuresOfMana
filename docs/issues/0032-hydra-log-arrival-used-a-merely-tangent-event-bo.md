---
id: 32
title: Hydra log arrival used a merely tangent event-box floor
status: resolved
symptom: After down_1 mapjumped to M0013_00_04 at (75,75), the windowless driver grounded at y=123.6 and stalled against left_1 instead of starting on the authored y=90 log.
tags: tooling,navigation,event-box,collision,hydra
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

MapJump grounding treated squared XZ distance equal to the 15-unit character radius as event-box overlap. The arrival at z=75 is exactly tangent to left_1, whose upper edge is z=60, so the non-strict comparison stole that adjacent box's y=123.6 collision floor even though AppEventBoxBase containment is strict.

## What was tried / dead ends

Extending ordinary event entry with a blocked forward probe was refuted when the existing mapjump reversal guard identified left_1 as the arrival box. Routing to right_1 was also refuted: its y=34 volume is the lower water path and is disconnected from this arrival. Allowing arbitrary downward route edges regressed the earlier elevated Kett return and was reverted.

## Resolution

Require strict body overlap (`d2 < radius^2`) when selecting a MapJump event-box floor. The authored script confirms py>=90 is the upper log path. A fixed-step uncapped SDL-offscreen no-audio run now grounds at y=90, stages to y=120, enters left_1, mapjumps to the west side of M0013_02_00, exits west into M0013_01_00 after 12,369 frames, and decodes zero audio.
