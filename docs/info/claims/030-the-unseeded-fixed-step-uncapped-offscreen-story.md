---
id: C030
kind: claim
status: holds
created: 2026-08-14
tags: progression,hydra,navigation
depends: src/host/main.cpp, tools/verify.sh
---

## Claim

The unseeded fixed-step uncapped offscreen story completes the Hydra recovery branch, descends the authored mountain wall, and enters the Hydra boss cluster at M0013_09_00 with zero decoded audio

## Evidence

Full ./tools/verify.sh on 2026-08-14: after Recovery, the run attached at the lone WALL_UP, entered wall_01 and wall_02b, exited west to M0013_05_05, entered in_1, mapjumped to M0013_09_00, and stopped after 15108 frames; SDL video was offscreen and audio decoded 0 sounds / 0 frames.

## What would falsify it

if the unseeded route fails to complete Recovery, does not traverse the authored mountain callbacks and M0013_05_05/in_1, fails to reach M0013_09_00, opens a window, or decodes audio
