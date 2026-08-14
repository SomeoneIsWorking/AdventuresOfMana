---
id: C031
kind: claim
status: holds
created: 2026-08-14
tags:
depends: src/host/story_driver.cpp, src/host/interaction.cpp, src/host/navigation.cpp, src/host/main.cpp, src/engine/event_box.cpp, src/engine/script.cpp, tools/verify.sh
---

## Claim

An unseeded fixed-step uncapped offscreen run buys and equips Motie's Keyring item 18 in slot 4, equips Silver Key item 30 in slot 5, consumes Keyring 18 at the Hydra main-route door, completes the recovery spring, and reaches M0013_09_00 with zero decoded audio frames.

## Evidence

tools/verify.sh passed; scratch/logs/silver-key-story.log records the Keyring purchase, both slots, Keyring use, M0013_09_00 semantic stop at frame 15325, offscreen driver, and 0 sounds / 0 frames.

## What would falsify it

A continuous unseeded shipping run fails to reach M0013_09_00, does not preserve the two authored keys in distinct slots, consumes Silver Key 30 at that door, creates a desktop window, or decodes any audio frame.
