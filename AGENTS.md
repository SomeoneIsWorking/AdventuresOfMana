# Project architecture rules

The structure and UI reference for this port is the Dusklight checkout identified by the machine-global port guide. Follow its ownership
pattern: a small host entry point that composes cohesive configuration, presentation, input, audio,
save, diagnostic, and UI modules. Do not copy platform-specific implementation mechanically.

Applied renderer ownership: `gpu_runtime_renderer` owns the shipping SDL3 GPU
device and optional presentation; `gpu_frame_renderer` composes immutable scene,
UI, and fade inputs; `gpu_presentation` owns the window/swapchain. GLES is not a
game backend and remains only in the explicit single-model inspection path.

USER 2026-08-14: "Please properly structure the project, don't put everything in one file/class, globally ban this behavior also"

- `src/host/main.cpp` is legacy orchestration debt. Do not add a subsystem, gameplay rule, parser,
  diagnostic implementation, or route table to it. Extract the touched responsibility behind a
  narrow interface first, and ratchet its permitted size downward in the same change.
- New C++ source files must have one cohesive owner and stay below the repository's enforced line
  limit. Splitting a god file into numbered parts, catch-all helpers, or a god class is prohibited.
- Engine behavior belongs under `src/engine/`; desktop orchestration and verification drivers belong
  under `src/host/`; binary/archive formats belong under `src/mcf/` or `tools/asset/`.
- `tools/check_structure.py` is part of the normal verification gate. Do not raise a legacy baseline
  to accommodate new code. A change that reduces a baseline must lower the recorded limit.
