---
id: I019
kind: instrument
status: DISTRUSTED
created: 2026-08-14
distrusted_on: 2026-08-14
---

## Instrument

build/mana --scene-pair running SDL3 GPU snapshot capture

## Validated by

Positive: real M0001_00_00 frame 30 reported 3 instances (2 skinned), 3 cached assets, wrote both nonempty images offscreen, decoded 0 audio frames, and exited 0. Other answers: the shipping adapter rejects an unnamed asset, its manual-scene parity reports 0 differing pixels while the live GLES/SDL3 comparison reports 205881 exact-byte differences, and the pre-fix lifetime defect reproduced exit 139 after output before explicit pre-SDL teardown fixed it.

## Known failure modes

(none recorded yet)

## DISTRUSTED 2026-08-14

The --scene-pair option and hidden GLES window were intentionally removed; use the live SDL3 GPU capture plus focused renderer controls.

> Every result this instrument produced is suspect until it is re-validated.
