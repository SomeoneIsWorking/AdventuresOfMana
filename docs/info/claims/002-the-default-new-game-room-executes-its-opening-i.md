---
id: C002
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/host/main.cpp#main, src/engine/script.cpp#Script::ResumeCoroutines, tools/verify.sh
reconfirmed: 2026-08-13
verified_at: 2026-08-13 21:17:33
---

## Claim

The default new-game room executes its opening Init sequence through the shipping host path: time advances past wait(600), Arena Guard dialogue appears, and Jackal is seeded from enemydat.bin.

## Evidence

SDL_AUDIODRIVER=dummy ./tools/verify.sh: opening-room lifecycle requires the Init-start, dialogue, and one-enemy-stat log lines after 600 fixed frames; M0000_00_00 matched 0/3 of the same assertions.

## What would falsify it

Any change to room loading, Script coroutine scheduling/game time, AddBoss naming, combat seeding, or the lifecycle verifier; or a 600-frame M0001_00_00 run missing any of the three required observations.

## Re-confirmed 2026-08-13

Re-ran the positive shipping path: M0001_00_00 matched 3/3 assertions after 600 frames. Ran M0000_00_00 as the negative class: 0/3 assertions matched.

## Re-confirmed 2026-08-13

Full SDL_AUDIODRIVER=dummy ./tools/verify.sh passed after camera command integration; opening lifecycle still matched all three required observations.
