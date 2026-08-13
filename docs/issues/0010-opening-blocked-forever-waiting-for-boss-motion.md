---
id: 10
status: resolved
symptom: The opening never releases the Jackal boss coroutine into combat
tags: [gameplay, scripting, animation, bosses]
---

# Opening blocked forever waiting for boss motion

## Root cause

`IsChrMotionFinish`, `ChrMotionGetFrame`, and `ChrMotionGetEndFrame` fell through
to generic zero-valued stubs. The opening called `MotionFinishWait(_BOSS)` after
Jackal's appearance, so false could never become true and `_boss_start_scene`
could never be set to 1.

The first implementation of a motion clock exposed a second part of the same
runtime path: `AddBoss` runs after the room's initial actor-cache pass. The late
Jackal had no renderable, so its `.smot` was never loaded and there was no real
duration to drive the clock. A 900-frame trace reported 843 skipped attacker
frames and never started BGM 2.

## Resolution

Every actor now has a motion frame and duration. `ChrMotion` resets the clock
when the motion changes, `ChrMotionForce` always resets it, the world advances
it in engine frames, and Lua frame/end/finish queries read it. Rendering samples
the same per-actor frame. Late actors load their model and motion assets on
demand; missing models are reported once rather than silently skipped forever.

The movement self-test exercises unfinished, exact-end, and forced-restart
classes through the shipping Lua bridge. The real 600-frame opening gate now
also requires the late `B0000_00` model load and BGM 2, which occurs only after
`MotionFinishWait` returns and the script releases combat.
