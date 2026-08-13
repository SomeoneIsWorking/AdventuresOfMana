---
id: C011
kind: claim
status: holds
created: 2026-08-13
tags: gameplay,progression,cutscenes
depends: src/engine/script.cpp#ClearRoomScript, src/engine/script.cpp#GlobalNumber, src/host/main.cpp#opening_story, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 01:59:36
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

## Re-confirmed 2026-08-14

Full ./tools/verify.sh pass on 2026-08-14 after stacked-floor and Bogard-route changes; all parsers passed, all focused self-tests passed, both continuous unseeded story gates passed, and gameplay gates decoded 0 audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after inverse-vine routing, AddEnemyZaco, repeatable late placement, and offscreen story-gate changes; all focused self-tests, negative discriminators, continuous story gates, exact asset corpus, and room census passed. The heroine gate settled at sccnt=12 after five WALL_UP and three WALL_DN traversals with zero decoded audio frames.
