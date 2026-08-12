# Open questions

`docs/re-frontier.md` says what the port *ships* and how honest each piece is.
This file is narrower: the specific **unanswered reversing questions** that are
currently blocking something, plus the findings that are recorded but have **not
been adversarially verified** — so nobody has to re-derive which is which.

The rule for this file: an entry names what would answer it, not just what is
unknown. An entry with no method is a worry, not a question.

## Not adversarially verified

These are single readings. Everything else quoted in `docs/assets.md` had a
second agent try to refute it; these did not, because the verifier died mid-run.
Treat them as leads, not as established.

| claim | where | what would settle it |
|---|---|---|
| The collision meshes come from `%s.scol` (format string `0x98879`), loaded by `ModeGame::Process_Room` @ `0x2e4c88`..`0x2e4d94` and `MapServer::AddCollisionFromFile` @ `0x32f644` — the only two sites binary-wide that construct a `SiCollisionMesh` and feed it bytes | `docs/assets.md`, chip-grid section | re-read those two ranges and re-run the `bl`-to-PLT enumeration for the ctor and `SetBinary` stubs; check `SiCollisionMesh`'s vtable for a second loader, which the grep could not see |
| `IsCollisionPushBack`'s fourth argument is an **iteration count**, not a mask (`cbz w20` skips both passes; `cmp w24, w20` bounds each refinement loop); the mesh mask is hardcoded `0xa` | same | read `0x3300a8`..`0x33022c` in full and check the register the loop bound comes from |
| The two low bits of the chip attribute byte are "mask-1 floor hit" and "mask-4 floor hit", and the masks are bitmasks ANDed against a per-polygon attribute word at polygon `+0x24` | same | decode what layers those bits name — the polygon attribute word's own meaning is untouched. This is not cosmetic: `_MakeRouteTable` requires two chips' low-2 bits to be EQUAL, so this defines where actors cannot path |

## Unanswered, and blocking

Each of these was put to a static-RE pass that did not complete (six of nine
agents died on an auth error). The disassembly is at `scratch/raw/full.asm`; the
method that works is in the workflow brief — match `bl 0x... <NAME@plt>` rather
than target addresses, and bisect every hit against `scratch/raw/nm_sorted.txt`
to find its containing function.

| question | why it blocks something |
|---|---|
| **actor `+0xc68`** — every writer and reader, and the layout of `+0xc60..+0xc80` | it is a factor in BOTH the AI movement equation and the distance timer. Named as the biggest AI gap in `docs/re-frontier.md` |
| **`ModeGame+0x958`** and the party system around it | AI state 2 reads it while walking the route tables; the port has no party at all |
| **The save format** — every field, offset and width in `_GameSaveAccess` (`0x30c820`..`0x312cbc`) | the title menu's Continue and Load items are refused because there is no save file to offer. Beware: an earlier attempt disassembled past the end of the function and read a neighbour's code as part of the walk |
| **The event-box engine side** — storage, the containment test, how the named Lua callback is invoked, what `SetEventBoxNoTouchEvent` changes | `AddEventBox` is the most-called script function (552 calls). The port's own version is edge-triggered on an XZ AABB; whether that is the engine's is untested |
| **Room transition and blocking** — what happens when the player walks toward a cell whose record has no room name | the port clamps; the engine's answer is unread |

## Known-different, deliberately

Not questions — implemented departures, each already named at its call site.
Listed here only so they are not rediscovered as bugs.

- The chip grid carries no character/AppObject occupancy bits (`CheckAddPos`'s
  bit 6 and bit 7, via a radius-12 push-back sphere), and does not separate the
  two floor-mask classes in bits 0-1.
- The chase is a breadth-first distance field; the engine's `_MakeRouteTable` is
  a depth-limited DFS flood fill from the goal.
- `Load_GroundAttribute` sizes the `.gdt` with `frintp` (ceil) while every
  chip-grid reader truncates. Counted: **0 of 1000 rooms** have a size that is
  not a multiple of 30, so the two agree everywhere. Latent, not live.
