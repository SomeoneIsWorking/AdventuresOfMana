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
| shader pack | compile HLSL to tracked SPIR-V, DXIL, and MSL, embed all formats, and select the active backend without runtime source guessing | **live and tested** |
| backend-independent assets | parse models, retain texture storage, resolve material textures, and compute bounds without a graphics API | **live and shared by GLES and SDL3 GPU** |
| GPU assets | texture, vertex-buffer, index-buffer, sampler, and lifetime ownership | **live and tested for static assets** |
| scene renderer | room, static actor, skinned actor, depth, blend, and draw submission | static textured draw is live offscreen; depth, blend, skinning, and shipping-scene integration remain missing |
| UI renderer | font atlas, sprites, message windows, HUD, and fade | missing |
| presentation | window/swapchain ownership, resize, present mode, and interactive pacing | missing |

The device and shader owners deliberately have no dependency on game assets or
`main.cpp`. `RenderAsset` is the shared CPU boundary: it retains the storage
behind texture spans and resolves draw-to-texture references once. GLES and
SDL3 GPU upload from that same result instead of maintaining two parsers.
`mana_gpu_selftest` initializes SDL's offscreen video driver, creates no window,
opens no audio subsystem, clears an RGBA8 GPU target to black and magenta, and
reads every pixel back. The two-color discriminator prevents an all-zero or
stale transfer buffer from looking like a successful capture; the mandatory
wrong-color negative proves the same shipping readback rejects all 12 pixels.
Generated MSL is normalized mechanically so compiler whitespace cannot dirty a
clean tree or make byte identity host-dependent. The embedded solid pipeline
then draws a full-target triangle and proves all 48
pixels changed from the black clear; its own wrong-color negative rejects all
48. The textured asset pipeline uploads the shipping `M0001_00_00` room's
vertex/index buffers and seven textures, submits all 12 draw ranges, and scans
6,912 readback pixels. The positive changes 3,868 pixels with 202 distinct red
values, and all 3,868 differ from a forced-white texture render; the paired
no-draw class changes 0. The verifier regenerates all 12
backend artifacts from four HLSL sources and byte-compares them with the
tracked pack.

SDL's official Shadercross build is a regeneration tool, not a runtime
dependency. On Linux, `tools/bootstrap_shadercross_linux.sh` downloads and
hash-verifies the pinned official Actions artifact in gitignored `scratch/`.
`tools/compile_shaders.sh` also accepts `SHADERCROSS` and `SHADERCROSS_LIB` for
other official builds.

## Migration order

1. ~~Move immutable texture and geometry upload into the GPU-assets owner while
   retaining CPU `Model` and `TextureSet` data independently of the backend.~~
2. Port the tested textured static-room pass into the shipping scene and compare offscreen captures against
   the existing GLES path from the same camera and frame.
3. Port skinning, actors, UI, fade, and capture as separate passes. Delete each
   GLES owner when its SDL3 GPU replacement passes its discriminator; do not
   maintain two permanent renderers.
4. Add presentation last so all automated work stays texture-backed,
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
