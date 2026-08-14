---
id: 41
title: Treasure callback omitted the selected item payload
status: resolved
symptom: Opening the Chain Flail chest acquired item 104 but sccnt remained 19 indefinitely because _BOX saw no tmp_tresureitem
tags: tooling,lua,inventory,treasure,progression
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The host's live box-open path started the room's `_BOX()` coroutine directly.
The original engine first publishes the selected `AddBox` payload through the
Lua global `tmp_tresureitem`. Fourteen shipping room scripts branch on that
global, so the callback ran but silently took no branch when the host omitted
it.

## What was tried / dead ends

The unseeded story driver successfully reached `M0012_11_00`, opened the live
box, and added item 104. Continuing for another 414,034 fixed-step frames did
not change `sccnt=19`; route selection and inventory acquisition were therefore
not the cause. Inspecting the shipping room callback exposed the missing input
contract.

## Resolution

`Script::StartTreasureCallback` now publishes `tmp_tresureitem` and then starts
`_BOX` when the room defines it. Its shipping-path selftest proves both classes:
a room without a handler still receives the payload, and a handler observes
item 104. The mandatory silent, SDL-offscreen run opens the Chain Flail chest
and reaches settled `sccnt=20` at frame 21,961 with zero decoded audio frames.
