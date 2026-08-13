---
id: C003
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/engine/world.h#Camera, src/engine/script.cpp#Dispatch, src/host/main.cpp#main, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 22:15:26
---

## Claim

The opening scene consumes its fixed camera target through the shipping Lua bridge and frames the arena gate instead of rendering from behind the wall.

## Evidence

M0001_00_00 frame 30 after the fix visibly shows the gate/floor and differs in 516051/518400 pixels from the pre-fix wall frame; --camera-selftest executes six command-state cases; full verify.sh passes.

## What would falsify it

Any change to Camera state, camera command dispatch, live view construction, or the camera verifier; or the same frame reverting to a wall-only view.

## Re-confirmed 2026-08-13

Re-ran full verify.sh with 6/6 camera command cases and visually inspected the corrected frame-30 opening view against the saved pre-fix frame.

## Re-confirmed 2026-08-13

Full verify.sh passed after scripted movement and the camera self-test remained 6/6; opening frame target behavior remains visible.

## Re-confirmed 2026-08-13

Full verify.sh re-ran camera SELFTEST 6/6 and the opening shipping path after the boss-AI ownership change.

## Re-confirmed 2026-08-13

Full verify.sh re-ran camera SELFTEST 6/6 and the opening shipping path after per-actor motion changes.

## Re-confirmed 2026-08-13

Final full verify.sh re-ran camera SELFTEST 6/6 and the opening shipping path.

## Re-confirmed 2026-08-13

Full verify.sh re-ran camera SELFTEST 6/6 and the live opening path.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; camera semantic selftest remained 6/6 and the opening lifecycle consumed the fixed-camera path.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; camera selftest remained 6/6 and opening lifecycle passed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; camera selftest remained 6/6.
