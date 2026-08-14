---
id: 37
title: Hydra descent stopped short and keyless locked door spun forever
status: resolved
symptom: After returning from M0013_11_00 the fixed-step driver stopped one body radius short of M0013_06_05's west exit; once routed onward, M0013_08_01's KEY door ran over a million frames with no usable key and no failure diagnostic.
tags: tooling,navigation,hydra,door,shop,progression
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The descent branch was still gated on owning Silver Key 30 even though the main door had correctly consumed it, leaving the default `(30,30)` target instead of the west boundary. At the later KEY door, collision blocked the player after the route deque had already reached its contact band; the 120-frame failure detector required a non-empty deque, so that negative branch could never fire.

## Resolution

The post-spring state now drives from the authored lone `WALL_UP` plane through `wall_01`, `wall_02`, and `wall_02b`, crosses west into `M0013_05_05`, enters `in_1`, and reaches the unique boss-cluster room `M0013_09_00`. The router can attach an imminent lone-wall contact before ground BFS rejects the disconnected top ledge. Locked KEY contact now has its own bounded 120-frame negative naming the room side, accepted ids, and all four equipped item buttons.

The adjacent `M0013_08_01` gate correctly reports that no generic Keyring is equipped. ShopInit/ShopAdd, binary-generated buy/sell prices, GetRC/AddRC, and host selection policies are now live and discriminated, but routing to an authored accessible shop and completing the purchase remains the next progression task. Full verification must run offscreen with no audio.
