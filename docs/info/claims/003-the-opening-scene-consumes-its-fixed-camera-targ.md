---
id: C003
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/engine/world.h#Camera, src/engine/script.cpp#Dispatch, src/host/main.cpp#main, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 21:17:33
---

## Claim

The opening scene consumes its fixed camera target through the shipping Lua bridge and frames the arena gate instead of rendering from behind the wall.

## Evidence

M0001_00_00 frame 30 after the fix visibly shows the gate/floor and differs in 516051/518400 pixels from the pre-fix wall frame; --camera-selftest executes six command-state cases; full verify.sh passes.

## What would falsify it

Any change to Camera state, camera command dispatch, live view construction, or the camera verifier; or the same frame reverting to a wall-only view.

## Re-confirmed 2026-08-13

Re-ran full verify.sh with 6/6 camera command cases and visually inspected the corrected frame-30 opening view against the saved pre-fix frame.
