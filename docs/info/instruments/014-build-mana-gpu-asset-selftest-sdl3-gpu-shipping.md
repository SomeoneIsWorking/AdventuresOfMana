---
id: I014
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

build/mana_gpu_asset_selftest SDL3 GPU shipping-asset renderer

## Validated by

On shipping M0001_00_00, the textured render changes 3868/6912 pixels and all 3868 differ from a forced-white material render; the same readback with draw submission disabled changes 0/6912. Full tools/verify.sh passed in scratch/logs/verify-sdl3-gpu-assets-final.log.

## Known failure modes

(none recorded yet)
