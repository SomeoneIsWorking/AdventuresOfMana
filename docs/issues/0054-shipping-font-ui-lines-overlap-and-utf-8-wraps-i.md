---
id: 54
title: Shipping-font UI lines overlap and UTF-8 wraps incorrectly
status: resolved
symptom: multi-line HUD/dialogue glyphs overlap vertically and non-ASCII dialogue wraps by bytes
tags: renderer,text,ui,utf8,sdl3,gles,tooling
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

The three inline GLES UI paths hardcoded a 22-pixel line pitch while the
shipping font renders 34-pixel scaled cells. The message path additionally
measured and wrapped UTF-8 one byte at a time, so multibyte codepoints could be
split or assigned the wrong width.

## What was tried / dead ends

The focused SDL3 capture initially made the overlap visible. Changing the test
string could remove one symptom but could not fix the shared HUD/dialogue
layout, so the layout owner itself had to use loaded-font metrics and decoded
codepoints.

## Resolution

`host/render_ui` now derives pitch from `Font::line_height`, wraps
`Utf8Codepoints`, and emits one command stream consumed by GLES and SDL3 GPU.
The focused English multi-line capture and real Japanese `SYS_GAMEOVER_MSG`
both report zero missing glyphs; the live offscreen pair reports 2 UI batches
and 58 glyph quads. Solid-atlas and invalid-batch negatives fail as required.
