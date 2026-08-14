---
id: C022
kind: claim
status: holds
created: 2026-08-14
tags: progression,headless
depends: src/host/main.cpp, tools/verify.sh
---

## Claim

An unseeded new game continuously leaves Kett Manor after scenario 15, defeats all five lizardmen across stacked floors, takes the Warrior regimen, and acquires Silver Key item 30 in 10,831 fixed-step offscreen frames with zero decoded audio.

## Evidence

./tools/verify.sh passed its continuous Silver Key gate on 2026-08-14: shipping out_1 mapjump, overworld cells M0000_10_09 through M0000_13_10, exactly five enemy5 kills, Warrior level 2, authored _BOX item 30, SDL offscreen, and 0 decoded sounds/frames; the complete parser/runtime suite ended ALL PARSERS PASSED.

## What would falsify it

Any unseeded ./tools/verify.sh run fails the Silver Key assertions, decodes audio, opens a window, fails to acquire item 30, or changes the cited driver without equivalent re-verification.
