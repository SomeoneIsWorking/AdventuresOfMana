---
id: I021
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

SDL3 GPU authored-fade composition discriminator

## Validated by

Positive: half-black coverage changes all 12 pixels including alpha to [102,51,26,191] +/-1 and the running pair reports 0.500 coverage. Other answer: zero coverage leaves [204,102,51,255], causing 12/12 exact expected-fade mismatches. The live paired images also caught the initially wrong ONE source-alpha factor through a 1.0-versus-0.75 alpha discrepancy before SRC_ALPHA fixed it.

## Known failure modes

(none recorded yet)
