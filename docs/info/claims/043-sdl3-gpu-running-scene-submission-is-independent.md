---
id: C043
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3-gpu,presentation
depends: src/host/gpu_scene.cpp#SceneRenderer::Draw, src/host/gpu_snapshot_renderer.cpp#SnapshotRenderer::Draw, src/tools/gpu_asset_selftest.cpp
reconfirmed: 2026-08-14
verified_at: 2026-08-14 12:54:26
---

## Claim

SDL3 GPU running-scene submission is independent of readback target ownership and renders a shipping snapshot into a caller-owned command buffer and render pass byte-identically to the diagnostic wrapper

## Evidence

tools/verify.sh in scratch/logs/verify-external-gpu-pass-final.log: the M0001_00_00 plus C0000_00 snapshot external-pass output differs from SnapshotRenderer::DrawAndReadback in 0/76,800 pixels; the missing-command target is rejected; the same suite completes the live paired run, 21,961-frame story, zero audio frames, and ALL PARSERS PASSED.

## What would falsify it

An external SDL3 render target produces any pixel difference from the readback wrapper for the same snapshot, accepts a missing target, or changes scene-wide opaque/blended ordering

## Re-confirmed 2026-08-14

Reverified after registry creation by the complete tools/verify.sh run in scratch/logs/verify-external-gpu-pass-final.log: external target parity 0/76800, missing-target negative passed, live pair passed offscreen with zero audio, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 180133b by the complete tools/verify.sh gate in scratch/logs/verify-external-gpu-pass-final.log: external target parity 0/76800, missing-target negative passed, live pair stayed offscreen with 0 audio frames, both 21,961-frame story runs passed, and ALL PARSERS PASSED.
