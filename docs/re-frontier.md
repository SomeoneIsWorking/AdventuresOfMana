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
| Start room | `M0000_00_00` | a new game's start room comes from the save/new-game path; `GameParameter::Init` grants the starting equipment but no map, and `MapJump` is only ever called from Lua |
| Engine-placed NPC positions | deterministic scan of chip centres for walkable floor nearest the room centre, one actor per chip | the chip attribute array and `GameRandom` are not reversed. What IS faithful: that the ENGINE places these, not the script (`AddNPC` @ `0x2c8a10` sets a flag when the script's x and z are both 0) |
| Talk reach | one chip, 30 units | the engine's own talk trigger is not reversed; 30 is the game's fundamental spatial unit rather than an invented number |
| Player i-frame duration | 30 frames | `AppCharacterPlayer::DamageProcess` reloads it from a per-character field the port cannot source. The RULE — that damage is refused while the timer runs — is the engine's |
| HUD layout | corner readout | `ModeGame::Draw_StatusData` @ `0x2f1098` draws the real one and is not reversed. Every WORD is the game's (`SYS_COMMON_STATUS_LABEL_*`) |
| Level-up screen layout | centred panel | same; every word is `SYS_LEVELUP_TYPE_*` and `SYS_HELP_LEVELUP_*` |
| Room size where no `.gdt` exists | whichever of 300x240 / 330x270 puts the room's collision AABB at `size * grid_index` | the size table lives in `ModeGame` at `+0x9dc` and what fills it has not been found. Scored against the 656 rooms where the truth IS known: 654 agree, 2 disagree |
| `GameRandom` | fixed-seed `mt19937` with the same range contract | `GameRandom` @ `0x3da480` is not reversed. Every roll's SHAPE is the engine's; the sequence is not |
| Damage floor at 1 | ~~port choice~~ | **retired** — it is the engine's, `cmp w8, #1` / `csinc` @ `0x2b349c` |

## Not reversed

| what | what is known | what is missing |
|---|---|---|
| **Enemy AI** | the architecture: a 27-way switch on the AI type sets a movement mode at actor `+0x3934` and a byte at `+0xc7b`, feeding a second switch at `0x2a95b4` that works through event boxes and route tables. 4 of the 27 cases read | the mode switch itself. This is the biggest gap and the one that most changes how the game plays |
| **Save / load** | the SHAPE (a flat ordered byte stream, one `memcpy` per field, no tags or lengths) and the whole 92-byte `GameParameter` header: two names, then `oG+0x168`, then 20 x 4 bytes, then 4 x 2 bytes | everything past the header — inventory, map flags, per-room enemy-dead bits, the 8 KB block `Init` memsets at `+0x444`. The port always starts a new game at level 1 |
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
| "`enemydat` ids run 0..106" | three blocks: 0..73, 100..123, 201..209 |
| "Item sell price is half the buy price" | the ratios are 2, 4, 6 and 15 |
| "`DataTableGetIdType`'s ranges match each table's record count" | the weapon range is 101..118 and `tblWeapon` holds 101..117 and 121. Both sets have 18 members — the coincidence that let the check pass |
| "`+0xc7b` is a chase flag" | an invented name; its readers are a motion-driven state machine |
| "The save fields stop being uniform after field 20 and jump backwards into `oG+0x10`" | they do not — I had disassembled past the end of `_GameSaveAccess` and read another function's code as part of the walk |
| "`tblItem` `+0x10` is *probably* the restore amount" | it IS, and the guess is retired: `UseInventoryFunc` @ `0x2deb78` reads `DataTableGetItem(id) + 0x10` and adds it to HP or MP |
