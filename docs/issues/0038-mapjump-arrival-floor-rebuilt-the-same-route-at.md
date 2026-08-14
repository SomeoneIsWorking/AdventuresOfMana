---
id: 38
title: Mapjump arrival floor rebuilt the same route at every waypoint
status: resolved
symptom: After the hidden stair mapjump to M0013_08_04, the uncapped windowless driver logged and rebuilt one route per waypoint because the temporary event-box floor was y=30 while point-ground route samples were y=0.
tags: tooling,navigation,mapjump,event-box,hydra
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

Mapjump arrival volumes temporarily retain their measured body-owned floor while
the route planner intentionally samples point ground. In `M0013_08_04` those
answers are y=30 and y=0 respectively until the character body clears `up_1`.
The generic 30-unit disconnected-component check treated that expected temporary
mismatch as a changed route and rebuilt the identical point-ground path.

## Resolution

Centralized the predicate in `host/navigation`: active mapjump-floor ownership
suppresses only this impossible-to-improve rebuild, while an unowned 30-unit
mismatch still rebuilds. The negative produced 168,575 frames and repeated
`M0013_08_04` rebuild messages. The positive reaches `M0013_09_04` at frame
16,138 with zero such messages, and the navigation self-test exercises both
classes.
