---
id: 24
title: AddEnemyZaco silently spawned no enemies
status: resolved
symptom: heroine encounter remained at sccnt 10 with no enemies, combat, or EnemyDead callback
tags: tooling,lua,enemies,verification
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The generated Lua binding existed, but `Dispatch` had no `AddEnemyZaco` implementation. Its generic void fallback therefore returned success without creating actors. Random actor placement also ran only during `loadRoom`, before a room's `Init` coroutine could spawn enemies.

## What was tried / dead ends

Late combat seeding was initially suspected because combat stats remained empty. That code was already polled after every coroutine resume; it had no actors to seed. The room script and binary wrapper instead showed the missing producer.

## Resolution

`AddEnemyZaco` now follows the binary-derived count plus `-1`-terminated random type contract and marks each actor for engine-owned placement. Placement is repeatable after coroutine resumes. Focused self-tests prove zero-count and multi-type behavior; the mandatory continuous gate creates, places, seeds, and defeats all three heroine-room Myconids, fires `EnemyDead`, and settles at `sccnt=12`.
