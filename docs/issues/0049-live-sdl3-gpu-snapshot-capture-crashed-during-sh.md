---
id: 49
title: Live SDL3 GPU snapshot capture crashed during shutdown
status: resolved
symptom: --scene-pair wrote both PNGs and logged final state, then exited 139 with a core dump
tags: sdl3-gpu,tooling,shutdown,lifetime,scene-pair
created: 2026-08-14
updated: 2026-08-14
---

Root cause: the diagnostic GPU device and its cached asset/pipeline resources were declared in the running-world lexical scope, but SDL_Quit() was called before that scope ended. Their destructors consequently called SDL GPU teardown APIs after the SDL video subsystem was gone. Fix: ScenePairCapture now owns the diagnostic backend and is explicitly destroyed before the GL context, window, and SDL video teardown. The real M0001_00_00 paired run writes both images, reports 3 instances / 3 cached assets / 0 audio frames, and exits 0.
