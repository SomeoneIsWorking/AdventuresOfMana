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

## Subsystems

| Subsystem | Status | Where |
|---|---|---|
| `cmd` Lua API extraction | **DONE — verified 200/200 names+impls** | `tools/asset/extract_cmd_api.py` -> `docs/cmd-api.md` |
| MPK archive reader | **DONE — 9886/9886 extracted, validated** | `tools/asset/mpk.py`, `tools/asset/lha.py` |
| Texture format (`.stex`/`SMDI`) | **DONE — 1319/1319 descriptors parse, image verified** | `tools/asset/stex.py` |
| Model/motion/collision formats | NOT STARTED — parsers located, layouts not reversed | `SiModelBase::SetBinary`, `SiModelMotion::SetBinary`, `SiCollisionMesh::SetBinary` |
| Engine reimplementation | NOT STARTED | `src/engine/` |
| Desktop host (window/GL/audio/input) | NOT STARTED | `src/host/` |

## Open questions (gate the next step)

- ~~Lua source or bytecode?~~ **ANSWERED: plain source**, 715 files / 50,643
  lines, Japanese developer comments intact. `sk1.lua` is the prelude (138
  helpers over the 200 native `cmd` functions). NOTE: `sk1.lua` is Shift-JIS,
  map scripts are UTF-8 — see `docs/assets.md`.
- ~~`SMDI` texture layout?~~ **ANSWERED** — see `docs/assets.md`.
- `Smd3` (model), `Smot` (motion), `SCol` (collision) layouts. Parsers are
  `SiModelBase::SetBinary`, `SiModelMotion::SetBinary`,
  `SiCollisionMesh::SetBinary`; same method as `.stex` applies.
- NOTE: container magics are written but **never checked** by the engine, so
  magic-based dispatch is not an option — `Resource::LoadFromFile` switches on a
  `ResourceKind` enum derived from the caller, not the file.
