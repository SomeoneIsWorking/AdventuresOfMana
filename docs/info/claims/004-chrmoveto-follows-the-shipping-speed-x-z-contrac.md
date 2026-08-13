---
id: C004
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/engine/world.h#Actor, src/engine/world.cpp#World::TickScriptMoves, src/engine/script.cpp#Dispatch, src/host/main.cpp#main, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 21:43:38
---

## Claim

ChrMoveTo follows the shipping speed/x/z contract and its coroutine-visible automatic movement advances over time instead of completing immediately.

## Evidence

Wrapper 0x2cb550 forwards speed,x,currentY,z to ChrMoveYTo; --movement-selftest verifies a 3-4-5 path at half and full duration, active/inactive polling, Y preservation, and speed-zero look-at; opening screenshots show hero at frame 30 and Jackal at frame 60; full verify.sh passes.

## What would falsify it

Any change to Actor scripted movement state, ChrMoveTo/ChrMoveYTo/IsChrAutoMove dispatch, gameplay ticking/player synchronization, or movement verifier; or opening actors ceasing to enter at the measured frames.

## Re-confirmed 2026-08-13

Re-ran full verify.sh with movement SELFTEST 6/6 and visually inspected the frame-30 hero and frame-60 Jackal opening captures.

## Re-confirmed 2026-08-13

Full verify.sh re-ran movement SELFTEST 6/6 and the opening shipping path after the boss-AI ownership change.

## Re-confirmed 2026-08-13

Full verify.sh re-ran movement/motion SELFTEST 9/9 and the opening shipping path after per-actor motion changes.

## Re-confirmed 2026-08-13

Final full verify.sh re-ran movement/motion SELFTEST 9/9 and the opening shipping path.

## Re-confirmed 2026-08-13

Full verify.sh re-ran movement SELFTEST 13/13 including exact path distance accounting.
