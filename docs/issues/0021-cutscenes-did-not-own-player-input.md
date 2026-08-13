---
id: 21
title: Cutscenes did not own player input
status: resolved
symptom: The headless opening escaped Will's room before its coroutine committed sccnt=2, causing the first Jackal story branch to repeat forever
tags: gameplay,cutscenes,input,scripting,progression,verification
created: 2026-08-13
updated: 2026-08-13
---

## Root causes

The Lua bridge stubbed `SetCinema` and `SetPlayerControllEnable`, while the host ignored the prelude's global `eventScene`. Human and headless movement/attacks therefore continued during authored scenes. Separately, a coroutine can intentionally `MapJump` while a fade still owns input; cancelling that room coroutine left the destination with a stale disabled-control bit.

## Resolution

Expose numeric Lua globals, implement both input commands, and gate movement, attacks, talking, and headless combat on all three native script-owned states. Clear transient cinema/control locks when replacing a room script; scenario globals remain intact. Add `--opening-story` plus `--stop-room` so verification drives the actual continuous story rather than seeding a convenient room state.

## Evidence

The continuous shipping run kills exactly two Jackals with their distinct intro/death dialogue, completes Will before leaving, crosses FREE and ordinary edges, enters the authored `out_01` box, reaches M0001_01_03, and reports sccnt=4. Movement selftest covers both values of SetCinema and SetPlayerControllEnable. Full verifier passed on 2026-08-13.
