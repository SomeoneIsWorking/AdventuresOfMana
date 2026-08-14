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
| backend-independent assets | parse models, retain texture storage, resolve material textures, compute bounds, and derive area-weighted smooth normals from indexed geometry without a graphics API | **live and shared by GLES and SDL3 GPU** |
| camera + render snapshot | resolve script camera targets/interpolation once and freeze room/object/actor inputs for one frame | **live and consumed by GLES and SDL3 GPU** |
| GPU assets | texture, generated-normal/vertex-buffer, index-buffer, sampler, and lifetime ownership | **live and tested for static and skinned assets** |
| scene renderer | room, static actor, skinned actor, depth, blend, and draw submission | **live offscreen for a composed shipping room and skinned actor, including running-world snapshots** |
| UI content + layout | format shipping strings/live player values once, decode UTF-8, wrap messages, and emit backend-independent panel/glyph batches | **live and shared by GLES and SDL3 GPU for the running HUD, level-up screen, and messages** |
| GPU UI renderer | shipping R8 font atlas upload, portable blended pipeline, and shared UI-batch submission | **live offscreen in SDL3 GPU and the running paired capture** |
| boot/title renderer | shipping logo PNG decode, aspect-fit sprite geometry, and attract/menu/crawl/name text layout | **backend-independent and rendered by both GLES and SDL3 GPU offscreen; interactive presentation still uses GLES** |
| fade overlay | freeze authored fade colour/coverage and composite it after scene/UI | **live offscreen in SDL3 GPU and the running paired capture** |
| presentation | shipping-window claim/release, swapchain ownership, resize-safe RGBA/depth targets, format-converting blit, and immediate/vsync selection | **live and smoke-tested as an isolated SDL3 GPU owner; runtime wiring remains blocked on extracting GLES resources from the world asset cache** |

The device and shader owners deliberately have no dependency on game assets or
`main.cpp`. `RenderAsset` is the shared CPU boundary: it retains the storage
behind texture spans and resolves draw-to-texture references once. GLES and
SDL3 GPU upload from that same result instead of maintaining two parsers.
`CameraTracker` similarly converts the mutable script camera into one
backend-independent `CameraFrame`, including room-local actor targets,
fixed targets, explicit eyes, and 30 Hz-scaled interpolation. `RenderSnapshot`
then freezes that frame plus the resolved room, visible map objects, and live
actor instances. Both renderers consume that same snapshot; SDL3 GPU does not
reach back into `World` or duplicate script-camera policy.
`mana_gpu_selftest` initializes SDL's offscreen video driver, creates no window,
opens no audio subsystem, clears an RGBA8 GPU target to black and magenta, and
reads every pixel back. `host/render_normals` derives finite unit normals for
both 16- and 32-bit index streams and leaves unsupported vertices exactly zero
so the shader applies ambient light rather than inventing an orientation. Its
focused positive checks +Z output for all three vertices; its degenerate
negative reports one degenerate triangle and three exact-zero normals. The
two-color discriminator prevents an all-zero or
stale transfer buffer from looking like a successful capture; the mandatory
wrong-color negative proves the same shipping readback rejects all 12 pixels.
The same mandatory executable tests presentation policy without constructing a
window: unpaced mode selects IMMEDIATE only when the claimed window reports it
supported, with VSYNC as the supported fallback. The explicit, non-mandatory
`--presentation-smoke` path claims an SDL3 GPU window, renders through a stable
RGBA8 color/depth pair, blits into the backend-selected swapchain format, and
submits one frame. Keeping that smoke path out of `tools/verify.sh` is
intentional: the normal automated renderer remains genuinely windowless rather
than redefining an offscreen window as windowless.
Generated MSL is normalized mechanically so compiler whitespace cannot dirty a
clean tree or make byte identity host-dependent. The embedded solid pipeline
then draws a full-target triangle and proves all 48
pixels changed from the black clear; its own wrong-color negative rejects all
48. The textured asset pipeline uploads the shipping `M0001_00_00` room's
vertex/index buffers and seven textures, submits all 12 draw ranges, and scans
6,912 readback pixels. The positive changes 3,868 pixels with 188 distinct red
values, and all 3,868 differ from a forced-white texture render; the paired
no-draw class changes 0. A layered near/far draw changes 0 pixels with depth
enabled and 3,868 with depth disabled. A second shipping room contributes two
water draw ranges, and 128 pixels differ from the opaque-material control. The
shipping hero `C0000_00` exercises the same two-bone incidence/weight formula
as the GLES path with the shared 80-bone pose palette. Its bind render changes
2,812 pixels from clear; translating every joint changes 3,723 pixels, while a
missing palette fails explicitly instead of drawing stale state. The room
derives normals from 1,094 triangles with zero degenerates and zero unsupported
vertices. Enhanced lighting differs from vanilla in 3,868 pixels and from an
equal-ambient control in 3,675, proving the result contains directional surface
response rather than global dimming. The skinned path differs from its
equal-ambient control in 2,750 pixels. Pose
evaluation lives in `host/render_pose`, not either backend. The verifier
regenerates all 33 backend artifacts from eleven HLSL sources and byte-compares
them with the tracked pack.

`host/gpu_scene` is the scene-wide submission owner. It accepts explicit camera
and per-object transforms, submits every asset's opaque material ranges before
any asset's blended ranges, and renders them through one depth target. A paired
no-depth control differs in 748 pixels from the old per-asset material order,
so that policy is executed rather than inferred from call sites. Its
shipping discriminator uses the host's default perspective-camera convention
to compose room `M0001_00_00` and hero `C0000_00` at room center. The actor
changes 88 of 76,800 pixels from the room-only image; translating the same
actor beyond the frustum changes 0. The mandatory test writes enhanced and
vanilla composites through `host/image_write` to
`scratch/screenshots/sdl3-gpu-scene.png` and
`scratch/screenshots/sdl3-gpu-scene-vanilla.png`, and refuses byte-identical
captures. PNG
output is no longer hidden in `main.cpp`, and an unavailable output directory
fails explicitly.

`host/gpu_snapshot_renderer` is the SDL3 GPU consumer of running frames. It
owns asset/pipeline caching by shipping asset name, model transforms, and pose
palettes without owning mutable world state. Scene submission accepts an
externally owned SDL3 command buffer and render pass; synchronized readback is
a wrapper, not a requirement. The shipping self-test reproduces the same
manually assembled SDL3 scene with 0/76,800 pixels different, reproduces its
readback through an external pass with another 0/76,800 difference, and rejects
an unnamed asset and missing target. This is the boundary a future swapchain or
linear-light post-process target will consume. `host/scene_pair_capture` owns the diagnostic GPU
lifetime, GLES row conversion, checked PNG output, comparison, and reporting.
The mandatory `--scene-pair` run captures both backends at frame 30 of the same
real room snapshot: 3 instances, 2 skinned, 3 cached assets, and the live HUD
through the same two UI batches and 58 glyph quads. It exits
cleanly through the offscreen video driver with zero audio frames; GPU objects
are destroyed before SDL video teardown.

`host/render_overlay` freezes the engine's authored fade RGB and resolved
coverage for one frame. `host/gpu_overlay` composites that input as its own
portable SDL3 GPU pass after the scene. Its half-black discriminator changes
all 12 pixels of an `[204,102,51,255]` clear to `[102,51,26,191]` (within one
quantization step); a zero-coverage control rejects all 12. The alpha value is
part of the contract: the first live pair exposed `ONE` as the wrong source
alpha factor, because the shipping GLES `glBlendFunc` uses `SRC_ALPHA` for both
colour and alpha. After correction, the windowless running GLES/SDL3 fade pair
has identical mean alpha. The pair now keeps the HUD enabled and certifies
scene, UI, and fade ordering in both backends from the same frame snapshot.

`host/game_ui_content` owns authored labels and live-value formatting;
`host/render_ui` owns pixel layout, decoded-codepoint wrapping, and the shared
command stream. `host/gles_ui_renderer` is a temporary submission-only consumer
while `host/gpu_ui` owns the SDL3 font atlas, pipeline, and batches. The focused
shipping-font test renders HUD, level-up, and multi-line message inputs, rejects
an invalid batch, and substitutes solid glyph quads as an atlas negative. It
also decodes a multibyte copyright sign as one codepoint and lays out the real
Japanese `SYS_GAMEOVER_MSG` with zero missing glyphs. Line pitch comes from the
loaded font's scaled line height; the retired 22-pixel constant overlapped the
34-pixel shipping glyph cells.

`host/title_ui` owns the title attract/menu, opening crawl, and name-entry text
commands; `host/render_sprite` owns decoded RGBA pixels and aspect-fit geometry.
The running GLES title and `host/gpu_sprite` consume those shared results. The
mandatory windowless title capture proves the live mode chain reaches and draws
ModeTitle with zero audio frames. The SDL3 capture independently observes
34,625 sprite pixels and 10,253 text pixels against omission controls. The
remaining title dependency on GLES is presentation itself: the interactive
window is still created with `SDL_WINDOW_OPENGL` and submits the shared frame
through the transitional GLES consumers.

`host/gpu_presentation` follows Dusklight's presentation ownership boundary
without copying its graphics API. It owns the SDL window, claim/release order,
swapchain policy, and resize-dependent render targets. Existing scene, UI,
sprite, and overlay pipelines keep their verified RGBA8 contract; presentation
performs the final format-converting blit to BGRA or another SDL-selected
swapchain format. The attempted runtime cutover exposed the actual next
dependency: the running world's cache is still typed as the transitional
`mana::gles::Asset`, even though that GLES upload owner is now mechanically
separate from backend-independent `RenderAsset` and actor-model policy. The
world cache must switch to `RenderAsset` before the interactive window changes backend; a
second hidden GLES window or duplicated asset parser is explicitly not an
acceptable bridge.

SDL's official Shadercross build is a regeneration tool, not a runtime
dependency. On Linux, `tools/bootstrap_shadercross_linux.sh` downloads and
hash-verifies the pinned official Actions artifact in gitignored `scratch/`.
`tools/compile_shaders.sh` also accepts `SHADERCROSS` and `SHADERCROSS_LIB` for
other official builds.

## Migration order

1. ~~Move immutable texture and geometry upload into the GPU-assets owner while
   retaining CPU `Model` and `TextureSet` data independently of the backend.~~
2. ~~Feed the running `RenderSnapshot` into the tested SDL3 GPU scene owner,
   then compare its offscreen capture against GLES at the same frame.~~
3. Route the resulting scene into the game loop, then port ~~running UI~~, ~~boot/title UI~~, ~~fade~~, and
   capture as separate passes. Delete each
   GLES owner when its SDL3 GPU replacement passes its discriminator; do not
   maintain two permanent renderers.
4. ~~Add an isolated presentation owner while keeping automated work
   texture-backed, windowless, unpaced, and silent.~~ **DONE:** the owner and
   explicit smoke path exist; deterministic readback remains the mandatory
   gate.
5. ~~Extract the transitional GLES upload type from the backend-independent
   render module.~~ **DONE:** `host/gles_asset` is the only owner of its GL
   handles. Next switch the world cache to plain `RenderAsset`, wire the live
   frame compositor to `host/gpu_presentation`, and remove the GLES owner after
   the paired capture supplies its final differential evidence.

## Lighting and image quality

The shipping model declaration contains no normals. The recovered vanilla
`mLight` contract is point-colour attenuation plus direct/ambient colour, with
defaults that reduce to texture times vertex colour; it is preserved by
`DirectionalLight::Vanilla`. Better surface shading is therefore labelled as a
PORT CHOICE rather than presented as recovered behaviour.

`host/render_normals` generates area-weighted smooth normals once at asset load.
The GPU asset interleaves that derived stream without changing the shipping
model bytes. Static and two-bone skinned vertex shaders apply a centralized
`DirectionalLight`: ambient 0.55 plus diffuse 0.45 sums to the vanilla maximum,
and a diagonal overhead direction avoids privileging one wall axis. Model yaw
rotates the light into local space; skinned normals use the same joint rows as
positions with direction `w=0`. These values live in one named configuration,
not Lua, textures, or scattered draw sites. The mandatory offscreen A/B and
equal-ambient controls cover a room and skinned hero with no window or audio.

This removes the washed-out flat baseline without claiming completion of image
quality. A linear-light scene target, authored vanilla `mLight` point colours,
and tone mapping remain separate future work.
