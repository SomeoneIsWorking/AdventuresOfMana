---
id: C012
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/host/main.cpp#main, src/host/render.cpp#ActorModelName, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 01:59:36
---

## Claim

A new game progresses continuously through the Julius and Shadow scene and both authored scripted edge moves into M0001_01_04 while headless gameplay tests run uncapped with zero audio decoding

## Evidence

tools/verify.sh passed on 2026-08-13: the continuous opening logged two Jackal kills, SHADOW scripted movement, M0001_00_03->M0001_01_03 and M0001_01_03->M0001_01_04 exits, stop-room M0001_01_04, sccnt=6; direct 1748-frame run ended in 3.22s with 0 sounds / 0 decoded frames

## What would falsify it

Falsified if the unseeded --opening-story run no longer reaches M0001_01_04 with sccnt=6, if either tagged boss model/motion fails, if either scripted edge transition disappears, or if a non-audio gameplay gate decodes audio

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass whose stronger playable-overworld gate landed in b50191c on 2026-08-13.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh pass on 2026-08-14 after stacked-floor and Bogard-route changes; all parsers passed, all focused self-tests passed, both continuous unseeded story gates passed, and gameplay gates decoded 0 audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after inverse-vine routing, AddEnemyZaco, repeatable late placement, and offscreen story-gate changes; all focused self-tests, negative discriminators, continuous story gates, exact asset corpus, and room census passed. The heroine gate settled at sccnt=12 after five WALL_UP and three WALL_DN traversals with zero decoded audio frames.
