---
id: C026
kind: claim
status: holds
created: 2026-08-14
tags: hydra,navigation,map-object,collision
depends: src/host/main.cpp, src/mcf/assets.cpp, src/mcf/mcf.h, tools/verify.sh
---

## Claim

The unseeded offscreen story run breaks the Hydra slope rock and reaches M0013_06_05

## Evidence

Full ./tools/verify.sh passed on 2026-08-14: mandatory fixed-step uncapped run entered padded M0013_01_00, followed its descending slope, consumed Mattock on object id 9/script id 1306, started _BREAKOBJ_1306, entered down_01, mapjumped to M0013_06_05 at y=330 after 12,328 frames, used SDL offscreen, and decoded 0 audio frames.

## What would falsify it

A continuous unseeded run fails to dispatch _BREAKOBJ_1306 or reach M0013_06_05 through down_01, opens a window, or decodes audio.
