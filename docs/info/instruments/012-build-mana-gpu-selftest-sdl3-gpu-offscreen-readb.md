---
id: I012
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

build/mana_gpu_selftest SDL3 GPU offscreen readback

## Validated by

On 2026-08-14 the normal shipping path read exact black and magenta across 24/24 pixels with SDL offscreen, zero windows, and no audio; --negative-control passed the same magenta GPU result against black and exited nonzero with 12/12 mismatches and the first observed/expected RGBA.

## Known failure modes

(none recorded yet)
