---
id: I017
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

build/mana_gpu_asset_selftest SDL3 GPU shipping scene

## Validated by

Shipping M0001_00_00 plus C0000_00 under one perspective camera changes 88/76800 pixels from room-only; moving the actor beyond the frustum changes 0. A paired no-depth control changes 748/6912 pixels between scene-wide and per-asset material ordering. Checked PNG capture succeeds and a missing output directory fails explicitly. Full tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-scene-final.log with offscreen video, 21961 uncapped frames, and 0 audio frames.

## Known failure modes

(none recorded yet)
