# Renderer architecture

## Reference and target

Dusklight is the ownership and project-structure reference. Its useful boundary
is not its backend API: Dusklight separates presentation
(`src/dusk/presentation.cpp`), window services, graphics services, settings,
and UI, while Aurora supplies its WebGPU renderer. Adventures of Mana targets
SDL3 GPU directly, so copying Aurora or Dusklight's backend would be the wrong
implementation even though their module boundaries are the lead.

The shipping game path is still GLES2. Do not describe the renderer migration
as complete until the room, actor, UI, fade, capture, and presentation paths no
longer issue GL calls.

## Owners

| Owner | Responsibility | Status |
|---|---|---|
| `host/gpu_device` | SDL video-subsystem lifetime, GPU device, offscreen targets, command submission, synchronized readback | **live and tested** |
| shader pack | compile and select SPIR-V, DXIL, and Metal shader artifacts without runtime source guessing | missing |
| GPU assets | texture, vertex-buffer, index-buffer, sampler, and lifetime ownership | missing; GLES resources remain in `host/render` |
| scene renderer | room, static actor, skinned actor, depth, blend, and draw submission | missing; GLES calls remain in `host/main.cpp` |
| UI renderer | font atlas, sprites, message windows, HUD, and fade | missing |
| presentation | window/swapchain ownership, resize, present mode, and interactive pacing | missing |

The first owner deliberately has no dependency on game assets or `main.cpp`.
`mana_gpu_selftest` initializes SDL's offscreen video driver, creates no window,
opens no audio subsystem, clears an RGBA8 GPU target to black and magenta, and
reads every pixel back. The two-color discriminator prevents an all-zero or
stale transfer buffer from looking like a successful capture; the mandatory
wrong-color negative proves the same shipping readback rejects all 12 pixels.

## Migration order

1. Integrate a portable shader build: one authored source must produce the
   formats SDL3 GPU needs on Vulkan, D3D12, and Metal. A Vulkan-only production
   path is not an acceptable intermediate architecture.
2. Move immutable texture and geometry upload into the GPU-assets owner while
   retaining CPU `Model` and `TextureSet` data independently of the backend.
3. Port the textured static-room pass and compare offscreen captures against
   the existing GLES path from the same camera and frame.
4. Port skinning, actors, UI, fade, and capture as separate passes. Delete each
   GLES owner when its SDL3 GPU replacement passes its discriminator; do not
   maintain two permanent renderers.
5. Add presentation last so all automated work stays texture-backed,
   windowless, unpaced, and silent.

## Lighting and image quality

The washed-out image should be corrected in the renderer, not by editing the
shipping Lua scripts or textures. First preserve a capture-compatible authored
baseline. Then measure which model attributes and material fields carry normals
and lighting inputs, add a linear-light scene target, and introduce lighting and
tone mapping as explicit passes with A/B captures. Exposure, contrast, ambient
level, and light direction must be named configuration with measured defaults;
they must not become scattered shader constants chosen to make one room look
good.
