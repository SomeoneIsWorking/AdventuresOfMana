---
id: C015
kind: claim
status: holds
created: 2026-08-14
tags:
depends: src/host/main.cpp#main, src/mcf/assets.cpp#Collision::GetFloorBelow, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 03:24:07
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

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after persistent binary-named party identities and the sccnt=12-to-14 return route landed; all continuous story gates, focused self-tests, negative discriminators, exact asset corpus checks, and the 993-room census passed. The newest gate restored live PARTY_HEROINE across nine room loads and settled at sccnt=14 after 5202 offscreen fixed-step frames with zero decoded audio.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 with the repository-owned RE-frontier validator and its zero-entry negative enabled. All continuous story gates, self-tests, negative discriminators, exact 9886-member corpus checks, and 993-room census passed; PARTY_HEROINE remained live at settled sccnt=14 after 5202 offscreen frames with zero decoded audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the windowless render bypass, OpenDoor, inventory bridge, and Matock chest changes; all gameplay, self-test, corpus, frontier, and generated-artifact gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the post-Matock route-planner fix; all continuous fixed-step offscreen gates, focused self-tests, negative discriminators, exact asset corpus checks, the 993-room census, and the new 6279-frame silent route to M0000_10_06 passed.
