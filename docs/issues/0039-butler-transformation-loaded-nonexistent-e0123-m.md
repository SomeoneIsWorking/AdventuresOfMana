---
id: 39
title: Butler transformation loaded nonexistent E0123 model
status: resolved
symptom: After Mirror exposes the Butler, enemy123_1 has no model and the uncapped story run attacks forever without reaching sccnt 19
tags: tooling,rendering,actor-model,kett,reverse-engineering
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The shared actor-model resolver assumed every `AddEnemy` id maps directly to
`E<id>_00`. The authored Butler transformation deliberately calls
`AddEnemy(123)`, for which no `E0123_00` asset exists.

## Evidence

`ModeGame::AddEnemy` at `0x2dda74` first formats `sk1/E%04d_00`, compares the
id with `0x7b`, and for 123 overwrites the filename with the literal
`sk1/B0023_00` before `CharacterSetFileName`. The extracted Lua comments out
`AddBoss(eBOSS.STEWARD_WOLF)` and calls `AddEnemy(123)`. Both `E0023_00` and
`B0023_00` exist, so substituting ordinary enemy 23 would be wrong.

## Resolution

`ActorModelName` preserves kind E and type id 123 for enemy ownership but
selects `B0023_00` exactly as the shipping special case does. The movement
selftest proves E123 -> B0023 and ordinary E23 -> E0023. The continuous
offscreen, silent, uncapped story gate loads `enemy123_1` as `B0023_00`,
defeats it, and settles at `sccnt=19`.
