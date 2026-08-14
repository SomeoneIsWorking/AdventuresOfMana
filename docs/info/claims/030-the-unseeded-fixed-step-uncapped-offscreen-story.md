---
id: C030
kind: claim
status: holds
created: 2026-08-14
tags: progression,hydra,navigation
depends: src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 10:53:03
---

## Claim

The unseeded fixed-step uncapped offscreen story completes the Hydra recovery branch, descends the authored mountain wall, and enters the Hydra boss cluster at M0013_09_00 with zero decoded audio

## Evidence

Full ./tools/verify.sh on 2026-08-14: after Recovery, the run attached at the lone WALL_UP, entered wall_01 and wall_02b, exited west to M0013_05_05, entered in_1, mapjumped to M0013_09_00, and stopped after 15108 frames; SDL video was offscreen and audio decoded 0 sounds / 0 frames.

## What would falsify it

if the unseeded route fails to complete Recovery, does not traverse the authored mountain callbacks and M0013_05_05/in_1, fails to reach M0013_09_00, opens a window, or decodes audio

## Re-confirmed 2026-08-14

Commit af6e3c5 passed the complete offscreen/dummy-audio fixed-step tools/verify.sh gate on 2026-08-14; gameplay and static/corpus evidence is recorded under scratch/logs/.

## Re-confirmed 2026-08-14

Commit fe20918 passed the complete fixed-step uncapped offscreen/dummy-audio tools/verify.sh gate through Hydra's authored sccnt=16 defeat transition on 2026-08-14; evidence is scratch/logs/verify-hydra-defeat.log.

## Re-confirmed 2026-08-14

Commit a80ff75: the complete mandatory ./tools/verify.sh passed in scratch/logs/verify-steward-wolf.log after the landed changes, exercising this claim's positive and negative checks offscreen; the extended continuous run settled at sccnt=19 with zero decoded audio frames.
