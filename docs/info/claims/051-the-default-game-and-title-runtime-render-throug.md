---
id: C051
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3,tooling
depends: src/host/main.cpp#main, src/host/gpu_runtime_renderer.cpp#RuntimeRenderer, src/host/gpu_frame_renderer.cpp#FrameRenderer, tools/verify.sh, run.sh
---

## Claim

The default game and title runtime render through SDL3 GPU, while automated capture and no-render story paths create zero SDL windows and decode zero audio frames.

## Evidence

2026-08-14 full tools/verify.sh passed: 83 source files/0 structure violations, live SDL3 title and scene/UI/fade captures reported runtime driver vulkan, 0 SDL windows and 0 audio frames, continuous unchanged-Lua story reached sccnt=20, and ALL PARSERS PASSED; zero-argument ./run.sh built and entered GPU presentation.

## What would falsify it

A zero-argument run initializes the game through GLES or omits SDL3 presentation, any normal capture/story test reports a nonzero SDL window count or decoded audio frame, or live scene/title capture stops writing validated pixels.
