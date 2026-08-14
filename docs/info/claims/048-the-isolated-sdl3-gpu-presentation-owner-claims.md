---
id: C048
kind: claim
status: holds
created: 2026-08-14
tags: renderer,presentation
depends: src/host/gpu_presentation.cpp#Presentation::Present, src/tools/gpu_selftest.cpp#main
reconfirmed: 2026-08-14
verified_at: 2026-08-14 14:51:46
---

## Claim

The isolated SDL3 GPU presentation owner claims and releases its window, renders through RGBA8 color and depth targets, and submits a format-converting blit to the selected swapchain while the mandatory renderer test remains windowless.

## Evidence

2026-08-14: SDL_VIDEODRIVER=offscreen ./build/mana_gpu_selftest passed its 0-window/0-audio device checks and all four presentation policy cases; the explicit --presentation-smoke then claimed a 64x48 swapchain and submitted one RGBA-to-swapchain frame successfully.

## What would falsify it

A backend rejects the RGBA-to-swapchain blit, resize target recreation fails, claim/release leaks a window, or the normal selftest constructs any SDL window.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact presentation source tree: structure gate scanned 83 files with 0 violations; normal SDL3 GPU test created 0 windows and initialized 0 audio while passing 4 presentation-policy cases; explicit offscreen --presentation-smoke claimed a 64x48 swapchain and submitted one RGBA-to-swapchain frame.
