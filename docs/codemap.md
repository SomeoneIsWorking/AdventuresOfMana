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
| Room data tables (`.odt`/`.gdt`/`.edt`) | **Layouts from the engine's own loaders; 1328/1328 files parse. Map objects RENDER in-game** (3284/3284 ids resolve to models); all 3284 object placements verified against room mesh bounds | `tools/asset/roomdata.py`, `tools/asset/object_table.py` |
| C++ asset layer | **DONE for MPK/.stex/.smdl** — port of the verified Python | `src/mcf/` |
| Desktop host (window + GLES2) | **Textured characters + rooms; GPU skinning + .smot playback** | `src/host/main.cpp` |
| Lua bridge | **Lua 5.3 + all 200 `cmd` bindings; 714/715 scripts run** (stubs record calls) | `src/engine/script.cpp` |
| Actor system | **Live actors driven by scripts**, rendered and animated in their rooms | `src/engine/world.{h,cpp}`, `src/host/render.cpp` |
| Game loop + player | **Boots into the game with no arguments**; real-time loop, WASD/arrow movement, floor + wall collision, WAIT/WALK, attack on Space/Z. Start room is a PORT CHOICE (`M0000_00_00`) — the save/new-game path is not reversed | `src/host/main.cpp` |
| Camera | **Script-driven follow camera** (`eCamGetData` slots) | `src/engine/world.h` |
| Fade | **Timed fades with the game's own shader**, verified to darken the frame | `src/host/main.cpp` |
| Combat volumes | **Attack arcs + damage spheres on bones; overlap predicate self-tested; fires in-game** (481 hits / 744 pairs on a real room; 0 hits at range, explained) | `src/engine/world.cpp`, `src/host/main.cpp` |
| Enemy behaviour | PARTIAL — real HP/defence/rewards from `enemydat.bin`; damage, death and drops work. Movement is a placeholder (closes on the player, no mutual separation). Player attack uses a real `tblWeapon` value but the game scales damage by a CHARGE METER (`[oG]+0x1b8`), so the port's flat-attack model is the wrong shape, not just a wrong number | `src/host/main.cpp` |
| Event boxes + transitions | **Edge-triggered boxes run handlers as coroutines; `mapjump` loads the destination room** | `src/engine/script.cpp`, `src/host/main.cpp` |
| Audio | **BGM + SE play, driven by scripts** | `src/engine/audio.cpp` |
| Game data tables | **Enemy table live in-game** (`enemydat.bin`, 107 records: max HP, defence, EXP, money each pinned to an engine read) and **`tblWeapon` extracted** (18 records). `tblItem`/`tblHelm`/`tblArmor`/`tblLevelup` located, not decoded | `src/mcf/assets.cpp`, `docs/assets.md`, `docs/weapon-table.md` |
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
- **Dialogue text is missing from the extracted data entirely** — see
  `docs/assets.md`. Likely in the OBB this repack omits, or hashed. Blocks all
  UI/dialogue work. **NEW LEAD, not yet followed:** `DataTableGetName` and
  `DataTableGetHelpString` index `sk1/enemydat.bin`, which IS present.
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
