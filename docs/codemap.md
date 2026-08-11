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
| Assets: one `sk1.mpk`, custom format, magic `mcfa`, zlib, no AES | header + `LibMpk*` exports |

## Subsystems

| Subsystem | Status | Where |
|---|---|---|
| `cmd` Lua API extraction | **DONE — verified 200/200 names+impls** | `tools/asset/extract_cmd_api.py` -> `docs/cmd-api.md` |
| MPK archive reader | NOT STARTED | `tools/asset/` |
| Model / motion / texture formats | NOT STARTED | — |
| Engine reimplementation | NOT STARTED | `src/engine/` |
| Desktop host (window/GL/audio/input) | NOT STARTED | `src/host/` |

## Open questions (gate the next step)

- Are the Lua scripts in `sk1.mpk` stored as **source** or **precompiled 5.3 bytecode**?
  Both `luaL_loadstring` and `luaL_loadbufferx` are imported, so either is possible.
- What are the model (`SiModelBase`) and motion (`SiModelMotion`) container formats?
