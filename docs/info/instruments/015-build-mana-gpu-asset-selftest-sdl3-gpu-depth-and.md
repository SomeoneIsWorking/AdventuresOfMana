---
id: I015
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

build/mana_gpu_asset_selftest SDL3 GPU depth and material blending

## Validated by

Shipping M0001_00_00 near-textured then far-white changes 0/6912 pixels with depth enabled and 3868/6912 with depth disabled; shipping M0000_00_03 has two blended water draws and changes 128/6912 pixels against the opaque-material control. Full tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-depth-blend-final.log.

## Known failure modes

(none recorded yet)
