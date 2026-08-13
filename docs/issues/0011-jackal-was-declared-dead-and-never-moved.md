---
id: 11
status: resolved
symptom: The opening reaches battle music but Jackal remains exactly 105 units from the player
tags: [gameplay, scripting, combat, bosses]
---

# Jackal was declared dead and never moved

## Root cause

Three command-boundary defects combined in the first boss behavior:

1. `IsDead` in the shipping prelude reads `ChrGetData(HP)`. Combat seeding wrote
   `Actor::hp`, while `ChrGetData` read an unrelated sparse slot map and returned
   zero. The live Jackal was therefore declared dead before entering its loop.
2. `math_LerpSin`, used to compute every normal stride, was a generic numeric
   stub returning zero. The exported `LerpSinf` at `0x33f438` clamps the
   interpolated angle and returns `sin(angle*pi/180) * length`.
3. `ChrMotionForce` clears the old duration and the script immediately calls
   `ChrMotionGetEndFrame` in the same resume. Resolving `.smot` metadata only on
   the next draw left that immediate query at zero.

`math_atan2`, `bit_and`, and `ChrLookTarget` were also inert dependencies of the
same boss state machine.

## Resolution

HP/MAXHP script slots now read and write the live combat fields. The measured
math helpers and persistent look target are implemented. `Script` receives a
synchronous duration resolver from the archive-owning host, so a motion command
and its following end-frame query see the same shipping `.smot` immediately.

The Lua-bridge self-test covers nonzero and zero math classes, live HP reads and
writes, synchronous duration, incomplete/complete clocks, and forced restart.
The real opening gate now requires `_BOSS` itself to begin scripted movement.
In the 900-frame trace, closest distance changed from the stuck 105.0 to 70.8
units; this is observed behavior, not a self-test inference.
