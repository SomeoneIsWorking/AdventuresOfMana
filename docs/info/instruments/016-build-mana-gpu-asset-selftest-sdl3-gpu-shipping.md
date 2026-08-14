---
id: I016
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

build/mana_gpu_asset_selftest SDL3 GPU shipping skinning

## Validated by

Shipping C0000_00 positive renders 2812/6912 pixels through 2641 skinned vertices; shifting all 80 joint matrices changes 3723/6912 pixels; omitting the exact 960-float palette fails explicitly. The paired no-draw negative changes 0/6912. Full tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-skinning-final.log with offscreen video, 21961 uncapped frames, and 0 audio frames.

## Known failure modes

(none recorded yet)
