---
id: 51
title: Running SDL3 GPU captures omitted the authored fade pass
status: resolved
symptom: --scene-pair captured SDL3 scene geometry before UI/fade, so FadeOut/FadeIn state could only appear in the GLES image and SDL3 had no composition owner
tags: renderer,sdl3,gpu,fade,composition,architecture,dusklight
created: 2026-08-14
updated: 2026-08-14
---

Root cause: fade drawing is an inline GLES full-screen triangle in main.cpp and RenderSnapshot intentionally contains only 3D scene instances. ScenePairCapture stops at SnapshotRenderer, so it has no backend-independent fade input or SDL3 composition pass. Proper fix: define a narrow fade-overlay frame, implement a dedicated SDL3 GPU overlay renderer with portable shaders, capture GLES after its fade, and compose the identical authored fade after the SDL3 scene in one target. Do not fold text/HUD/presentation into this owner.

### Resolution (2026-08-14)
FadeOverlay now freezes the engine's authored RGB/coverage independently of either backend. gpu_overlay owns a portable SDL3 full-frame blended pass; ScenePairCapture submits it after the same running snapshot and captures GLES after its fade. The first live discriminator found a second root cause: the SDL pass used ONE for source alpha while GLES glBlendFunc applies SRC_ALPHA to alpha too, producing 1.0 instead of 0.75 destination alpha at half fade. Matching SRC_ALPHA makes live mean alpha identical (0.678625 each; alpha MAE 4.39e-7). The focused half-black pass changes all 12 pixels to [102,51,26,191] +/-1; zero coverage rejects all 12. The paired fade gate explicitly disables the still-unported HUD so it proves scene+fade only.
