# RE frontier — what is the engine's, and what is not

`docs/codemap.md` says what exists. This says how much of it is **real**.

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

## Port choices

| what | the choice | why the engine's answer is unavailable |
|---|---|---|
| Engine-placed NPC positions | deterministic scan of chip centres for walkable floor nearest the room centre, one actor per chip | the chip attribute array and `GameRandom` are not reversed. What IS faithful: that the ENGINE places these, not the script (`AddNPC` @ `0x2c8a10` sets a flag when the script's x and z are both 0) |
| Which AI state means "pursue" | state 0 | the mode bodies that would say are not reversed. State 0 is the state the engine resets to, and the states an enemy can never leave (weight sum 0, 336 of 856 descriptors) would be absurd as a permanent chase. Everything about the TIMING is the engine's; only this mapping is not |
| Talk reach | one chip, 30 units | the engine's own talk trigger is not reversed; 30 is the game's fundamental spatial unit rather than an invented number |
| Player i-frame duration | 30 frames | `AppCharacterPlayer::DamageProcess` reloads it from a per-character field the port cannot source. The RULE — that damage is refused while the timer runs — is the engine's |
| HUD layout | corner readout | `ModeGame::Draw_StatusData` @ `0x2f1098` draws the real one and is not reversed. Every WORD is the game's (`SYS_COMMON_STATUS_LABEL_*`) |
| Level-up screen layout | centred panel | same; every word is `SYS_LEVELUP_TYPE_*` and `SYS_HELP_LEVELUP_*` |
| `GameRandom` | fixed-seed `mt19937` with the same range contract | `GameRandom` @ `0x3da480` is not reversed. Every roll's SHAPE is the engine's; the sequence is not |
| Damage floor at 1 | ~~port choice~~ | **retired** — it is the engine's, `cmp w8, #1` / `csinc` @ `0x2b349c` |

## Not reversed

| what | what is known | what is missing |
|---|---|---|
| **Enemy AI** | the architecture: a 27-way switch on the AI type sets a movement mode at actor `+0x3934` and a byte at `+0xc7b`, feeding a second switch at `0x2a95b4`. the 27-case jump table fully mapped: 4 handlers set a mode outright, 23 run one parameterised roll idiom, and **all 27 are now resolved** — the last 7 needed hand reading because they roll a SECOND time and pick among 2-3 modes, so "the pass mode" was a malformed question. Validated against every hand-read case, the mode switch's **dispatch** mapped (modes 0/1/2 share one body — the one 59 of 107 enemies reach), that body's event-box loop read (a floor-type state machine over terrain regions, not pathing), and the state-timer block at actor `+0x377c` — exactly 2 records of 140 bytes, filled from `enemydat.bin` `+0x80..+0x194` by `SetAITblFromEnemyTbl` (53 of 53 stores accounted), int/float split taken from the engine's own load widths and checked both ways over all 107 records. The **duration roll is confirmed from a second path**: the engine rolls `{base,range}` at runtime from `actor+0x377c + state*8`, and `SetAITblFromEnemyTbl` writes those slots from file `0x90/0xa8/0xc0/0xd8` — exactly the `descriptor[st]+0x10` the port reads, so the port's timings match the engine's | actor `+0xc68`, a factor in BOTH the movement equation and the distance timer. Bounded search: not a fixed-offset store, not one of `ChrSetData`'s 141 slots, not in `Update`, and not one of the 25 resolvable register-indexed stores across 313 character functions — the residual is 16 unresolved ones. Also the DISTANCE-driven timer: mode 9 computes a state's length as distance/speed at `0x2aa73c` instead of rolling `{base, range}`, and the port implements only the rolled form. Also **the state machine is otherwise fully specified** — 4 states, weighted-roulette transitions, per-state durations — so what remains is what each state *does* (the mode bodies; modes 4, 5, 6, 8, 9 and 10 are now read and turn out to be **one shared body** parameterised by a float and an optional motion-length floor, leaving only mode 3 and the modes 0-2 body's deeper tail),  The mode switch is now fully MAPPED: seven distinct bodies (0-2, 3, 4, 5, 6/7/10/11, 8, 9), and modes 7 and 11 are **dead** — all 17 stores to `+0x3934` are inside `UpdateAI` and none writes them. Six bodies are reachable and one (0-2) is read, so five remain. Still the biggest gap and the one that most changes how the game plays |
| **Save / load** | the SHAPE (a flat ordered byte stream, one `memcpy` per field, no tags or lengths); the direction flag at `0x422368`, buffer at `0x422388`, cursor at `0x422374`; the two names via `SaveAccessStr(oG+0x68)` and `(oG+0xe8)`; the first field run `oG+0x168..0x1c6`; the four inventory bags at `oG+0x1c8` stride `0xc` with the counter at `oG+0x3c8`; and the current cell `{col,row,world}` at `ModeGame+0x9b18`. `_GameSaveAccess` is `0x30c820..0x312cbc`, 25,760 bytes with a single `ret` | the rest of that function: map flags, the per-room enemy-dead bits `ClearRoomEnemyDead` touches at `+0x414..+0x438`, and the 8 KB block `Init` memsets at `+0x444`. The port always starts a new game at level 1 |
| Charge meter gate | it fills at `will * 100/s`, tops at 16000, a swing spends it, and it multiplies damage | `AppCharacterPlayer::IsUpSpGauge` @ `0x2b6228` decides WHEN it may fill. The port leaves the meter at the 0 a new game has — the neutral 1x |
| Enemy weaknesses | four gating bytes at the enemy record's `+0xa5c`..`+0xa5f`, each paired with an attack-type id, quarter the defence on a match | the attack-type ids. The port never sets `weak` |
| Life steal | attack param `+0x28 == 0x6d` heals the attacker `damage / 4` | the attack-type ids again |
| Shop / inn / ring menu UI | all the DATA: item names (`ITEM_NAME_<id>`), buy and sell prices, categories, and the `@I`/`@P` message slots the dialogue uses | the screens |
| Text control code `@<digits>` | it indexes a caller-supplied argument array | every caller on the dialogue path passes NULL. 1 of 393 strings is affected |
| CJK glyphs | the font atlas is ASCII 32..126 | the original draws CJK with the **Android system font**, which is not in the archive. Japanese text decodes and expands correctly and cannot be drawn |
| enemydat `+0x51`, `+0x53`, `+0x58`, `+0x5c` | where `SetEnemyId` puts them | what they are |
| `tblItem` `kind` | — | no consumer found; Candy/Ether/Elixir share kind 1 while Potion and Hi-Potion are kind 2 |
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
| "The AI's event-box loop uses 30.0 as a chip-scaled reach" | written one commit earlier from the fact that 30.0 is loaded just before the branch into the loop. It is never read there — across `0x2a9618`..`0x2a9cd4` those callee-saved registers are only reassigned. A constant being *live* is not a constant being *used* |
| "`DataTableGetIdType` returns type 1 for ids 1..38" | 1..37. `cmp w8, #0x25` is 37 and `b.hs` leaves on `>=`, so `id-1 <= 36`. The self-test asserted the wrong bound too, because it was written from the same reading — `item_table.py`, which gets the range from a *different* function (`DataTableGetItem`), is what caught it |
