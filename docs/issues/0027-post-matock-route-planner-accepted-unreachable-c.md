---
id: 27
title: Post-Matock route planner accepted unreachable cave floors
status: resolved
symptom: The unseeded run either stalled in M0000_08_06 or selected M0000_09_06 in_2 despite arriving on the isolated lower floor.
tags: tooling,navigation,collision,post-matock
created: 2026-08-14
updated: 2026-08-14
---

Root cause: point objectives were allowed to choose the nearest reachable sample without requiring that sample to lie inside the authored event box; the exact-node corridor also discarded its separately validated live-to-grid attachment segment, and a transient lane objective changed with the player every frame. The driver now requires event-box containment, retains stable phase coordinates, preserves the attachment segment in the exact-node corridor, and follows the measured lower connected component east. The mandatory unseeded gate reaches M0000_10_06 in fixed-step uncapped offscreen mode with zero decoded audio. Falsified if that gate stops before M0000_10_06, accepts an unreachable event-box floor, creates a window, or decodes audio.
