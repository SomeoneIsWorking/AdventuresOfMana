---
id: C049
kind: claim
status: falsified
created: 2026-08-14
tags: renderer,structure
depends: src/host/gles_asset.cpp#Asset::Release, src/host/main.cpp#main
reconfirmed: 2026-08-14
verified_at: 2026-08-14 15:04:26
falsified_on: 2026-08-14
---

## Claim

The transitional GLES asset uploader is isolated from backend-independent render policy and owns release of its textures and buffers before the running GL context is destroyed.

## Evidence

2026-08-14 full tools/verify.sh passed after moving uploads from host/render into move-only host/gles_asset: the same-frame GLES/SDL3 scene, UI, and fade pair remained live; the uncapped zero-audio story reached sccnt=20; structure scanned 85 files with 0 violations; ALL PARSERS PASSED.

## What would falsify it

A GLES handle reappears in host/render, an Asset copy becomes possible, a resource is released after its GL context, or the running scene/story verifier changes output or fails.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact GLES ownership source tree: 85 source files/0 structure violations, same-frame GLES/SDL3 scene UI fade pair, uncapped zero-audio story through sccnt=20, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact shared-frame-compositor tree: 87 source files/0 structure violations, 33/33 shaders, live offscreen GLES/SDL3 scene UI fade pair with zero audio, uncapped story through sccnt=20, and ALL PARSERS PASSED.

## FALSIFIED 2026-08-14

The transitional GLES asset uploader and its running cache were deleted after the GPU runtime cutover, so this ownership claim no longer describes existing code.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
