---
id: C006
kind: claim
status: holds
created: 2026-08-13
tags: animation,scripting,bosses
depends: src/engine/world.h#Actor, src/engine/world.cpp#TickMotions, src/engine/script.cpp#IsChrMotionFinish, src/host/main.cpp#missing_actor_models, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 04:29:42
---

## Claim

The default opening resolves late-spawned boss assets, advances the boss's shipping motion duration, and exits MotionFinishWait into combat.

## Evidence

The 600-frame shipping path logs late _BOSS model B0000_00 loading and BGM 2 after MotionFinishWait; movement selftest exercises unfinished, exact-end, and force-restart motion classes; full verify.sh passes.

## What would falsify it

Any change to actor motion clocks, Lua motion commands/queries, late actor rendering, room lifecycle verification, or a 600-frame opening that does not reach BGM 2.

## Re-confirmed 2026-08-13

Strengthened 600-frame opening gate observed late B0000_00 load and BGM 2; movement/motion SELFTEST passed 9/9; full verify.sh passed.

## Re-confirmed 2026-08-13

Final 600-frame opening gate observed late B0000_00 load and BGM 2 with motion resolution independent of rendering; SELFTEST 9/9 and full verify.sh passed.

## Re-confirmed 2026-08-13

Full verify.sh observed late boss asset load, BGM 2, and subsequent scripted boss movement.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; the opening resolved the late boss, advanced its motion, switched to BGM 2, moved the boss, and reached combat.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; late boss model, BGM 2, and scripted movement were observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after strict parser and cmd-API instrumentation; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after exact MPK corpus identity gating; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after mandatory runtime preflight; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full read-only tools/verify.sh passed on 2026-08-13; late boss model, BGM 2, and movement remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after boss-death progression; late boss model, BGM 2, movement, death, and transition were observed.

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
