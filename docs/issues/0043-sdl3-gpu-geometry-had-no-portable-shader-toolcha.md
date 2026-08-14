---
id: 43
title: SDL3 GPU geometry had no portable shader toolchain
status: resolved
symptom: The device layer could clear textures, but any first draw pipeline would have required an untracked developer compiler or a Vulkan-only SPIR-V shortcut
tags: tooling,sdl3,gpu,shaders,portability,structure
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

SDL3 GPU deliberately consumes backend-specific compiled shaders. The project
had no owned shader sources, no reproducible cross-compilation path, no runtime
format selection, and no rule preventing a local SPIR-V-only experiment from
becoming the production architecture.

## What was tried / dead ends

Building SDL_shadercross from source would vendor its large SPIRV-Cross and DXC
dependency stack; SDL's own guidance recommends using their official prebuilt
artifacts instead. Requiring Shadercross at game runtime would also turn a
developer tool into a shipping dependency.

## Resolution

Two tracked HLSL sources generate six tracked SPIR-V, DXIL, and MSL artifacts
through a pinned, hash-verified official Shadercross artifact in gitignored
`scratch/`. CMake embeds the complete pack, and the runtime selects the format
supported by the active SDL GPU backend. The verifier regenerates and
byte-compares all six artifacts, rejects a missing pack, draws a full-target
triangle across 48 pixels, and proves its wrong-color negative rejects 48/48.
