---
id: C025
kind: claim
status: holds
created: 2026-08-14
tags: hydra,navigation,collision
depends: src/host/main.cpp, tools/verify.sh
---

## Claim

The unseeded offscreen story run crosses the Hydra upper log and reaches M0013_01_00

## Evidence

Full ./tools/verify.sh passed on 2026-08-14: mandatory fixed-step uncapped run rejected exact tangent floor ownership at M0013_00_04, chose y=90, entered left_1, mapjumped to west M0013_02_00, exited into M0013_01_00 after 12,369 frames, used SDL offscreen, and decoded 0 audio frames.

## What would falsify it

A continuous unseeded run fails to reach M0013_01_00 through left_1, accepts the exactly tangent arrival box as overlapping, opens a window, or decodes any audio.
