---
id: C011
kind: claim
status: holds
created: 2026-08-13
tags: gameplay,progression,cutscenes
depends: src/engine/script.cpp#ClearRoomScript, src/engine/script.cpp#GlobalNumber, src/host/main.cpp#opening_story, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 23:39:40
---

## Claim

A new game progresses continuously through both opening Jackal fights and the escape into M0001_01_03 with scenario count 4

## Evidence

2026-08-13 --opening-story live run killed exactly two _BOSS actors, showed distinct second-fight intro and escape dialogue, completed Will's scene, crossed FREE and ordinary edges, entered out_01, stopped in M0001_01_03, and logged sccnt=4; mandatory verifier carries the same assertions

## What would falsify it

Any change to Lua coroutine/global lifetime, input gating, fades, room transitions, door/edge traversal, combat death progression, event boxes, opening scripts, story driver, or its verifier assertions

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass on 2026-08-13 after the opening-router, actor mapping, coordinate, scripted-transition, and silent-test changes; every claim-specific runtime/self-test gate passed on the shipping corpus.

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass against the source landed in 37bda36 on 2026-08-13; every claim-specific runtime/self-test gate passed on the shipping corpus.

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass whose stronger playable-overworld gate landed in b50191c on 2026-08-13.
