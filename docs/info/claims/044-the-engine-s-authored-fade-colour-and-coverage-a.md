---
id: C044
kind: claim
status: holds
created: 2026-08-14
tags: renderer,sdl3-gpu,fade
depends: src/host/render_overlay.cpp#FadeOverlay::FromEngineColor, src/host/gpu_overlay.cpp#OverlayRenderer::Draw, src/host/scene_pair_capture.cpp#ScenePairCapture::WriteFromGles, src/host/main.cpp
reconfirmed: 2026-08-14
verified_at: 2026-08-14 14:42:04
---

## Claim

The engine's authored fade colour and coverage are consumed by a dedicated SDL3 GPU composition pass after the running scene, with blend RGB and destination alpha matching the shipping GLES rule

## Evidence

Complete tools/verify.sh in scratch/logs/verify-sdl3-fade-final.log: focused half-black overlay converts all 12 [204,102,51,255] clear pixels to [102,51,26,191] +/-1; zero coverage rejects all 12; live M0001 frame 30 captures the same 3-instance scene at fade coverage 0.500 offscreen with 0 audio frames and exits 0. Direct image measurement reports identical GLES/SDL3 mean alpha 0.678625 and alpha MAE 4.39e-7. ALL PARSERS PASSED.

## What would falsify it

A nonzero engine fade is omitted from SDL3 composition, RGB or destination alpha no longer follows SRC_ALPHA/ONE_MINUS_SRC_ALPHA, the paired run loses 0.500 coverage, or the pass creates a window/audio activity

## Re-confirmed 2026-08-14

Reverified after registry creation by complete tools/verify.sh in scratch/logs/verify-sdl3-fade-final.log: 12/12 positive pixels, 12/12 zero-coverage negative, live 0.500 fade offscreen with zero audio, 21/21 shader artifacts, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 3bee232 by complete tools/verify.sh in scratch/logs/verify-sdl3-fade-final.log: all focused positives and negatives passed, 21/21 portable shaders regenerated, the 0.500 scene/fade pair stayed offscreen with 0 audio frames, both 21,961-frame story runs passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact source tree committed as abd18ce: all focused positives/negatives, unpaced offscreen story through sccnt=20 with zero audio frames, room census, API/frontier/table checks, and ALL PARSERS PASSED

## Re-confirmed 2026-08-14

2026-08-14 full tools/verify.sh passed on the exact source tree committed as 088030a: structure gate 81 files/0 violations, 33/33 shader artifacts reproduced exactly, SDL3 GPU title omission controls measured sprite and text contributions, running ModeTitle completed offscreen with zero decoded audio frames, uncapped story verification reached sccnt=20, and ALL PARSERS PASSED.
