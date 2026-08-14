---
id: 36
title: Healing spring wrote duplicate actor state instead of live player stats
status: resolved
symptom: M0013_11_00's shipping recovery spring called ChrSetData for MainPlayer HP and MP, but combat health and mana could remain unchanged because Lua accessed Actor fields while gameplay used PlayerStats.
tags: scripting,player-state,combat,hydra,tooling
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

`Script::ChrGetData` and `ChrSetData` treated every handle as an Actor. The host combat loop owns MainPlayer HP/MP in `PlayerStats`, so the Actor copy was not authoritative and MP lived only in generic actor data.

## Resolution

`Script` now receives the host-owned `PlayerStats`; MainPlayer HP/MAXHP/MP/MAXMP reads use it and HP/MP writes clamp into it while retaining the actor fallback for isolated scripts. The movement self-test executes the shipping `_HEALSPRING` coroutine from damaged live state through its authored wait and proves both resources refill. The full fixed-step uncapped offscreen/no-audio route consumes the Silver Key at the main gate, enters `M0013_11_00/Recovery`, waits for the coroutine to finish, and returns. Full `./tools/verify.sh` passed on 2026-08-14 with 51 movement cases and zero decoded audio.
