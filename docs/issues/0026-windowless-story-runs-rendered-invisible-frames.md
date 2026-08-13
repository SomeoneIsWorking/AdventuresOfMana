---
id: 26
title: Windowless story runs rendered invisible frames and OpenDoor was inert
status: resolved
symptom: uncapped silent offscreen progression runs consumed GPU time and stalled at Bogard's opened north door
tags:tooling,headless,door,progression
created: 2026-08-14
updated: 2026-08-14
---

## Root cause


## What was tried / dead ends


## Resolution

### Resolution (2026-08-14)
Two independent tooling/runtime causes: no-window still executed the full draw path, rebuilding skeletal palettes for discarded pixels, and OpenDoor remained a typed no-op after SetDoor(CLOSE). Windowless non-capture gameplay now skips rendering while retaining simulation, and binary-derived OpenDoor clears the directional room mark. A mandatory unseeded gate proves the resulting path enters M0010_00_00 and acquires Matock item 17 through the shipping _BOX callback.
