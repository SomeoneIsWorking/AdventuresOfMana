# Renderer architecture

## Reference and target

The Dusklight checkout identified by the machine-global port guide is the
structure and UI ownership reference for this port. Its useful pattern is a small host composition point
with separate presentation, graphics, settings, input, audio, save, diagnostic,
and UI owners. Adventures of Mana uses SDL3 GPU directly; it follows those
boundaries without copying Dusklight's backend or platform-specific code.

SDL3 GPU is the shipping game and title renderer. GLES2 remains linked only for
the explicit single-model inspection tool. It is not initialized by the default
game, screenshots, boot/title capture, or automated gameplay runs.

## Owners

| Owner | Responsibility |
|---|---|
| `host/gpu_device` | SDL video lifetime, GPU device, texture-backed targets, command submission, and synchronized readback |
| `host/gpu_presentation` | window and swapchain claim/release, resize-dependent RGBA/depth targets, format conversion, and immediate/vsync selection |
| `host/gpu_runtime_renderer` | shipping runtime graphics lifetime and the choice between presentation and texture readback |
| `host/gpu_frame_renderer` | one immutable game frame in scene -> UI -> authored-fade order |
| `host/gpu_snapshot_renderer` | GPU asset cache, model transforms, skin palettes, and scene submission from `RenderSnapshot` |
| `host/gpu_ui`, `host/gpu_sprite`, `host/gpu_overlay` | font/UI batches, boot/title art, and final fade composition |
| `host/render_asset` | backend-independent model and texture retention, material resolution, bounds, and generated normals |
| `host/render_camera`, `host/render_snapshot` | resolve mutable engine state once and freeze the render input for a frame |
| `host/render_ui`, `host/title_ui`, `host/render_sprite`, `host/render_overlay` | backend-independent UI, title, sprite, and fade inputs |
| shader pack | eleven HLSL sources compiled to tracked SPIR-V, DXIL, and normalized MSL artifacts |

The runtime renderer accepts immutable frame data and never reaches into Lua or
mutable `World` state. The game loop owns simulation and builds a
`RenderSnapshot`, `UiFrame`, and `FadeOverlay`; `RuntimeRenderer` owns the GPU
objects that consume them. This is the same composition boundary used by
offscreen capture, so tests exercise the shipping implementation instead of a
test-only renderer.

`RenderAsset` is the single CPU authority for parsed geometry and textures.
`gpu_snapshot_renderer` uploads it lazily by shipping asset name. The former
GLES runtime cache, UI submitter, and paired-capture adapter were deleted after
the live SDL3 path reproduced the same three-instance frame with two skinned
actors, UI, and half fade. Keeping both runtimes would have left two resource
owners and required a hidden GL window in automated tests.

## Rendering and shading

`gpu_scene` submits every opaque material range before blended ranges while
sharing one depth target. Static and two-bone skinned pipelines consume the same
80-bone pose evaluator. `render_normals` derives area-weighted normals from the
shipping indexed geometry; the shader applies centralized directional lighting
with ambient fallback for unsupported or degenerate vertices. Shipping controls
prove enhanced room and hero pixels differ from equal-ambient and vanilla
renders rather than merely changing global brightness.

The frame compositor renders UI after the depth-tested scene and applies the
engine's authored fade last. The UI path uploads the shipping R8 font atlas and
consumes shared decoded-codepoint layout. Boot/title art uses shared decoded RGBA
sprites and aspect-fit geometry. Presentation renders into the same verified
RGBA/depth contract, then performs a final format-converting blit to the actual
swapchain.

## Windowless, unpaced, silent verification

Normal automated gameplay passes `--no-window --no-audio` (or inherits both
from `--opening-story`) and advances on a fixed simulation step without wall
clock pacing. A non-capture run creates neither a renderer nor an SDL window.
Capture runs create an SDL3 GPU device and texture-backed target but still zero
SDL windows. The runtime queries SDL's live window inventory and fails if this
invariant is false; the verifier requires the `windowless run: 0 SDL windows`
result along with zero decoded audio frames.

The focused GPU tests initialize no audio subsystem and create no windows. They
cover device/readback wrong-color controls, shader-pipeline omission controls,
shipping static and skinned assets, depth and blend policy, generated normals,
directional lighting, font/UI, sprite/title, and fade. The optional
`--presentation-smoke` test is the only renderer test that deliberately creates
a window.

## Tooling contract

`./run.sh` builds and launches the current game target with no hidden backend or
mode flag. It refuses an unconfigured build and names a missing shipping archive
before launch. `tools/verify.sh` is the normal gate. It regenerates all 33 shader
artifacts, checks the exact 9,886-member corpus, runs focused renderer controls,
captures the live SDL3 scene/title paths, and drives the unchanged shipping Lua
scripts through the continuous story suite. Recurring cleanup is centralized in
`tools/clean_verify_outputs.sh`; sessions and the verifier do not issue ad-hoc
cleanup commands.
