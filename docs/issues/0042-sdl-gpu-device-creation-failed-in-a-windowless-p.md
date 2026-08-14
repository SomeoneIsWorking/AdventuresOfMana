---
id: 42
title: SDL GPU device creation failed in a windowless process
status: resolved
symptom: SDL_CreateGPUDevice failed with 'Video subsystem not initialized' even though the test needed no window or swapchain
tags: tooling,sdl3,gpu,headless,windowless
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

SDL's GPU implementation requires `SDL_INIT_VIDEO` before device creation even
when the command stream only targets an offscreen texture. The first device
selftest incorrectly equated "windowless" with "video subsystem uninitialized."

## What was tried / dead ends

Calling `SDL_CreateGPUDevice` directly in a fresh process deterministically
failed before any texture or command-buffer operation, so Vulkan availability,
render-target format, and readback synchronization were not yet relevant.

## Resolution

`mana::gpu::Device` initializes the video subsystem when it does not already
have an owner, creates the GPU device, and restores the previous subsystem
state at shutdown. The verifier forces SDL's offscreen video driver and proves
there are zero SDL windows, audio remains uninitialized, and black and magenta
clears produce distinct exact readbacks across all 24 tested pixels.
