---
id: C015
kind: claim
status: holds
created: 2026-08-14
tags:
depends: src/host/main.cpp#main, src/mcf/assets.cpp#Collision::GetFloorBelow, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 01:59:36
---

## Claim

An unseeded new game continuously reaches Bogard's house through the authored overworld and vine route

## Evidence

2026-08-14 live --opening-story --continue-story run reached M0010_00_01 in 2959 fixed-step frames after exactly three WALL_UP traversals, M0000_07_04->M0000_06_04->M0000_06_05 room edges, elevated in_01 entry, and authored mapjump; audio decoded 0 sounds / 0 frames. Mandatory tools/verify.sh gate asserts each discriminator.

## What would falsify it

Falsified if the mandatory continuation no longer reaches M0010_00_01 from an unseeded new game, does not traverse exactly three vine pairs and the elevated in_01 callback, or decodes audio

## Re-confirmed 2026-08-14

Full ./tools/verify.sh pass on 2026-08-14 after stacked-floor and Bogard-route changes; all parsers passed, all focused self-tests passed, both continuous unseeded story gates passed, and gameplay gates decoded 0 audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after inverse-vine routing, AddEnemyZaco, repeatable late placement, and offscreen story-gate changes; all focused self-tests, negative discriminators, continuous story gates, exact asset corpus, and room census passed. The heroine gate settled at sccnt=12 after five WALL_UP and three WALL_DN traversals with zero decoded audio frames.
