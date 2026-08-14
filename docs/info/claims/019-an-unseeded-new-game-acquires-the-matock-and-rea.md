---
id: C019
kind: claim
status: holds
created: 2026-08-14
tags: progression tooling windowless audio
depends: src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 03:24:07
---

## Claim

An unseeded new game acquires the Matock and reaches the lower route at M0000_10_06

## Evidence

2026-08-14 full ./tools/verify.sh pass: the fixed-step uncapped --opening-story --continue-story gate ran with SDL offscreen, decoded 0 audio sounds/frames, acquired item 17 through the live chest callback, measured 149/1353 reachable samples on M0000_09_06's lower floor, and reached M0000_10_06 after 6279 frames.

## What would falsify it

The mandatory gate fails to acquire item 17, fails to traverse M0000_09_06 to M0000_10_06, opens a visible video driver, decodes any audio, or route evidence accepts an authored goal box with no reachable sample inside it.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the post-Matock route-planner fix; all continuous fixed-step offscreen gates, focused self-tests, negative discriminators, exact asset corpus checks, the 993-room census, and the new 6279-frame silent route to M0000_10_06 passed.
