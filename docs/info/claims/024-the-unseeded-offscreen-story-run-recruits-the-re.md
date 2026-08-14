---
id: C024
kind: claim
status: holds
created: 2026-08-14
tags: progression,event-box,tooling
depends: src/host/main.cpp, src/engine/script.cpp, src/engine/script.h, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 06:27:12
---

## Claim

The unseeded offscreen story run recruits the Red Mage and operates the first Hydra-dungeon pressure switch

## Evidence

Final ./tools/verify.sh passed on 2026-08-14: the mandatory fixed-step uncapped run entered M0013_02_00/sw_01 before down_1, mapjumped to M0013_00_04 at frame 12250, used SDL offscreen, and decoded 0 audio frames; movement selftest passed 48/48 including both switch payload and object visibility classes.

## What would falsify it

A clean unseeded run bypasses sw_01, fails to enable/enter down_1, fails to reach M0013_00_04, opens a window, decodes audio, or exits nonzero.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra-dungeon pressure-switch changes, including all negatives, gameplay gates through M0013_00_04, 48/48 movement cases, 993-room census, cmd API, and world-map checks.
