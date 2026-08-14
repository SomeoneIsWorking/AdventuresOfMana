---
id: C018
kind: claim
status: holds
created: 2026-08-14
tags: progression,inventory,tooling
depends: src/engine/script.cpp, src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 03:24:07
---

## Claim

The unseeded story acquires Matock item 17 from Bogard's authored chest through live AddBox/OpenDoor/inventory semantics in a renderless offscreen run.

## Evidence

tools/verify.sh continuous Matock chest gate: room exit into M0010_00_00, opened box and acquired item 17, settled requested item 17, offscreen driver, zero decoded audio.

## What would falsify it

Any mandatory run fails to enter M0010_00_00, fails to fire _BOX/acquire item 17, decodes audio, or uses a visible/rendered non-capture path.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the windowless render bypass, OpenDoor, inventory bridge, and Matock chest changes; all gameplay, self-test, corpus, frontier, and generated-artifact gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the post-Matock route-planner fix; all continuous fixed-step offscreen gates, focused self-tests, negative discriminators, exact asset corpus checks, the 993-room census, and the new 6279-frame silent route to M0000_10_06 passed.
