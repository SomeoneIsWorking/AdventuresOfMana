---
id: I020
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

SDL3 GPU external render-pass snapshot discriminator

## Validated by

Positive: caller-owned Device::RenderAndReadback command/pass and SnapshotRenderer::Draw reproduce the wrapper in 0/76,800 pixels. Other answer: calling the shipping Draw path with a null command/pass rejects exactly 'scene draw has no command buffer'; material-order and off-frustum controls continue to report 748 and 0 differences respectively.

## Known failure modes

(none recorded yet)
