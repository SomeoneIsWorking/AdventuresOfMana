# Adventures of Mana — PC port: codemap

Source binary: `libmcfandroid.so` from `com.square_enix.adventures` v1.1.4 (arm64-v8a).
Engine is Square Enix / MCF's in-house C++ engine ("MCF" / "Si" prefixes). Not Unity.
The stock engine only; the 5play `libRMS.so` mod-menu injection is excluded from all work.

## Established facts

| Fact | Evidence |
|---|---|
| 4747 exported FUNC symbols, unmangled C++, 204 classes | `readelf --dyn-syms` |
| Renderer is GLES2, ~15 inline GLSL ES 1.00 shaders | strings in `.rodata` |
| Audio: OpenSL ES + Ogg Vorbis | DT_NEEDED + `LibSoundStorage_Ogg` |
| Scripting: Lua 5.3 + tolua++, one module `cmd`, **200 functions** | `docs/cmd-api.md` |
| Platform abstraction exists: `MCFSiPlatform` -> `MCFSiPlatform_Android` (sole backend) | symbol scan |
| Gamepad path already ships (Android TV / LEANBACK) | manifest + `SiController` |
| Assets: one `sk1.mpk`, magic `mcfa`, 9886 entries, custom codec (NOT zlib), no AES | `docs/mpk-format.md` |

## How real is it?

`docs/re-frontier.md` splits every subsystem into RE-VERIFIED, PORT CHOICE and
NOT REVERSED, and lists the claims this project has had to retract. Read it
before trusting anything below to be the engine's rather than the port's.

## Verification

`./tools/verify.sh` runs every asset parser over the full 9886-file corpus. It
refuses to run — rather than reporting a vacuous pass — if the corpus is missing.

## Subsystems

| Subsystem | Status | Where |
|---|---|---|
| `cmd` Lua API extraction | **DONE — verified 200/200 names+impls** | `tools/asset/extract_cmd_api.py` -> `docs/cmd-api.md` |
| MPK archive reader | **DONE — 9886/9886 extracted, validated** | `tools/asset/mpk.py`, `tools/asset/lha.py` |
| Texture format (`.stex`/`SMDI`) | **DONE — 1319/1319 descriptors parse, image verified** | `tools/asset/stex.py` |
| Model format (`.smdl`/`Smd3`) | **DONE — 1375/1375 parse, geometry rendered; material blend flag (word 9) drives alpha blending** | `tools/asset/smdl.py`, `render_smdl.py` |
| Motion format (`.smot`/`Smot`) | **DONE — 1721/1721 parse; quaternions verified. 98 of 60,803 time arrays unexplained** | `tools/asset/smot.py` |
| Collision format (`.scol`/`SCol`) | **DONE — cells + triangles reversed; floor queries work** | `tools/asset/scol.py`, `Collision::GetFloor` |
| Room data tables (`.odt`/`.gdt`/`.edt`) | **Layouts from the engine's own loaders; 1328/1328 files parse.** Room extent now comes from **the engine's own world table** (988 of 993 rooms; `.gdt` and the old AABB inference are fallbacks), so rooms are no longer assumed to be one 300x240 cell -- actor placement went 85/116 -> **116/116** on the floor. Map objects RENDER in-game (3284/3284 ids resolve to models); all 3284 object placements verified against room mesh bounds | `tools/asset/roomdata.py`, `tools/asset/object_table.py` |
| Title screen | **Ported and rendering.** Boot chain runs Init -> CESA -> MakerLogo -> Title -> Game. The attract screen and the three-item menu are the engine's own: the item ids come from the table `ModeTitle::Render` memcpys from `0xbd354`, in its order, and every word is the shipping string table's. Continue/Load Game are shown dimmed (no save format past the inventory). **Name entry is ported too**: two fields, typed live, validated by the engine's own rules (character set from `SYS_NAMEENTRY_USE`, length 4 in ja / 8 otherwise, length outranking the character check) with the engine's own error messages; `--nameentry-selftest` covers all three errors in both languages and is verified to fail. **The opening crawl is ported too**: 15 lines (loaded as 40 ids, stopping at the first empty one, as the engine does), the engine's own line step, fades, scroll rate and end condition, with Skip. Boot chain is now attract -> menu -> crawl -> names -> game | `src/engine/mode.{h,cpp}`, `src/host/main.cpp` |
| World map (which room is in which cell) | **Done and verified.** 32 worlds, 1879 grid cells, 1000 named rooms, read from `.rodata` at `0xbd564`. Every name encodes its own world id and (col,row) (1000/1000); sizes match the `.gdt` ground truth **656/656** against a control that scores 384/656. This is how the engine names a room -- by grid position, not by string | `tools/asset/worldmap.py`, `src/engine/world_table.inc`, `docs/world-map.md` |
| C++ asset layer | **DONE for MPK/.stex/.smdl** — port of the verified Python | `src/mcf/` |
| Desktop host (window + GLES2) | **Textured characters + rooms; GPU skinning + per-actor `.smot` playback.** Actors created by a running room coroutine load their renderable and motion assets on demand instead of remaining invisible | `src/host/main.cpp` |
| Lua bridge | **Lua 5.3 + all 200 `cmd` bindings; 714/715 scripts run** (stubs record calls) | `src/engine/script.cpp` |
| Actor system | **Live actors driven by scripts**, rendered and animated in their rooms. `ChrMoveTo`/`ChrMoveYTo` use the shipping `(speed,x[,y],z)` contract and advance asynchronously. Motion commands reset a per-actor clock whose duration comes from that actor's real `.smot`; frame/end/finish queries now drive `MotionFinishWait` and rendering from the same clock. `--movement-selftest` covers moving/stationary and unfinished/finished/restarted classes. Scripted movement still uses a straight segment rather than the engine's route table | `src/engine/world.{h,cpp}`, `src/engine/script.cpp`, `src/host/main.cpp`, `src/host/render.cpp` |
| Game loop + player | **Boots into the engine's new-game room (`M0001_00_00`) with no arguments**; real-time loop, WASD/arrow movement, floor + wall collision, WAIT/WALK, attack on Space/Z. Room `Init()` now runs as its own coroutine, advances from the engine-style game clock used by `wait`, and is cleared on map change without losing scenario globals. The opening Jackal receives the engine's `_BOSS` handle and its late spawn is initialized from `enemydat.bin` | `src/host/main.cpp`, `src/engine/script.cpp` |
| Camera | **Script-driven camera**: `eCamGetData` orbit slots plus distinct fixed target, target offset, character target and explicit eye-position commands. `--camera-selftest` executes both target classes through the live Lua bridge; the opening frame now shows the arena gate instead of the back of its wall. Camera collision/occluder handling remains missing | `src/engine/world.h`, `src/engine/script.cpp`, `src/host/main.cpp` |
| Game over | **The engine's own three-step sequence** (`Process_GameOver`): game-over BGM, `SYS_GAMEOVER_MSG` in the message window, 800 ms fade to black, then end. Only the last step differs — `SetNextMode(5)` is a title screen this port does not have | `src/host/main.cpp` |
| Fade | **Timed fades with the game's own shader**, verified to darken the frame | `src/host/main.cpp` |
| Combat volumes | **Attack arcs + damage spheres on bones; overlap predicate self-tested; fires in-game** (481 hits / 744 pairs on a real room; 0 hits at range, explained) | `src/engine/world.cpp`, `src/host/main.cpp` |
| Player stats | **`GameParameter` reimplemented from `Init` + `Update`** — level, HP/MP pools, EXP and next-level curve, money, the four stats with their caps, attack from the weapon table and defence from the armour tables. Both derived formulas reproduce `Init`'s own constants (`--player-selftest`). **The level-up screen is in**: earning a level opens it, the player picks one of the four regimens with up/down and confirm, and the stats, the full heal and the new maxima follow. Layout is a PORT CHOICE; every WORD is the game's (`SYS_LEVELUP_TYPE_*`, `SYS_HELP_LEVELUP_*`). Levelling decoded: `tblLevelup`'s four training regimens, the lane swap that applies them, and the full heal — cross-checked against the game's own help text. No save/load path, so this is always a new game at level 1 — though `Inventory::NewGame` now reproduces `Init`'s four starting grants | `src/mcf/mcf.h`, `docs/assets.md` |
| Enemy behaviour | PARTIAL — **combat is two-sided**: enemies attack with their real `enemydat.bin` attack power against the player's real equipment defence, gated by the engine's reversed i-frame rule. real HP/defence/rewards from `enemydat.bin`; damage, death and drops work. Movement is a placeholder (closes on the player, no mutual separation) but now at each enemy's OWN speed from `enemydat +0x68`, with speed-0 enemies standing still. The AI type (`+0x64`) is carried but not acted on. `UpdateAI`'s architecture IS mapped — 27 short selectors that set a movement mode (`+0x3934`) and a second, unidentified byte (`+0xc7b` — an earlier note called it a chase flag, which was an invention and is retracted), feeding a second switch at `0x2a95b4` that does the work via event boxes and route tables — and 4 of the 27 cases are read. The mode switch is the next step; 59 of 107 enemies are type 0, which is one line. **The engine's real damage formula is now implemented** — magical attack added, charge meter as a multiplier, a 0..24% roll, and the engine's own floor at 1 — with the charge meter left at its new-game 0 (its fill rate `will * 100/s` and its 16000 top are decoded; the `IsUpSpGauge` gate that says *when* it may fill is not) and enemy weaknesses left off (their attack-type ids are not decoded). Kill rewards carry the engine's 0..10% roll. Faction filter from `GetType` | `src/host/main.cpp` |
| Event boxes + transitions | **Edge-triggered boxes run handlers as coroutines; `mapjump` loads the destination room.** One callback name can own multiple physical volumes (all retain their callback and name-wide setters update all matches). Containment is the engine's strict enabled 3D test, made at the player centre 15 units above its collision-floor contact; no-touch boxes cannot fire. A live `M0000_01_08` trigger loads `M0006_01_02` | `src/engine/world.h`, `src/engine/script.cpp`, `src/host/main.cpp` |
| Audio | **BGM + SE play, driven by scripts** | `src/engine/audio.cpp` |
| Status HUD | **HP/MP, GP/EXP, ATK/DEF/Lv drawn in the corner**, labels taken from the game's own `SYS_COMMON_STATUS_LABEL_*` strings so it follows `--lang`. Layout is a PORT CHOICE — `ModeGame::Draw_StatusData` is not reversed, so this is a plain readout rather than a fake of the game's UI. `--no-hud` hides it | `src/host/main.cpp` |
| Text / dialogue | **NPC conversations work in-game, with the control codes expanded** (`CnvFormatString`: `@N(n)` -> a `CHARACTER_NAME_n`, `@H`/`@G` -> the hero/girl names, `@P @i @I @S` -> the script-set parameter slots; 392 of 393 strings expand fully, `--text-selftest`) — walk up, press Space, lines advance one at a time. Drawn on screen and waits for the player (`msgId`'s `coroutine.yield` is honoured). 146 lines fire across the shipping scripts with 0 unresolved ids. — 1906 strings in en and ja, rendered in the game's own `font_<lang>.bin` with word wrap — 543 glyphs in English and 1209 in Japanese, so kana, the copyright sign and the em dash all draw. `BasicFont.sfont` (ASCII 32..126) remains only as the fallback | `tools/asset/strings.py`, `src/mcf/assets.cpp`, `src/host/main.cpp` |
| PNG | **Own decoder, own inflate** (`src/mcf/png.cpp`) — the project has no zlib, and the boot art is plain PNG rather than `.stex`. Handles 8-bit RGBA non-interlaced, which is every PNG the boot path touches, and refuses anything else by name. Cross-checked against Python's `zlib` via `tools/asset/png_check.py`: identical hashes on 4 shipping images, 2.6M pixels. `--png-selftest` also proves it refuses 6 malformed/unsupported inputs | `src/mcf/png.cpp`, `tools/asset/png_check.py` |
| Boot chain | **The engine's real mode path, ported**: ModeInit -> ModeCESA -> ModeMakerLogo -> ModeTitle -> ModeGame, with the EMODE values taken from `ApplicationMode::ProcessMain`'s factory switch and each transition from the `SetNextMode` argument in that mode's own `Process`. `--boot` runs it and **now draws the real art** — the maker logo and the title logo, via the port's own PNG decoder, aspect-fit. ModeCESA stays blank because its art is in neither the archive nor the assets root. ModeTitle waits for the player as the engine does (headless runs auto-advance). `--mode-selftest` drives the chain rather than asserting a list; `--shot-mode NAME` captures a splash screen, which `--screenshot` cannot since it counts gameplay frames. Still opt-in while the title menu and name entry are unreversed. Game over now returns to ModeTitle as the engine does | `src/engine/mode.{h,cpp}` |
| Enemy AI | **The state machine is in and driving ordinary enemies; bosses remain owned by their map-script `_BOSS` coroutines.** Four states per machine, two machines per enemy, all from that enemy's own `enemydat.bin` record: per-state `{base, range}` durations counted down in frames, and transitions chosen by the engine's weighted roulette (`NextAiState`, verbatim from `UpdateAI` @ `0x2a8d50`). `--ai-selftest` proves both ownership classes, sweeps every roll of all 856 shipping descriptors and asserts each state is selected exactly its weight's worth of times; verified to fail on a `<=` and on swapped weights. Observed in a headless run: enemy 26 oscillates 0->1->0 on its real 1.5s/7s timers. Which state pursues is now the ENGINE's answer, not a port choice: **state 2**, which walks the route tables (state 0 idles, state 1 wanders, state 3 spaces against other enemies). State 1's wander is implemented — the same +-4 x +-3 window, 126 attempts and chip-centre destination — and took a headless combat run from 0 landed hits to 231. Chasing follows a chip distance field built the engine's own way, a collision probe per chip centre, with its goal-relative height band. The mode bodies behind each state are mapped but five of six are still unread | `src/mcf/assets.cpp`, `src/engine/world.h`, `src/host/main.cpp` |
| Inventory | **The four bags reimplemented from the engine's own accessors** — items (16 slots, stride 12), weapons (16), a bag SHARED by helms/armour/accessories (16), and magic (8, direct-indexed off the id). They tile `GameParameter+0x168..+0x368` with no gaps, and the layout is confirmed by three independent functions (`IsHaveItem`, `AddItem`, and the save walk). **Nothing stacks**: `Add` takes the first free slot and fails when the bag is full. Slot word 1 is the acquisition order from the counter at `+0x368`, not a quantity. `--inventory-selftest`, 47 cases, verified to fail on a sabotaged range bound and on a stacking `Add`. Not wired to a UI — the ring menu and shop screens do not exist | `src/mcf/mcf.h`, `src/mcf/assets.cpp` |
| Game data tables | **Enemy table live in-game** (`enemydat.bin`, 107 records: max HP, defence, EXP, money each pinned to an engine read) and **`tblWeapon` extracted** (18 records). **`tblItem` and `tblMagic` decoded** (37 items and 8 spells, names from `ITEM_NAME_<id>` — which is the format for EVERY table, not just items — prices, the id->category ranges out of `DataTableGetIdType`, and the effect amount at `+0x10` confirmed against `UseInventoryFunc`'s own read), `tblHelm`/`tblArmor`/`tblShield`/`tblLevelup` decoded | `src/mcf/assets.cpp`, `docs/assets.md`, `docs/weapon-table.md` |
| Engine reimplementation | NOT STARTED | `src/engine/` |

## Open questions (gate the next step)

- ~~Lua source or bytecode?~~ **ANSWERED: plain source**, 715 files / 50,643
  lines, Japanese developer comments intact. `sk1.lua` is the prelude (138
  helpers over the 200 native `cmd` functions). NOTE: `sk1.lua` is Shift-JIS,
  map scripts are UTF-8 — see `docs/assets.md`.
- ~~`SMDI` texture layout?~~ **ANSWERED** — see `docs/assets.md`.
- ~~`Smd3` model layout?~~ **ANSWERED** — see `docs/assets.md`.
- ~~`Smot` motion layout?~~ **ANSWERED** — see `docs/assets.md`.
- ~~`SCol` collision layout?~~ **ANSWERED at loader level** — see `docs/assets.md`.
- ~~`.scol` 40-byte node internals?~~ **ANSWERED** via `SiCollisionMesh::GetFloor`
  — they are collision triangles. See `docs/assets.md`.
- **OPEN:** 98/60,803 motion time arrays are non-monotonic (9 files). Meaning
  unknown; not per-channel sub-arrays. See `docs/assets.md`.
- ~~`.smdl` slot 5?~~ **ANSWERED: vertex declaration** — see `docs/assets.md`.
- ~~`.smdl` slots 0, 3, 4?~~ **ANSWERED: materials, mesh record, draw ranges** —
  see `docs/assets.md`. Multi-material models now render correctly.
- ~~`.smdl` section 1 bone record?~~ **ANSWERED — skinning works.** See
  `docs/assets.md`.
- Bone record fields +0x040, +0x0C0 and +0x100 have no established role.
- ~~`.stexinfo` / `.mtex` map texturing?~~ **ANSWERED** — maps render fully
  textured. See `docs/assets.md`.
- `room_field.mtex` (240x240, the only NPOT texture) has 58 bytes of mip padding
  the exact-sum rule does not predict.
- `.stexinfo` record +0x80 (a small u32) is unidentified.
- Dialogue text is resolved from the MPK's `str_en.bin` and `str_ja.bin`
  members under its `sk1` directory: 1906 ids, English and UTF-8 Japanese.
  `GetIDString` resolves them in the live script bridge —
  `./build/mana --string SYS_PARTYMSG_2_1`.
- ~~19 of 53 NPC ids have no model?~~ **ANSWERED: the mapping was wrong, and
  all 35 eNPC ids now resolve** (was 24/36). Two rules: ids 0..9 are the named
  party members and use `C<id>_00` id-for-id; ids >= 10 use `N<id-10>_00`. The
  offset is documented in sk1.lua's own enum comments (25/25 annotated entries
  agree, 0/25 match id-for-id), and the `C` rule was confirmed semantically by
  rendering eNPC 5 (CHOCOBO -> a chocobo) and 6 (CHOCOBOT -> the same bird in
  armour). Enemies and bosses were checked for the same bug and are id-for-id
  (73/73 and 23/23), so the fix is correctly scoped to NPCs.
- ~~`AddNPC`'s 6th parameter?~~ **ANSWERED: it is a placement extent, not a
  heading.** `AddNPC` @ `0x2c8a10` sets a flag when the script's x and z are
  both 0 and hands the float to `ModeGame::AddCharacterRandomPos`, which scans
  the room's per-chip attribute array. The port reproduces the TRIGGER; the
  chip array and RNG are not reversed, so the actual pick is a documented port
  choice. A "chip" is **30 units**, confirmed independently against the event
  box `EvBoxOneY("in_01",3,3.4,2)` -> `(90,102)..(120,132)`.
- **OPEN:** the engine converts room-local to world with a PER-ROOM size
  (`RoomLocalToWorldX` @ `0x2e3584`), not a fixed 300x240. The port uses the
  fixed size, which costs 31 of 116 script-placed actors their floor in
  multi-cell dungeon rooms. Using the collision AABB instead was tried and
  falsified — see `docs/assets.md`.
- NOTE: container magics are written but **never checked** by the engine, so
  magic-based dispatch is not an option — `Resource::LoadFromFile` switches on a
  `ResourceKind` enum derived from the caller, not the file.

## Building and running

    cmake -S . -B build -G Ninja && cmake --build build
    ./build/mana --model B0000_00 --screenshot out.png

Needs SDL3 and GLESv2; lucent is fetched by CMake. `--screenshot` renders one
frame and exits, using SDL's `offscreen` video driver when no display is present,
so the renderer is verifiable headlessly.

## Lua API priority

`docs/lua-census.md` measures which of the 200 `cmd` functions the shipping
scripts actually call. Implement in that order.

## Verified end to end

    ./build/mana --render-room M0000_03_06 --time 8 --screenshot out.png

Loads the room mesh and its shared textures, runs the room's Lua script against
a live actor system, resolves each spawned actor's model from its type id, places
it on the floor via a collision query, resolves its motion by numeric prefix, and
draws it GPU-skinned. Frames at t=0 and t=20 differ by 22,416 bytes, which is how
the animation is confirmed to be applied rather than silently falling back to
bind pose.

## Whole-game coverage

    ./build/mana --room-census

Loads **every** room the way the real path does — mesh, collision, `.odt`
objects, and the room script against a live actor system — headlessly, and
reports with denominators. Current state:

    993 rooms (3 non-room M*.smdl skipped): 0 mesh parse failures,
        2 without collision, 0 script failures
    285 rooms have no script (expected)
    165 actors spawned, 0 with no model, 8 intentionally invisible (eNPC.TRANS)
    539 event boxes
    423 rooms have an .odt; 3284 objects, 0 unresolved ids

Both remaining caveats are explained, not outstanding:

- **285 rooms with no script** are overworld tiles, which are entered by walking
  rather than by a scripted transition. Only 4 script-less rooms are an explicit
  `mapjump` destination, and 177 of the 285 are in the overworld map `M0000`.
- **2 rooms with no collision** (`M0006_00_03`, `M0012_01_02`) are orphans: no
  script, no `.odt`, no `.gdt`, no collision, and unreachable by any `mapjump`.

`M0001`, `M0002` and `M0020` carry no grid suffix and are not rooms at all;
counting them as rooms inflated the total and made them look like rooms missing
a collision mesh.

It parses rather than uploads models, so it is fast and headless but **cannot**
catch GPU upload or render faults — stated because a green census would
otherwise imply more than it checks. It exits non-zero on a mesh or script
failure or an unresolved object id, and refuses (exit 2) if the archive holds no
rooms at all.

## Verified playable, end to end

    ./build/mana --walk-to 105 117 --warmup 400 --screenshot out.png

Boots with no room argument, walks the player into the house's event box, runs
the `in_01` handler as a coroutine (`CHOCOBO_BYE` -> `bgmfield` -> `mapjump`),
starts BGM 4 and decodes 4,553,579 PCM frames of it, loads `M0010_02_02`, runs
that room's script, and ends with the player standing inside the furnished
interior. Every subsystem in the table above is exercised by that one command.

Known gaps it also shows honestly: the destination room has no `.odt`, and its
`SHOP` NPC (id 25) is one of the 19 of 53 NPC ids with no `N<id>_00` model.

## Collision probe

    ./build/mana --collision-probe M0000_03_06

Walks outward from the room centre in 8 directions and prints how far before a
wall or floor edge stops it. A wall system that blocks nothing is
indistinguishable from one that is never called, so this reports distances and
the reason, not a pass/fail.

## Audio

Sound effects are `sk1/SE%04d.wav` inside the MPK (ids **1..176, contiguous**).
Music is `assets/bgm%03d*.ogg` from the **APK, not the MPK**, in two banks:
**1..30 and 101..130** — the original and arranged soundtracks. Pass the
directory holding them with `--bgm-dir` (default `scratch/raw/assets`).

`./build/mana --audio-selftest` decodes a fixed set of SE and BGM from both banks
and fails non-zero if any produces no PCM. A decoder that plays nothing is
indistinguishable from one that is never called, so this runs in `verify.sh`.

## Running it

    ./build/mana --render-room M0000_03_06              # interactive
    ./build/mana --render-room M0000_03_06 --screenshot out.png   # one frame, headless

Arrow keys / WASD move the player, Esc quits. The player refuses to step off the
collision mesh and follows floor height; motion switches between WAIT (0) and
WALK (1). ~4000 frames in 3 s headless (uncapped, no vsync).

## Where the port stands

The **asset pipeline is complete**: every shipped format opens, and each parser
was accepted on evidence (a decoded image, rendered geometry, unit quaternions,
exact byte tilings) rather than on "it didn't crash".

The blocker is no longer *what is in the data*. It is now engine code. The next
dependency is `.smdl` section slots 0/3/4/5 — meshes, materials, draw ranges —
which nothing has needed yet but which per-submesh texture binding requires.
Those should be reversed when the renderer needs them, from the draw path
(`SiDrawServer`, `SiShaderBind`), not blind.
