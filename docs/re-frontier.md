# RE frontier — what is the engine's, and what is not

`docs/codemap.md` says what exists. This says how much of it is **real**.
`docs/open-questions.md` is the working list of what is still unanswered.

The rule this file exists to enforce: on a reimplementation project the cardinal
sin is faking a step's output before its RE is done, because that makes a broken
port look finished. Everything below is one of three things.

- **RE-VERIFIED** — read out of `libmcfandroid.so` and checked on real data.
- **PORT CHOICE** — the engine's own answer is not reversed, so the port picked
  one, said so at the call site, and picked something defensible rather than a
  magic number.
- **NOT REVERSED** — a gap, named. Nothing stands in for it.

Every claim below is greppable: the port marks these in the source with
`PORT CHOICE`, `STOPGAP` or `not reversed`, so this list can be rebuilt from the
code rather than from memory.

The structured entries below are the machine-readable execution frontier.
`python3 tools/re_frontier_check.py` validates their identities, dependencies,
cycles, statuses, and evidence; the full verifier proves a zero-entry parse is
rejected. The global `re_frontier.py` workflow can additionally render
`next`/`hacks` views when pointed at this file. The detailed tables later in
this file retain the derivations and corrections behind these entries.

## Runtime spine

### runtime.assets — Shipping asset ingestion
- status: re-verified
- deps:
- evidence: Full `tools/verify.sh` parses the exact 9,886-member archive corpus, rejects missing/wrong/extra members, and byte-compares generated tables.
- where: `src/mcf/`, `tools/asset/`, `tools/verify.sh`
- gap:
- notes: This is the ground-truth data layer used by every later runtime step.

### runtime.lua-api — Shipping Lua command surface and semantics
- status: re-partial
- deps: runtime.assets
- evidence: The binary-derived extractor proves exactly 200 registrations/names/implementations and its 199/200 negative fails; 714/715 scripts execute against the bridge. Binary-derived `AddBox`, `OpenDoor`, `AddItem`, `IsAddItem`, and `DelItem` semantics now drive the Matock chest through the shared runtime inventory.
- where: `src/engine/script.cpp`, `src/engine/cmd_api.inc`, `docs/cmd-api.md`
- gap: Many commands still use typed recording stubs; implement semantics in shipping call-frequency and progression order.
- notes: `AddEnemyZaco` is implemented from its binary wrapper and target function, including late placement.

### runtime.world — Rooms, collision, actors, and transitions
- status: re-partial
- deps: runtime.assets, runtime.lua-api
- evidence: The 993-room census has zero mesh/script failures; world-table sizes score 656/656; continuous gates exercise ordinary, FREE-door, mapjump, event-box, and paired vine transitions.
- where: `src/engine/world.cpp`, `src/host/main.cpp`, `docs/world-map.md`
- gap: Exact player/door contact primitives, empty-cell transition behavior, chip occupancy bits, and the engine's event callback policy remain unreversed.
- notes: Current 15/30-unit contacts are recoverable engine behavior represented by port choices, not final fidelity.

### runtime.random-placement — Engine random placement and RNG sequence
- status: hack
- deps: runtime.world
- evidence: Binary reads establish the random-placement trigger and chip-grid construction, but the port selects nearest-centre chips with fixed-seed `mt19937`.
- where: `src/host/main.cpp`, `src/engine/script.cpp`
- gap: Reverse `GameRandom` and `AddCharacterRandomPos` selection, then remove deterministic nearest-centre placement and the substitute RNG.
- notes: The shortcut is deterministic for tests but does not reproduce shipping choices.

## Playable progression

### progression.opening-heroine — New game through heroine joining
- status: re-partial
- deps: runtime.world
- evidence: Mandatory uncapped, silent, offscreen run starts unseeded, completes both Jackals, waterfall recovery, Bogard route, five upward/three downward vine traversals, three Myconids, `EnemyDead`, and Hasim's scene at settled `sccnt=12`.
- where: `src/host/main.cpp`, `tools/verify.sh`
- gap: Continue the authored execution spine from `sccnt=12`; internal progression evidence does not yet establish visual/behavioral parity with a reference Android run.
- notes: The gate proves continuity and state ownership, not whole-game completion.

### progression.bogard-with-heroine — Return to Bogard and receive the Matock objective
- status: re-partial
- deps: progression.opening-heroine
- evidence: Binary `ModeGame::AddParty` stores the party id at +0x40c and indexes the nine handle relocations at 0x3ea080; the mandatory unseeded run restores PARTY_HEROINE across the authored return route, completes Bogard's pendant/Matock scene, and settles at sccnt=14 offscreen with zero decoded audio.
- where: `src/engine/script.cpp`, `src/host/main.cpp`, `tools/verify.sh`
- gap: Party follow AI between room loads and visual/behavioral parity against an Android reference run remain unverified.
- notes: The story-continuity mechanism is live; this remains partial rather than claiming reference fidelity.

### progression.wendel — Reach Wendel and advance the main quest
- status: re-partial
- deps: progression.bogard-with-heroine
- evidence: The unseeded, fixed-step uncapped, silent, renderless-offscreen run acquires Mattock item 17 with the binary's seven-use count, crosses `M0011_00_00..02`, preserves the elevated `M0000_09_06/in_2` arrival floor without reversing, descends the authored `M0000_08_06` wall pairs, follows the collision-proven route to Kett, climbs its three visible 30-unit steps, enters `M0012_00_00/bed_01`, acquires Cure, removes the kidnapped heroine, leaves through the shipping `out_1` mapjump, defeats all five scenario-15 lizardmen across stacked floors, takes the Warrior regimen through the real level-up path, acquires and equips Silver Key item 30, crosses `M0000_14_08/in_01`, recruits the Red Mage in `M0013_03_01`, traverses the connected row-0 dungeon route, presses `M0013_02_00/sw_01`, enters the newly enabled `down_1`, rejects exact tangency with `M0013_00_04/left_1` as arrival-floor overlap, grounds at y=90, climbs the upper log into `left_1`, returns to the west side of `M0013_02_00`, and reaches `M0013_01_00` in 12,369 frames with zero decoded audio.
- where: `src/host/main.cpp`, `tools/verify.sh`
- gap: Resolve the disconnected authored `down_01` target in `M0013_01_00`, then continue through the remaining Hydra/Mirror dungeon route and boss progression toward Wendel. General AppObject collision shapes/policies beyond proven breakable flag `0x08` remain debt.
- notes:

## Gameplay systems

### gameplay.enemy-ai — Ordinary enemy behavior
- status: re-partial
- deps: runtime.world
- evidence: State transition/timing ownership and state 2 pursuit are binary-derived; all 856 descriptor machines pass exhaustive roulette tests.
- where: `src/host/main.cpp`, `docs/assets.md`
- gap: Implement the five unread reachable mode bodies, distance-driven mode-9 timer, route-table seeding, party targeting, occupancy bits, and exact DFS route construction.
- notes: This is the largest behavior-fidelity gap.

### gameplay.player-combat — Player damage, attacks, rewards, and progression
- status: re-partial
- deps: runtime.world
- evidence: Binary-derived damage/reward formulas and live volume discriminators pass; continuous gates defeat real scripted waves and advance `EnemyDead`.
- where: `src/host/main.cpp`, `src/engine/world.cpp`
- gap: Reverse i-frame duration source, charge-meter gate, attack-type ids, weaknesses, and life steal.
- notes:

## UI and persistence

### ui.status — Shipping status HUD
- status: hack
- deps: runtime.assets
- evidence: Labels and values are shipping data, but the corner layout is explicitly host-authored.
- where: `src/host/main.cpp`
- gap: Reverse and implement `ModeGame::Draw_StatusData` @ `0x2f1098`, then remove the authored corner readout.
- notes:

### ui.level-up — Shipping level-up screen
- status: hack
- deps: gameplay.player-combat
- evidence: Regimen data and effects are binary-derived, but the centred panel layout is host-authored.
- where: `src/host/main.cpp`
- gap: Reverse the shipping screen and input flow, then remove the authored panel.
- notes:

### ui.menus — Ring, shop, and inn interfaces
- status: todo
- deps: runtime.lua-api
- evidence: Shipping item/category/price/string data are decoded.
- where: `src/host/main.cpp`, `docs/assets.md`
- gap: Reverse and implement the actual screens and their runtime flows.
- notes:

### persistence.save-load — Save and load
- status: todo
- deps: runtime.lua-api
- evidence: `_GameSaveAccess` stream shape, core player/inventory fields, names, and current room cell are partially mapped in the binary.
- where: `docs/open-questions.md`, `docs/re-frontier.md`
- gap: Map every field and width, room enemy-dead flags, and the 8 KB block before enabling Continue or Load Game.
- notes: Title entries remain unavailable because no trustworthy save format exists.

## Port choices

| what | the choice | why the engine's answer is unavailable |
|---|---|---|
| Engine-placed NPC positions | deterministic scan of chip centres for walkable floor nearest the room centre, one actor per chip | `GameRandom` is not reversed, so the engine's CHOICE among the walkable chips cannot be reproduced (the grid itself now is). What IS faithful: that the ENGINE places these, not the script (`AddNPC` @ `0x2c8a10` sets a flag when the script's x and z are both 0) |
| Enemy pathing | breadth-first distance field over the chip grid, rebuilt each frame from the player's chip | the engine's `_MakeRouteTable` @ `0x2a7c5c` is a depth-limited DFS flood fill from the goal, gated on a height band around the GOAL chip (`|dh| < 5`) and on the goal chip's attribute byte. The port's grid is built the engine's own way (a collision probe per chip centre — see `docs/assets.md`); what differs is the fill order and the two occupancy bits |
| Chip occupancy bits 6/7 | not modelled — a chip is walkable iff the floor probe hits | `CheckAddPos` clears bit 6 when a character stands on the chip and bit 7 when an AppObject's AABB blocks it, using a radius-12 sphere through `IsCollisionPushBack`. The port enumerates neither characters nor objects at grid-build time |
| Wander radius `s10` | not applied | it is a per-mode value (4/5/6 chips) and the port has no mode word. The widest candidate in the engine's own +-4 x +-3 window is 5 chips, so the gate only ever rejects when `s10` is 4, and then only the far corners |
| Wander reachability | destination must be on the floor | the engine requires `_MakeRouteTable` to find a route; the port has no route table, so it cannot see a wall between here and there |
| Talk reach | one chip, 30 units | the engine's own talk trigger is not reversed; 30 is the game's fundamental spatial unit rather than an invented number |
| Player `WALL_UP` / `WALL_DN` contact volume | 15-unit character radius | `AppCharacterBase::_AppEvMove` proves event collision resolves a character against the box, but the precise player collision primitive is not separately reversed. The port uses the same established 15-unit half-height/radius as ordinary event-box centre probing. Flag direction, paired volumes, and destination floors are authored: `EvBoxWallUp` stores floor-1 and `EvBoxWallDn` stores floor-14 |
| Door contact width | one chip (30 units) to either side of the centred `SetDoor` object | `_SetDoor` @ `0x2ce43c` proves the object is centred on the selected room side, and the shipping `eDoor.FREE` comment says body contact opens it. The precise object collision volume has not been reversed; the room's static `.scol` retains its boundary wall, so the port uses the game's fundamental chip width for contact. Door type, side, adjacent world-table cell, and FREE-vs-KEY gate are the engine's |
| Player i-frame duration | 30 frames | `AppCharacterPlayer::DamageProcess` reloads it from a per-character field the port cannot source. The RULE — that damage is refused while the timer runs — is the engine's |
| HUD layout | corner readout | `ModeGame::Draw_StatusData` @ `0x2f1098` draws the real one and is not reversed. Every WORD is the game's (`SYS_COMMON_STATUS_LABEL_*`) |
| Level-up screen layout | centred panel | same; every word is `SYS_LEVELUP_TYPE_*` and `SYS_HELP_LEVELUP_*` |
| `GameRandom` | fixed-seed `mt19937` with the same range contract | `GameRandom` @ `0x3da480` is not reversed. Every roll's SHAPE is the engine's; the sequence is not |
| Damage floor at 1 | ~~port choice~~ | **retired** — it is the engine's, `cmp w8, #1` / `csinc` @ `0x2b349c` |

## Not reversed

| what | what is known | what is missing |
|---|---|---|
| **Enemy AI** | the architecture: a 27-way switch on the AI type sets a movement mode at actor `+0x3934` and a byte at `+0xc7b`, feeding a second switch at `0x2a95b4`. the 27-case jump table fully mapped: 4 handlers set a mode outright, 23 run one parameterised roll idiom, and **all 27 are now resolved** — the last 7 needed hand reading because they roll a SECOND time and pick among 2-3 modes, so "the pass mode" was a malformed question. Validated against every hand-read case, the mode switch's **dispatch** mapped (modes 0/1/2 share one body — the one 59 of 107 enemies reach), that body's event-box loop read (a floor-type state machine over terrain regions, not pathing), and the state-timer block at actor `+0x377c` — exactly 2 records of 140 bytes, filled from `enemydat.bin` `+0x80..+0x194` by `SetAITblFromEnemyTbl` (53 of 53 stores accounted), int/float split taken from the engine's own load widths and checked both ways over all 107 records. The **duration roll is confirmed from a second path**: the engine rolls `{base,range}` at runtime from `actor+0x377c + state*8`, and `SetAITblFromEnemyTbl` writes those slots from file `0x90/0xa8/0xc0/0xd8` — exactly the `descriptor[st]+0x10` the port reads, so the port's timings match the engine's. `+0xc68`, the remaining movement/timer multiplier, is the base constructor's fixed 1.0 (`str q0, [this+0xc64]` @ `0x2a63dc`). | the DISTANCE-driven timer: mode 9 computes a state's length as distance/speed at `0x2aa73c` instead of rolling `{base, range}`, and the port implements only the rolled form. Also **the state machine is otherwise fully specified** — 4 states, weighted-roulette transitions, per-state durations — so what remains is what each state *does* (the mode bodies; modes 3, 4, 5, 6, 8, 9 and 10 are now read and turn out to be **one shared body** parameterised by a float and an optional motion-length floor, and the modes 0-2 body's tail is now read too — it decrements `+0x3910`, and on floor type 1 clamps the state to 1 and forces the mode to 1, then joins the same roll. **Every** mode ends in the shared roll. The state dispatch is also read: states 0/1 roll plainly, state 2 walks the route tables, state 3 calls `UpdateAI_TargetPos`. What remains is what seeds `MakeRouteTable`, and the meaning of the per-mode float and `party[0x958]`),  The mode switch is now fully MAPPED: seven distinct bodies (0-2, 3, 4, 5, 6/7/10/11, 8, 9), and modes 7 and 11 are **dead** — all 17 stores to `+0x3934` are inside `UpdateAI` and none writes them. Six bodies are reachable and one (0-2) is read, so five remain. Still the biggest gap and the one that most changes how the game plays |
| **Save / load** | the SHAPE (a flat ordered byte stream, one `memcpy` per field, no tags or lengths); the direction flag at `0x422368`, buffer at `0x422388`, cursor at `0x422374`; the two names via `SaveAccessStr(oG+0x68)` and `(oG+0xe8)`; the first field run `oG+0x168..0x1c6`; the four inventory bags at `oG+0x1c8` stride `0xc` with the counter at `oG+0x3c8`; and the current cell `{col,row,world}` at `ModeGame+0x9b18`. `_GameSaveAccess` is `0x30c820..0x312cbc`, 25,760 bytes with a single `ret` | the rest of that function: map flags, the per-room enemy-dead bits `ClearRoomEnemyDead` touches at `+0x414..+0x438`, and the 8 KB block `Init` memsets at `+0x444`. The port always starts a new game at level 1 |
| Charge meter gate | it fills at `will * 100/s`, tops at 16000, a swing spends it, and it multiplies damage | `AppCharacterPlayer::IsUpSpGauge` @ `0x2b6228` decides WHEN it may fill. The port leaves the meter at the 0 a new game has — the neutral 1x |
| Enemy weaknesses | four gating bytes at the enemy record's `+0xa5c`..`+0xa5f`, each paired with an attack-type id, quarter the defence on a match | the attack-type ids. The port never sets `weak` |
| Life steal | attack param `+0x28 == 0x6d` heals the attacker `damage / 4` | the attack-type ids again |
| Shop / inn / ring menu UI | all the DATA: item names (`ITEM_NAME_<id>`), buy and sell prices, categories, and the `@I`/`@P` message slots the dialogue uses | the screens |
| Text control code `@<digits>` | it indexes a caller-supplied argument array | every caller on the dialogue path passes NULL. 1 of 393 strings is affected |
| enemydat `+0x51`, `+0x53`, `+0x58`, `+0x5c` | where `SetEnemyId` puts them | what they are |
| 98 of 60,803 motion time arrays | they are non-monotonic, in 9 files | what that means |

## Corrections this project has had to make

Kept because a confidently wrong note costs more than no note.

| claim | what was actually true |
|---|---|
| "Dialogue text is missing from the extracted data entirely" | it ships in `str_en.bin` / `str_ja.bin`; the earlier search looked for CJK byte runs and inside per-room assets |
| "The collision AABB gives the room origin" (scored 116/116) | falsified — the AABB lo differs from the grid by multiples of 30 in 659/992 rooms |
| "303 objects are outside their room cell" | the TEST was wrong: 140 of 993 rooms span several cells. 3284/3284 objects are inside their room mesh |
| "The charge meter IS the attack power passed to the attack volume" | it is a MULTIPLIER; `SetCollisionAttackParam` stores it at param `+0x2c` and the attack power separately at `+0x30` |
| "The damage floor at 1 is a port choice" | it is the engine's |
| "AI state 0 is the pursue state" (port choice) | it is **state 2**, and this is now the engine's answer. State 2 calls `UpdateAI_TargetChr` (`vtable[0x428]`), which for a type-4 caller `SearchNear`s the player and party and takes the nearer, then walks the route table to it. A same-session guess that state 3 was the chase was also wrong: it calls `UpdateAI_TargetPos`, which searches type 4 — other enemies |
| "The start room is a port choice; `M0000_00_00`" | it is the engine's, and the guess was **wrong**. `ModeGame`'s ctor stores `{col, row, world}` at `ModeGame+0x9b18`; the new-game arm @ `0x2d2b8c` writes world = 1 and zeroes col and row, so a new game starts at world 1 cell (0,0) = `sk1/M0001_00_00` |
| "Room size where no `.gdt` exists is a port choice" | it is the engine's. `RoomSizeW` @ `0x2e3654` reads a size index from the cell record and looks it up in a per-world 8-entry table. Scores **656/656** against the `.gdt` ground truth the inference scored 654 on, and covers 344 rooms that have no `.gdt` |
| "A 272-record 16x17 world grid at `0xbd678`" (then retracted as an artifact) | the retraction was **wrong**, and the original reading was right for the wrong reasons. It IS a 16x17 grid of 272 cells — but the base is `0xbd5ec`, not `0xbd678`, and the record is `{int32 size_index; int32; char name[] at +8}` |
| "`M0000_00_00` at `0xbd5f8` is a separate literal in a different shape (no `sk1/` prefix)" | it is the tail of `"sk1/M0000_00_00"` at `0xbd5f4`, which is cell 0's own name. The linker merged the shorter literal into the longer one's tail; reading from `0xbd5f8` starts 4 bytes into the string |
| "Nothing references the table — 0 `adrp`+`add` pairs land in its 36KB" | the scan was right and the inference from it was wrong. Nothing takes its address because it is **copied into `ModeGame`** and read at `this+0xa64` with a computed index — exactly the access shape the scan said it could not see |
| "`enemydat` ids run 0..106" | three blocks: 0..73, 100..123, 201..209 |
| "Item sell price is half the buy price" | the ratios are 2, 4, 6 and 15 |
| "`DataTableGetIdType`'s ranges match each table's record count" | the weapon range is 101..118 and `tblWeapon` holds 101..117 and 121. Both sets have 18 members — the coincidence that let the check pass |
| "`+0xc7b` is a chase flag" | an invented name; its readers are a motion-driven state machine |
| "The save fields stop being uniform after field 20 and jump backwards into `oG+0x10`" | they do not — I had disassembled past the end of `_GameSaveAccess` and read another function's code as part of the walk |
| "`tblItem` `+0x10` is *probably* the restore amount" | it IS, and the guess is retired: `UseInventoryFunc` @ `0x2deb78` reads `DataTableGetItem(id) + 0x10` and adds it to HP or MP |
| "`GameParameter::AddItem(id, 1)` adds one of an item" | the second argument is a `bool`, not a count — the symbol is `AddItemEib`. It gates the write, so `false` is the dry run `IsAddItem` uses |
| "Items stack, and a slot's second word is the quantity" | nothing stacks. `AddItem` takes the first slot whose id is 0 without ever comparing the id being added against the ids already held, and the second word is the acquisition order, taken from a counter at `GameParameter+0x368` |
| "Several AI cases roll `GameRandom(100)` against a per-enemy probability at `+0x3894`" | `+0x3894` is an **index**, not a probability — it is multiplied by 140 (`smaddl`) to select a record in the AI parameter block at actor `+0x377c` |
| "The AI parameter block is not from `enemydat.bin`, because `SetEnemyId`'s memcpy lands past it" | true of that memcpy, false as a conclusion. `SetAITblFromEnemyTbl` @ `0x2a6cb0` is a second path out of the same file and copies `+0x80..+0x194` into the block. Two thirds of every enemy record is AI configuration that sat unparsed behind the wrong conclusion |
| "Japanese cannot be drawn — the atlas is ASCII 32..126 and the original draws CJK with the Android system font" | the atlas claim was true of `BasicFont.sfont` and the CONCLUSION from it was wrong. `BasicFont` is not what the engine draws UI text with: `FontFileLoad` @ `0x2c2608` loads `sk1/font_<lang>.bin`, which carries 543 characters in English and 1209 in Japanese, kana and all. The port draws with it now |
| "The AI's event-box loop uses 30.0 as a chip-scaled reach" | written one commit earlier from the fact that 30.0 is loaded just before the branch into the loop. It is never read there — across `0x2a9618`..`0x2a9cd4` those callee-saved registers are only reassigned. A constant being *live* is not a constant being *used* |
| "The per-chip height/attribute grids come from the room's `.gdt`, so the route table is buildable from what the port already parses" | they do not come from the `.gdt` at all. `ModeGame::MakeRandomChrPosTbl` @ `0x2dd0a0` builds both by raycasting the collision mesh once per chip through `CheckAddPos` @ `0x2dcd20`; an enumeration of every writer of `+0x9ba8`/`+0x9bb0` across the whole disassembly, in five encodings, finds exactly two — the ctor null-init and that function. Consequence: the port's `GetFloor`-at-chip-centre grid is essentially the ENGINE's method, not a substitution for it |
| "`_MakeRouteTable`'s `(w3,w4)` is the previous chip, re-based each step" | it is the **fixed goal cell** — saved to `w23`/`w24` @ `0x2a7d8c`/`0x2a7d94` and restored unchanged for all four recursive calls, and the wrapper reads `route[chipsW*w4 + w3]` at the end. So the 5-unit test is an absolute height band around the GOAL, not a per-step slope limit, and both attribute tests are against the goal chip. It is a depth-limited flood fill |
| "The occupancy probe sphere has radius 5" | 12. `mov w10, #0x41400000` @ `0x2dce44` is 12.0f. The `5` is `IsCollisionPushBack`'s fourth argument, and that is not a mask either — @ `0x3300a8` it bounds two refinement loops, with the mesh mask hardcoded `0xa`. NOT adversarially verified (the verifier agent died) |
| "`GetHeightMapData` is at `0x2dd004`" | `0x2dd010`. `0x2dd004` is inside `CheckAddPos` |
| "`DataTableGetIdType` returns type 1 for ids 1..38" | 1..37. `cmp w8, #0x25` is 37 and `b.hs` leaves on `>=`, so `id-1 <= 36`. The self-test asserted the wrong bound too, because it was written from the same reading — `item_table.py`, which gets the range from a *different* function (`DataTableGetItem`), is what caught it |
