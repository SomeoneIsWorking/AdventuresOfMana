---
id: C014
kind: claim
status: holds
created: 2026-08-14
tags:
depends: src/engine/world.cpp#World::FindEventWall, src/host/main.cpp#main
reconfirmed: 2026-08-14
verified_at: 2026-08-14 01:00:51
---

## Claim

Shipping WALL_UP/WALL_DN event boxes drive player vine traversal, including three consecutive floor changes in M0000_07_05

## Evidence

./tools/verify.sh passes eventbox self-test 18/18; scratch/logs/bogard-route.log from an uncapped --no-audio opening run logged authored floors 0->90->150->180. The self-test feeds the shipping first-vine coordinates and a near-miss negative.

## What would falsify it

FindEventWall, the player event-wall consumer, event-box flag storage, or EvBoxWallUp/EvBoxWallDn authored offsets change

## Re-confirmed 2026-08-14

Full ./tools/verify.sh pass on 2026-08-14 after stacked-floor and Bogard-route changes; all parsers passed, all focused self-tests passed, both continuous unseeded story gates passed, and gameplay gates decoded 0 audio.
