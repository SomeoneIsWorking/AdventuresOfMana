---
id: C012
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/host/main.cpp#main, src/host/render.cpp#ActorModelName, tools/verify.sh
---

## Claim

A new game progresses continuously through the Julius and Shadow scene and both authored scripted edge moves into M0001_01_04 while headless gameplay tests run uncapped with zero audio decoding

## Evidence

tools/verify.sh passed on 2026-08-13: the continuous opening logged two Jackal kills, SHADOW scripted movement, M0001_00_03->M0001_01_03 and M0001_01_03->M0001_01_04 exits, stop-room M0001_01_04, sccnt=6; direct 1748-frame run ended in 3.22s with 0 sounds / 0 decoded frames

## What would falsify it

Falsified if the unseeded --opening-story run no longer reaches M0001_01_04 with sccnt=6, if either tagged boss model/motion fails, if either scripted edge transition disappears, or if a non-audio gameplay gate decodes audio
