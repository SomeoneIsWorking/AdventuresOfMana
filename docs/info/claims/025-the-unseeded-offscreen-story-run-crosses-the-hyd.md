---
id: C025
kind: claim
status: holds
created: 2026-08-14
tags: hydra,navigation,collision
depends: src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 07:51:12
---

## Claim

The unseeded offscreen story run crosses the Hydra upper log and reaches M0013_01_00

## Evidence

Full ./tools/verify.sh passed on 2026-08-14: mandatory fixed-step uncapped run rejected exact tangent floor ownership at M0013_00_04, chose y=90, entered left_1, mapjumped to west M0013_02_00, exited into M0013_01_00 after 12,369 frames, used SDL offscreen, and decoded 0 audio frames.

## What would falsify it

A continuous unseeded run fails to reach M0013_01_00 through left_1, accepts the exactly tangent arrival box as overlapping, opens a window, or decodes any audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra mountain change; all claim-specific self-tests and continuous fixed-step offscreen zero-audio progression gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the verifier was made to rebuild mana first; 62 inventory cases and all continuous offscreen zero-audio gates passed.
