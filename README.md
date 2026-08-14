# Adventures of Mana — PC port

A from-scratch PC port of *Adventures of Mana* (Square Enix, 2016 — the 3D remake
of *Final Fantasy Adventure* / *聖剣伝説*), reimplemented in C++20 against the
shipping Android build.

**No emulation and no ARM execution anywhere.** The Android `libmcfandroid.so` is
read as a specification, never run: every format, table and formula in `src/` was
reversed from that binary's own loaders and is cited by function address in
`docs/assets.md`. The game's Lua scripts run unmodified on a real Lua 5.3 with
all 200 of the engine's `cmd` bindings reimplemented natively.

The game and title renderer use SDL3 GPU with portable SPIR-V, DXIL, and MSL
shaders. GLES2 remains only in the explicit single-model inspection tool.

## What works

Walk around the world in real time, fight, talk to people, read the game's text.
More precisely:

- **All 993 rooms load** — mesh, textures, collision, map objects, scripts — with
  zero parse failures and zero unresolved object ids.
- **Movement and collision** — floor queries and wall sliding off the room's own
  `.scol` mesh.
- **Combat, both ways** — attack arcs and damage spheres on real bones; enemies
  use their `enemydat.bin` HP, defence, attack, EXP and money; the player's
  defence comes from the real equipment tables.
- **Dialogue** — the game's own text, in English or Japanese, drawn in the game's
  own bitmap font with word wrap, waiting for the player line by line, with the
  `@` control codes expanded (`@N(36):` → `Prisoner:`).
- **Animation** — GPU skinning driven by the shipping `.smot` motion data.
- **Audio** — BGM and sound effects, driven by the scripts.
- **Room transitions** — event boxes run their handlers, `mapjump` loads the
  destination.

`docs/codemap.md` carries an honest per-subsystem status, and
`docs/re-frontier.md` splits everything into what was read out of the shipping
binary, what the port had to choose for itself, and what is simply missing —
along with every claim this project has had to retract.
`docs/open-questions.md` is the shorter working list: the reversing questions
that are currently blocking something, and the findings that are recorded but
have not yet survived a second, adversarial reading. Nothing here is marked
done unless it was verified on real data.

## What is NOT here

**No game assets, and there never will be.** You need your own copy. The port
reads the APK's `assets/` tree; point it at yours with either

```sh
export MANA_ASSETS=/path/to/extracted/apk/assets
```

or by dropping the tree at `scratch/raw/assets/` in the checkout. Both work, so a
one-off run needs no setup. Without it every tool refuses to run rather than
reporting a vacuous pass.

Some reversing tools additionally want the game's `libmcfandroid.so` at
`scratch/raw/libmcfandroid.so`; they skip themselves, loudly, when it is absent.

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./run.sh                     # builds stale inputs and boots the game
./build/mana --help          # every flag
```

Needs a C++20 compiler, SDL3, and libvorbisfile. Lua and
[lucent](https://github.com/SomeoneIsWorking/lucent) are fetched by CMake.

## Verify

```sh
./tools/verify.sh
```

Runs every asset parser over the whole 9886-file corpus, plus the combat, audio
and text self-tests and a headless census that loads all 993 rooms. It **refuses
to run** if the corpus is missing rather than printing "0 failures" over 0 files.

Each self-test is checked against both classes — cases that must pass and cases
that must fail — because a check that cannot say "no" is not a check.

## Layout

| path | what |
|---|---|
| `src/mcf/` | asset layer: MPK archive, `.stex`, `.smdl`, `.smot`, `.scol`, room tables, strings |
| `src/engine/` | world, actors, Lua bridge and the 200 `cmd` bindings |
| `src/host/` | SDL3 GPU presentation/render owners, host orchestration, UI and diagnostics |
| `tools/asset/` | Python reference implementations, the cross-check for `src/mcf/` |
| `docs/` | reversed formats, the `cmd` API, the codemap, the RE frontier |

## Legal

This repository contains only original code and notes about file formats. It
ships no code, art, audio, text or data from the game, and it will not run
without a copy you own. Interoperability reverse engineering; not affiliated with
or endorsed by Square Enix.
