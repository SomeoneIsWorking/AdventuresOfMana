---
id: 12
status: resolved
symptom: Jackal moves but never performs a collision-bounded scripted attack
tags: [gameplay, scripting, combat, collision, bosses]
---

# Jackal never entered a real damaging attack phase

## Root cause

`GetGroundAttribute` returned zero, so the Jackal script could not see the
authored EX_1 arena boundary that enables its charge. `ISHITMAP` likewise never
changed because scripted movement did not query physical walls.

The first attempted wiring used `.scol` triangle masks as ground flags. That was
wrong: script flags live in the room's `.gdt`, a 7.5-unit u32 grid. M0001's GDT
has EX_1 around the arena boundary. Its physical walls are `.scol` class 1
(`0x2`), falsifying the previous `0x18`-only wall mask; class 1 is also used by
floors, so normal orientation must discriminate them.

Two combat defects initially made the trace lie. Bosses still received the
host's always-live generic attack volume, and the player-damage branch bypassed
one-hit-per-swing. Jackal also requests absent bone `c_spine`; native
`GetBoneMatrix` is exact `strcmp` and returns null. The collision path uses
`SiModelBase::GetBoneIDByName`, whose exact search returns ID 0 on a miss;
B0000 bone 0 is `y_ang`, so fuzzy-matching `c_spine_a` would invent behavior.

## Resolution

The C++ GDT parser validates the engine's five header fields and exact payload
size, then serves room-local attributes to Lua. Script movement tests steep
wall candidates in `.scol`, sets `ISHITMAP` for the following script frame, and
reports the first collision per actor. Bosses no longer receive generic attack
volume 0. Lua attack-volume false-to-true edges advance the swing ID, and the
dedup gate now precedes both defender branches. An unresolved attack bone uses
the engine's bone-0 fallback and is counted in the runtime summary.

The shipping 600-frame boundary run steers the player to local `(30,30)`, an
authored EX_1 cell. It observes `_BOSS` hit the map, 109 scripted-volume
overlaps, two landed attack phases, and six player damage. The self-test also
checks GDT EX_1 set/clear classes, arena-wall/open-floor collision, and blocked
movement's `ISHITMAP` result.
