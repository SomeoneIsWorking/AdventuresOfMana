---
id: 9
status: resolved
symptom: Scripted bosses can move or change motion independently of their _BOSS coroutine
tags: [gameplay, scripting, ai, bosses]
---

# Generic enemy AI overrode script-owned bosses

## Root cause

The host's generic enemy-AI loop selected both ordinary enemies (`kind == 'E'`)
and bosses (`kind == 'B'`). `AddBoss`, however, starts the map's `_BOSS`
coroutine, which owns the boss's movement and motion. The opening Jackal therefore
had two controllers writing the same actor every frame.

This is not a `SetCinema` problem. `ModeGame::SetCinema(bool)` at `0x2d3e74`
only stores the cinemascope flag and the current frame counter. The map prelude's
`_EVENT_START` likewise sets the Lua `eventScene` variable; the Jackal script has
its own `_boss_start_scene` wait before beginning combat behavior.

## Evidence

The extracted corpus contains 714 map scripts. All 22 live scripts which call
`AddBoss` contain a base or composite `_BOSS` function; the only apparent
exception is a commented-out `AddBoss` line. Ordinary enemies have no such
per-instance coroutine and use `enemydat.bin` table AI.

## Resolution

`UsesHostEnemyAI` gives the generic host loop one explicit ownership predicate:
live ordinary enemies only. `--ai-selftest` runs both discriminator classes and
requires an ordinary enemy to be accepted and a scripted boss to be refused.
Boss combat statistics are still seeded from `enemydat.bin`; only the competing
movement/state controller is removed.
