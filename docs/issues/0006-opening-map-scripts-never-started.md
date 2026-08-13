---
id: 6
title: Opening map scripts never started
status: resolved
symptom: Default new game loaded M0001_00_00 but its opening scene and boss did not run
tags: gameplay,scripting,lifecycle
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

The host evaluated a room's Lua file but never called its `Init` function. Even
after starting `Init`, the shared `wait(n)` helper could not complete because
`GetGameTimeMs` fell through to the generic numeric stub and always returned
zero. The opening boss then spawned under an invented host handle, so all
shipping commands addressed `_BOSS` while the actor lived under another name.

## What was tried / dead ends

The first screenshot after starting `Init` still showed no dialogue. That did
not disprove the coroutine path: it isolated the frozen game-time API because
the script's first yield is `wait(600)`. Callback presence also cannot be used
to infer composite boss bodies: Hydra has `_BOSS_00` through `_BOSS_02` but only
declares an `_BOSS_02` coroutine. Composite boss naming remains separate RE.

## Resolution

Start room `Init` after the player exists, advance the script game clock from
the 30 Hz gameplay timeline, and clear map-local callbacks and coroutines on a
transition while retaining scenario globals. `AddBoss` now creates the common
base actor under the binary-supplied `_BOSS` name and starts that coroutine
when the room defines it. Combat seeding is idempotent per actor, so enemies
created by a resumed coroutine receive `enemydat.bin` state without restoring
the HP of actors already fighting.

The shipping-path gate runs 600 fixed frames in `M0001_00_00` and requires all
three observations: its `Init` starts, the Arena Guard line appears, and exactly
one late-spawned enemy receives table stats. The same assertions match 0/3 in
`M0000_00_00`, proving the discriminator can report the negative class.
