---
id: 25
title: Heroine vanished across rooms and Bogard return never started
status: resolved
symptom: after Hasim scene at sccnt 12 the headless story idled in M0000_09_08, and AddParty used an anonymous room-local handle that Bogard scripts could not address
tags: tooling,progression,party,scripting,actors
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

Two independent ownership gaps overlapped. The story driver had no `sccnt=12` return phase, so it fell back to `(30,30)` in the heroine room. `AddParty` also reused anonymous enemy spawning and `World::Reset` discarded that actor on the next room load. Shipping `ModeGame` instead stores one persistent party id at `+0x40c` and names ids 1–9 through the relocation table at `0x3ea080` (`PARTY_HEROINE` through `PARTY_MARCIE`). Bogard's script addresses that exact handle.

## What was tried / dead ends

Merely extending the route would have made the player reach Bogard alone: the anonymous party actor disappeared at the first transition and `ChrMoveUse("PARTY_HEROINE", ...)` remained a silent no-op. The actor identity and persistence contract therefore had to land before the route could be trusted.

## Resolution

`AddParty` now uses the exact handle table, preserves the selected id across rooms, treats selecting the current id as idempotent, and implements id 0 as removal. Each room transition restores the companion under its script-visible handle. The headless driver follows the authored return route and talks to Bogard until the pendant/Matock scene settles at `sccnt=14`. The mandatory gate requires a live `PARTY_HEROINE` diagnostic, four total Bogard talks, the key dialogue, offscreen video, and zero decoded audio.
