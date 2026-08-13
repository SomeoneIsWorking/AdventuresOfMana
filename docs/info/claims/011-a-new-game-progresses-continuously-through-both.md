---
id: C011
kind: claim
status: holds
created: 2026-08-13
tags: gameplay,progression,cutscenes
depends: src/engine/script.cpp#ClearRoomScript, src/engine/script.cpp#GlobalNumber, src/host/main.cpp#opening_story, tools/verify.sh
---

## Claim

A new game progresses continuously through both opening Jackal fights and the escape into M0001_01_03 with scenario count 4

## Evidence

2026-08-13 --opening-story live run killed exactly two _BOSS actors, showed distinct second-fight intro and escape dialogue, completed Will's scene, crossed FREE and ordinary edges, entered out_01, stopped in M0001_01_03, and logged sccnt=4; mandatory verifier carries the same assertions

## What would falsify it

Any change to Lua coroutine/global lifetime, input gating, fades, room transitions, door/edge traversal, combat death progression, event boxes, opening scripts, story driver, or its verifier assertions
