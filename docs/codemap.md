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
| Model format (`.smdl`/`Smd3`) | **DONE — 1375/1375 parse, geometry rendered** | `tools/asset/smdl.py`, `render_smdl.py` |
| Motion format (`.smot`/`Smot`) | **DONE — 1721/1721 parse; quaternions verified. 98 of 60,803 time arrays unexplained** | `tools/asset/smot.py` |
| Collision format (`.scol`/`SCol`) | **DONE at loader level — 992/992 parse. 40-byte node internals not reversed** | `tools/asset/scol.py` |
| Engine reimplementation | NOT STARTED | `src/engine/` |
| Desktop host (window/GL/audio/input) | NOT STARTED | `src/host/` |

## Open questions (gate the next step)

- ~~Lua source or bytecode?~~ **ANSWERED: plain source**, 715 files / 50,643
  lines, Japanese developer comments intact. `sk1.lua` is the prelude (138
  helpers over the 200 native `cmd` functions). NOTE: `sk1.lua` is Shift-JIS,
  map scripts are UTF-8 — see `docs/assets.md`.
- ~~`SMDI` texture layout?~~ **ANSWERED** — see `docs/assets.md`.
- ~~`Smd3` model layout?~~ **ANSWERED** — see `docs/assets.md`.
- ~~`Smot` motion layout?~~ **ANSWERED** — see `docs/assets.md`.
- ~~`SCol` collision layout?~~ **ANSWERED at loader level** — see `docs/assets.md`.
- The `.scol` 40-byte node record internals. `SetBinary` never reads them, so the
  loader constrains nothing; needs the `CollisionBase` query methods instead.
- **OPEN:** 98/60,803 motion time arrays are non-monotonic (9 files). Meaning
  unknown; not per-channel sub-arrays. See `docs/assets.md`.
- ~~`.smdl` slot 5?~~ **ANSWERED: vertex declaration** — see `docs/assets.md`.
- `.smdl` slots 0, 3, 4 (meshes / materials / draw ranges) remain unidentified;
  per-submesh texture binding needs them.
- NOTE: container magics are written but **never checked** by the engine, so
  magic-based dispatch is not an option — `Resource::LoadFromFile` switches on a
  `ResourceKind` enum derived from the caller, not the file.

## Where the port stands

The **asset pipeline is complete**: every shipped format opens, and each parser
was accepted on evidence (a decoded image, rendered geometry, unit quaternions,
exact byte tilings) rather than on "it didn't crash".

The blocker is no longer *what is in the data*. It is now engine code. The next
dependency is `.smdl` section slots 0/3/4/5 — meshes, materials, draw ranges —
which nothing has needed yet but which per-submesh texture binding requires.
Those should be reversed when the renderer needs them, from the draw path
(`SiDrawServer`, `SiShaderBind`), not blind.
