---
id: 5
title: Event-box callback names are not unique
status: resolved
symptom: Joined doorway triggers lost every volume except the final registration, and floor-level transitions could not fire
tags: gameplay,event-box,scripting
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

The script bridge treated an event-box name as a unique key and replaced an
existing volume when a second `AddEventBox` used the same callback. The engine's
`AppEventBoxServer::Add` stores each volume independently; the name is only the
Lua callback. The port also passed its floor-contact Y directly to the strict
event-box predicate, while the engine probes the character centre.

## What was tried / dead ends

Spawning inside `M0000_01_08`'s apparent trigger did not transition. A full-Y
diagnostic showed the surviving final box was `(150,120,90)..(180,150,120)`,
while the player stood at Y=120. Changing strict bounds would have contradicted
`AppEventBoxBase::IsHit @ 0x2bb0f8`; the actual correction is to retain both
boxes and test at centre height.

## Resolution

### Resolution (2026-08-13)

`AddEventBox` now appends each volume. Name-based enable, no-touch, and flag
mutators update every matching volume, matching the engine's enumeration loops.
The host continues to use the collision floor for player movement and rendering,
but probes event boxes at `floor + 15`. `--eventbox-selftest` checks both
same-name volumes and the strict floor/centre Y split. A headless run spawning
inside the second `M0000_01_08` volume logs `entered event box 'in_01'` and
`mapjump -> M0006_01_02`, then renders the destination room.
