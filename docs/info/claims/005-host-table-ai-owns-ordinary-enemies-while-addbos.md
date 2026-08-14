---
id: C005
kind: claim
status: holds
created: 2026-08-13
tags: ai,bosses,scripting
depends: src/engine/world.h#UsesHostEnemyAI, src/engine/script.cpp#AddBoss, src/host/main.cpp#ai_selftest
reconfirmed: 2026-08-14
verified_at: 2026-08-14 07:51:11
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

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; AI selftest remained 856 machines / 0 failures and boss ownership path passed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; AI selftest remained 856 machines / 0 failures.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after strict parser and cmd-API instrumentation; AI selftest remained 856 machines / 0 failures.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after exact MPK corpus identity gating; AI selftest remained 856 machines / 0 failures.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after mandatory runtime preflight; AI selftest remained 856 machines / 0 failures.

## Re-confirmed 2026-08-13

Full read-only tools/verify.sh passed on 2026-08-13; AI selftest remained 856 machines / 0 failures.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after boss-death progression; AI selftest remained 856 machines / 0 failures and boss script ownership remained intact.

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass on 2026-08-13 after the opening-router, actor mapping, coordinate, scripted-transition, and silent-test changes; every claim-specific runtime/self-test gate passed on the shipping corpus.

## Re-confirmed 2026-08-13

Re-proved by the complete tools/verify.sh pass against the source landed in 37bda36 on 2026-08-13; every claim-specific runtime/self-test gate passed on the shipping corpus.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh pass on 2026-08-14 after stacked-floor and Bogard-route changes; all parsers passed, all focused self-tests passed, both continuous unseeded story gates passed, and gameplay gates decoded 0 audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after inverse-vine routing, AddEnemyZaco, repeatable late placement, and offscreen story-gate changes; all focused self-tests, negative discriminators, continuous story gates, exact asset corpus, and room census passed. The heroine gate settled at sccnt=12 after five WALL_UP and three WALL_DN traversals with zero decoded audio frames.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after persistent binary-named party identities and the sccnt=12-to-14 return route landed; all continuous story gates, focused self-tests, negative discriminators, exact asset corpus checks, and the 993-room census passed. The newest gate restored live PARTY_HEROINE across nine room loads and settled at sccnt=14 after 5202 offscreen fixed-step frames with zero decoded audio.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 with the repository-owned RE-frontier validator and its zero-entry negative enabled. All continuous story gates, self-tests, negative discriminators, exact 9886-member corpus checks, and 993-room census passed; PARTY_HEROINE remained live at settled sccnt=14 after 5202 offscreen frames with zero decoded audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the windowless render bypass, OpenDoor, inventory bridge, and Matock chest changes; all gameplay, self-test, corpus, frontier, and generated-artifact gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the post-Matock route-planner fix; all continuous fixed-step offscreen gates, focused self-tests, negative discriminators, exact asset corpus checks, the 993-room census, and the new 6279-frame silent route to M0000_10_06 passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed 2026-08-14 after replacing the false Chocobot-detour gate: all existing discriminators passed, and the corrected unseeded continuation reached M0011_00_02 in 6785 fixed-step uncapped offscreen frames with zero gameplay audio decode.

## Re-confirmed 2026-08-14

Reconfirmed against pushed commit 61c7781; its pre-commit full ./tools/verify.sh pass exercised the identical tree, including all existing discriminators and the corrected 6785-frame offscreen silent M0011_00_02 continuation.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Silver Key doorway changes, including all asset/parser negative controls, gameplay gates, selftests, 993-room census, cmd API, world map, and the continuous offscreen/no-audio progression through M0013_03_01.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 after removing unused route branches; all parser negatives, gameplay gates, selftests, 993-room census, command/world-map checks, and continuous offscreen/no-audio progression through M0013_03_01 passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra-dungeon pressure-switch changes, including all negatives, gameplay gates through M0013_00_04, 48/48 movement cases, 993-room census, cmd API, and world-map checks.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra mountain change; all claim-specific self-tests and continuous fixed-step offscreen zero-audio progression gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the verifier was made to rebuild mana first; 62 inventory cases and all continuous offscreen zero-audio gates passed.
