---
id: C028
kind: claim
status: holds
created: 2026-08-14
tags: progression,hydra,door
depends: src/host/main.cpp, src/mcf/assets.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 07:51:12
---

## Claim

The unseeded fixed-step uncapped offscreen story consumes the equipped Silver Key at a KEY door, clears the Roper side room, and reaches M0013_00_02 with zero decoded audio

## Evidence

Full tools/verify.sh gate on 2026-08-14: key 30 consumed from slot 4, item 402 acquired, M0013_00_02 reached after 13580 frames, SDL offscreen, audio decoded 0 sounds / 0 frames

## What would falsify it

if the route crosses the KEY door without consuming an accepted equipped key, leaves a spent key equipped, dies or stalls in M0013_01_01, opens a window, decodes audio, or fails to reach M0013_00_02

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the verifier was made to rebuild mana first; 62 inventory cases and all continuous offscreen zero-audio gates passed.
