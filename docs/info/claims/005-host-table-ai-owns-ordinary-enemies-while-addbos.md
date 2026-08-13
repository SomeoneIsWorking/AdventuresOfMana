---
id: C005
kind: claim
status: holds
created: 2026-08-13
tags: ai,bosses,scripting
depends: src/engine/world.h#UsesHostEnemyAI, src/engine/script.cpp#AddBoss, src/host/main.cpp#ai_selftest
reconfirmed: 2026-08-13
verified_at: 2026-08-13 22:02:29
---

## Claim

Host table AI owns ordinary enemies, while AddBoss actors are controlled by their map-script boss coroutines.

## Evidence

UsesHostEnemyAI is exercised through --ai-selftest for both E and B actors; the extracted 714-script corpus pairs every live AddBoss encounter with base or composite _BOSS code; full verify.sh passes.

## What would falsify it

Any change to AddBoss coroutine startup, UsesHostEnemyAI, the host AI loop, the AI selftest, or evidence of a live AddBoss encounter without script-owned boss behavior.

## Re-confirmed 2026-08-13

AI SELFTEST accepted ordinary E and refused scripted B, swept 856 shipping machines with 0 failures, and full verify.sh passed.

## Re-confirmed 2026-08-13

Full verify.sh re-ran AI SELFTEST over both ownership classes and 856 shipping machines after dynamic actor asset loading.

## Re-confirmed 2026-08-13

Final full verify.sh re-ran both AI ownership classes and all 856 shipping machines.

## Re-confirmed 2026-08-13

Full verify.sh re-ran AI ownership and all 856 shipping AI machines after boss-script changes.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; AI ownership selftests covered all 856 machines and the opening boss remained map-script owned through its live attack path.
