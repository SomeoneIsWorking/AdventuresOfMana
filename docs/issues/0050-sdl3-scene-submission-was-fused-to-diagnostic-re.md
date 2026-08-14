---
id: 50
title: SDL3 scene submission was fused to diagnostic readback targets
status: resolved
symptom: The running SnapshotRenderer can render only through DrawAndReadback, so no caller can submit the same scene into a swapchain or post-process render pass
tags: renderer,sdl3,gpu,presentation,architecture,dusklight
created: 2026-08-14
updated: 2026-08-14
---

Root cause: SceneRenderer owns the Device::RenderAndReadback call and hides its opaque-before-blended submission loop inside that callback. SnapshotRenderer likewise builds transforms and palettes only inside DrawAndReadback. The scene policy therefore cannot be reused with an externally owned SDL_GPUCommandBuffer/SDL_GPURenderPass. Proper fix: expose validated scene submission and snapshot submission as target-independent operations, retain DrawAndReadback as a diagnostic wrapper, and prove both paths are byte-identical on shipping assets.

### Resolution (2026-08-14)
SceneRenderer::Draw now validates and submits into a caller-owned SDL_GPUCommandBuffer/SDL_GPURenderPass; DrawAndReadback is only a wrapper around that operation. SnapshotRenderer::Draw likewise builds cached assets, transforms, and palettes for any compatible external pass. The shipping self-test renders the same snapshot through both ownership paths with 0/76,800 differing pixels and rejects a null command target with an exact diagnostic.
