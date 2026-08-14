---
id: 35
title: Key doors never opened and stop-room failed successfully
status: resolved
symptom: The continuous Hydra route reached M0013_00_01 with the equipped Silver Key but could not cross its KEY door; when later combat died or stalled before a requested stop room, the diagnostic still returned success.
tags: tooling,door,inventory,combat,navigation,hydra
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The host preserved `SetDoor`'s type but intentionally treated every type except FREE as permanently closed. `AppObjectModel::HitCharacter` proves KEY doors instead scan the four equipped item buttons for ids 18, 30, or 37, call `ModeGame::UseInventory` on the matching button, and open the authored side. The inventory model also retained an equipped id after its last use was consumed. Independently, `--stop-room` only stopped on success and never made normal termination at another room an error, so game-over and timeout diagnostics could masquerade as passing runs.

The top-level verifier had another false-green path: it checked that
`build/mana` existed but never rebuilt it. A source self-test added during this
fix remained invisible—the suite reported 61 cases until a manual build made
the same command report 62—proving the gate could exercise stale code.

After the door, two more missing shipping behaviors became observable. The host had learned Cure item 501 but could not cast it, despite `UseInventoryFunc(501)` proving its 2 MP cost and `wisdom + 20 + GameRandom(25)% of wisdom` healing. The route planner allowed 30-unit descent only for event-box goals, leaving the post-Roper treasure chest visible but unreachable on its lower floor component.

## Resolution

Centred body contact with a KEY door consumes an equipped shipping key and opens that side; exhausting an item clears its button. Cure is callable with C and the fixed-step story driver casts it at half health using the binary formula. Explicitly lower actor goals share the already validated descending-route path. An unmet `--stop-room` now prints the requested and final rooms and exits nonzero, with a shipping-artifact negative in `tools/verify.sh`.

The verifier builds the `mana` target before any runtime assertion, so its
binary evidence corresponds to the current source tree.

The unseeded fixed-step uncapped SDL-offscreen run consumes Silver Key 30, defeats the five Ropers, acquires Iron Shield 402, returns through the opened side, and reaches `M0013_00_02` after 13,580 frames with zero decoded audio.
