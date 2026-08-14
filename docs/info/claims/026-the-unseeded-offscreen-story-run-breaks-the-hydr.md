---
id: C026
kind: claim
status: holds
created: 2026-08-14
tags: hydra,navigation,map-object,collision
depends: src/host/main.cpp, src/mcf/assets.cpp, src/mcf/mcf.h, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 10:18:31
---

## Claim

The unseeded offscreen story run breaks the Hydra slope rock and reaches M0013_06_05

## Evidence

Full ./tools/verify.sh passed on 2026-08-14: mandatory fixed-step uncapped run entered padded M0013_01_00, followed its descending slope, consumed Mattock on object id 9/script id 1306, started _BREAKOBJ_1306, entered down_01, mapjumped to M0013_06_05 at y=330 after 12,328 frames, used SDL offscreen, and decoded 0 audio frames.

## What would falsify it

A continuous unseeded run fails to dispatch _BREAKOBJ_1306 or reach M0013_06_05 through down_01, opens a window, or decodes audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra mountain change; all claim-specific self-tests and continuous fixed-step offscreen zero-audio progression gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the verifier was made to rebuild mana first; 62 inventory cases and all continuous offscreen zero-audio gates passed.

## Re-confirmed 2026-08-14

Full offscreen/dummy-audio fixed-step gate tools/verify.sh passed on 2026-08-14; scratch/logs/verify-keyring-structure-v4.log ends VERIFICATION OK.

## Re-confirmed 2026-08-14

Commit af6e3c5 passed the complete offscreen/dummy-audio fixed-step tools/verify.sh gate on 2026-08-14; gameplay and static/corpus evidence is recorded under scratch/logs/.
