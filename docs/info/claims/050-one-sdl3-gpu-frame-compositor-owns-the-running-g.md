---
id: C050
kind: claim
status: holds
created: 2026-08-14
tags: renderer,presentation
depends: src/host/gpu_frame_renderer.cpp#FrameRenderer::Draw, src/host/scene_pair_capture.cpp#ScenePairCapture::WriteFromGles
reconfirmed: 2026-08-14
verified_at: 2026-08-14 15:04:33
---

## Claim

One SDL3 GPU frame compositor owns the running game pass order of RenderSnapshot, UiFrame, then FadeOverlay for both deterministic readback and caller-owned presentation passes.

## Evidence

2026-08-14 full tools/verify.sh passed after ScenePairCapture delegated composition to gpu::FrameRenderer; the live frame still reported 3 instances (2 skinned), 3 cached assets, 2 UI batches/58 glyph quads, fade coverage 0.500, offscreen video, and zero decoded audio frames.

## What would falsify it

Capture or presentation wires scene/UI/fade independently, the live pair ordering or counts change unexpectedly, or omission controls stop distinguishing any one pass.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact frame-compositor source tree: live offscreen pair retained 3 instances (2 skinned), 3 cached assets, 2 UI batches/58 glyph quads and fade 0.500 with zero audio; all gates passed.
