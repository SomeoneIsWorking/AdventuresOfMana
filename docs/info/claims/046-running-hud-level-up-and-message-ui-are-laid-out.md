---
id: C046
kind: claim
status: falsified
created: 2026-08-14
tags: renderer,ui,sdl3,gles
depends: src/host/render_ui.cpp#BuildGameUi, src/host/scene_pair_capture.cpp#WriteFromGles, src/host/main.cpp
reconfirmed: 2026-08-14
verified_at: 2026-08-14 15:04:26
falsified_on: 2026-08-14
---

## Claim

Running HUD, level-up, and message UI are laid out once from shipping strings and font metrics, then consumed by GLES and SDL3 GPU in the same scene/UI/fade order

## Evidence

2026-08-14 tools/verify.sh: focused UI rendered English multiline and Japanese SYS_GAMEOVER_MSG with zero missing glyphs; solid-atlas and invalid-batch negatives fired; live offscreen frame pair reported 2 UI batches/58 glyph quads and fade coverage 0.500 with zero audio frames; full suite ended ALL PARSERS PASSED

## What would falsify it

if either backend stops consuming BuildGameUi output, the live pair omits UI, or the focused shipping-font discriminators fail

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact source tree committed as 088030a: structure gate 81 files/0 violations, 33/33 shader artifacts reproduced exactly, SDL3 GPU title omission controls measured sprite and text contributions, running ModeTitle completed offscreen with zero decoded audio frames, uncapped story verification reached sccnt=20, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact shared-frame-compositor tree: 87 source files/0 structure violations, 33/33 shaders, live offscreen GLES/SDL3 scene UI fade pair with zero audio, uncapped story through sccnt=20, and ALL PARSERS PASSED.

## FALSIFIED 2026-08-14

The shared layout remains authoritative, but the GLES game UI consumer was deleted; SDL3 GPU is now its sole shipping renderer.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
