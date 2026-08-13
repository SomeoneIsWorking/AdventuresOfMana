---
id: C005
kind: claim
status: holds
created: 2026-08-13
tags: ai,bosses,scripting
depends: src/engine/world.h#UsesHostEnemyAI, src/engine/script.cpp#AddBoss, src/host/main.cpp#ai_selftest
reconfirmed: 2026-08-13
verified_at: 2026-08-13 21:27:48
---

## Claim

Host table AI owns ordinary enemies, while AddBoss actors are controlled by their map-script boss coroutines.

## Evidence

UsesHostEnemyAI is exercised through --ai-selftest for both E and B actors; the extracted 714-script corpus pairs every live AddBoss encounter with base or composite _BOSS code; full verify.sh passes.

## What would falsify it

Any change to AddBoss coroutine startup, UsesHostEnemyAI, the host AI loop, the AI selftest, or evidence of a live AddBoss encounter without script-owned boss behavior.

## Re-confirmed 2026-08-13

AI SELFTEST accepted ordinary E and refused scripted B, swept 856 shipping machines with 0 failures, and full verify.sh passed.
