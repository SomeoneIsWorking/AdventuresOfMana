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
| camera + render snapshot | resolve script camera targets/interpolation once and freeze room/object/actor inputs for one frame | **live and consumed by GLES; SDL3 GPU consumption remains missing** |
| GPU assets | texture, vertex-buffer, index-buffer, sampler, and lifetime ownership | **live and tested for static and skinned assets** |
| scene renderer | room, static actor, skinned actor, depth, blend, and draw submission | **live offscreen for a composed shipping room and skinned actor**; integration with the running world's scene snapshot remains missing |
| UI renderer | font atlas, sprites, message windows, HUD, and fade | missing |
| presentation | window/swapchain ownership, resize, present mode, and interactive pacing | missing |

The device and shader owners deliberately have no dependency on game assets or
`main.cpp`. `RenderAsset` is the shared CPU boundary: it retains the storage
behind texture spans and resolves draw-to-texture references once. GLES and
SDL3 GPU upload from that same result instead of maintaining two parsers.
`CameraTracker` similarly converts the mutable script camera into one
backend-independent `CameraFrame`, including room-local actor targets,
fixed targets, explicit eyes, and 30 Hz-scaled interpolation. `RenderSnapshot`
then freezes that frame plus the resolved room, visible map objects, and live
actor instances. The GLES path consumes the snapshot now; SDL3 GPU no longer
needs to reach back into `World` or duplicate script-camera policy when the
running scene is connected.
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
6,912 readback pixels. The positive changes 3,868 pixels with 194 distinct red
values, and all 3,868 differ from a forced-white texture render; the paired
no-draw class changes 0. A layered near/far draw changes 0 pixels with depth
enabled and 3,868 with depth disabled. A second shipping room contributes two
water draw ranges, and 128 pixels differ from the opaque-material control. The
shipping hero `C0000_00` exercises the same two-bone incidence/weight formula
as the GLES path with the shared 80-bone pose palette. Its bind render changes
2,812 pixels from clear; translating every joint changes 3,723 pixels, while a
missing palette fails explicitly instead of drawing stale state. Pose
evaluation lives in `host/render_pose`, not either backend. The verifier
regenerates all 15 backend artifacts from five HLSL sources and byte-compares
them with the tracked pack.

`host/gpu_scene` is the scene-wide submission owner. It accepts explicit camera
and per-object transforms, submits every asset's opaque material ranges before
any asset's blended ranges, and renders them through one depth target. A paired
no-depth control differs in 748 pixels from the old per-asset material order,
so that policy is executed rather than inferred from call sites. Its
shipping discriminator uses the host's default perspective-camera convention
to compose room `M0001_00_00` and hero `C0000_00` at room center. The actor
changes 88 of 76,800 pixels from the room-only image; translating the same
actor beyond the frustum changes 0. The mandatory test writes that composite
through `host/image_write` to `scratch/screenshots/sdl3-gpu-scene.png`. PNG
output is no longer hidden in `main.cpp`, and an unavailable output directory
fails explicitly.

SDL's official Shadercross build is a regeneration tool, not a runtime
dependency. On Linux, `tools/bootstrap_shadercross_linux.sh` downloads and
hash-verifies the pinned official Actions artifact in gitignored `scratch/`.
`tools/compile_shaders.sh` also accepts `SHADERCROSS` and `SHADERCROSS_LIB` for
other official builds.

## Migration order

1. ~~Move immutable texture and geometry upload into the GPU-assets owner while
   retaining CPU `Model` and `TextureSet` data independently of the backend.~~
2. Feed the running `RenderSnapshot` into the tested SDL3 GPU scene owner, then
   compare its offscreen capture against GLES at the same frame.
3. Route the resulting scene into the game loop, then port UI, fade, and
   capture as separate passes. Delete each
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
