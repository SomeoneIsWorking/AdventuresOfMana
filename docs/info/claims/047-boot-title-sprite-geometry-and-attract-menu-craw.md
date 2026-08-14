---
id: C047
kind: claim
status: holds
created: 2026-08-14
tags: renderer,title,sdl3,gles
depends: src/host/title_ui.cpp#BuildTitleUi, src/host/render_sprite.cpp#BuildAspectFitSprite, src/host/main.cpp
reconfirmed: 2026-08-14
verified_at: 2026-08-14 15:04:26
---

## Claim

Boot/title sprite geometry and attract/menu/crawl/name text layout are backend-independent and consumed by both the running GLES path and SDL3 GPU

## Evidence

2026-08-14 tools/verify.sh: 33/33 shaders regenerated; SDL3 shipping title omission controls observed 34,625 sprite and 10,253 text pixels; running offscreen ModeTitle capture wrote bytes with zero audio frames; full suite ended ALL PARSERS PASSED

## What would falsify it

if main stops calling BuildTitleUi, SDL3 stops consuming BuildAspectFitSprite/BuildTitleUi output, either omission discriminator becomes zero, or the running ModeTitle capture fails

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact shared-frame-compositor tree: 87 source files/0 structure violations, 33/33 shaders, live offscreen GLES/SDL3 scene UI fade pair with zero audio, uncapped story through sccnt=20, and ALL PARSERS PASSED.
