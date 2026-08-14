---
id: C008
kind: claim
status: holds
created: 2026-08-13
tags: bosses,combat,collision,scripting
depends: src/mcf/assets.cpp#ParseGdt, src/mcf/assets.cpp#Collision::BlockedXZ, src/engine/script.cpp#ChrAttackBoneValid, src/engine/world.cpp#TickScriptMoves, src/host/main.cpp#ground_attribute, src/host/main.cpp#hit_this_swing, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 04:29:42
---

## Claim

The opening Jackal enters authored attack phases, damages the player once per enabled phase, and stops scripted movement on the arena wall.

## Evidence

The 600-frame shipping boundary gate reads M0001 GDT EX_1, logs _BOSS map collision, 109 overlaps, 2 landed phases and 6 player damage; movement selftest covers 19 GDT/wall/ISHITMAP/swing cases; full verify.sh passes.

## What would falsify it

Any change to GDT parsing/query, GetGroundAttribute, BlockedXZ or wall candidates, script movement collision, boss attack ownership, attack-bone fallback, swing edge/dedup, or the opening-boss attack gate.

## Re-confirmed 2026-08-13

Full shipping verifier passed on 2026-08-13; the 600-frame opening-boss boundary produced a _BOSS scripted map collision, 2 landed hits, 6 player damage over 2 hits, and 155 measured bone-0 fallbacks; movement selftest was 19/19.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after commit 6e0e4f1 and the MPK tooling change; opening attack path again produced landed hits, player damage, and _BOSS map collision.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after parser exit-status repair; landed hits, player damage, and _BOSS collision remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after strict parser and cmd-API instrumentation; landed hits, player damage, and _BOSS collision remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after exact MPK corpus identity gating; landed hits, player damage, and _BOSS collision remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after mandatory runtime preflight; landed hits, player damage, and _BOSS collision remained observed.

## Re-confirmed 2026-08-13

Full read-only tools/verify.sh passed on 2026-08-13; landed hits, player damage, and _BOSS collision remained observed.

## Re-confirmed 2026-08-13

Full tools/verify.sh passed on 2026-08-13 after boss-death progression; nonlethal attack path retained landed boss hits/player damage/map collision, while lethal path used a player-origin overlap.

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
