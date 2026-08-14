---
id: 40
title: Headless Hydra route skipped Mirror and stalled after AfterBossEvent
status: resolved
symptom: The continuous story opened Fire but exited the Hydra room before Mirror, or chose an unreachable post-Hydra mountain edge
tags: tooling,navigation,story-driver,hydra,kett
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The deterministic story policy had a room-exit target for the Hydra arena but
no ownership of the two live reward boxes, so it left after the first
incidental Fire pickup. `AfterBossEvent` then spawns on the low component of
`M0000_14_08`; guessing west entered a disconnected elevated mountain loop
and could never reach Kett.

## Evidence

`EnemyDead` authors Fire first and Mirror second by `AddBox` order. Runtime
actor order retains that order. The route diagnostic proved the
`M0000_14_08` post-event component has reachable contact bands
up/right/down/left=0/48/244/108. South reaches `M0000_14_09`, after which west
across row 9 reaches Kett. West from `M0000_14_08` instead reaches
`M0000_13_08`'s elevated component; successive diagnostics proved its
apparent mountain route closes back on itself.

## Resolution

`StoryDriver` now targets each live unopened treasure actor in authored spawn
order, equips Mirror in the next free button without replacing Keyring or
Silver Key, then drives south to `M0000_14_09` and west to Kett. Selftests
prove Fire-before-Mirror and the first return edge; the continuous offscreen,
silent, uncapped gate proves both pickups, `AfterBossEvent`, the complete
return, and settled `sccnt=19`.
