---
id: C013
kind: claim
status: holds
created: 2026-08-13
tags:
depends: src/host/main.cpp#main, tools/verify.sh
---

## Claim

The unseeded opening completes the prison escape and waterfall recovery, restoring playable overworld control in M0000_05_06

## Evidence

Complete tools/verify.sh pass on 2026-08-13: continuous opening logged SHADOW_KNIGHT scripted movement, M0001_01_04->M0001_00_04, authored mapjump to M0000_05_06, recovery dialogue, and terminal state sccnt=10 eventScene=0 cinema=false player-control=true with 0 live coroutines; direct 5000-frame silent run completed in 3.46s

## What would falsify it

Falsified if the unseeded --opening-story gate no longer reaches M0000_05_06, omits the chase/fall/recovery sequence, fails to commit sccnt=10, or ends with input/cinema/eventScene/coroutine state still locked
