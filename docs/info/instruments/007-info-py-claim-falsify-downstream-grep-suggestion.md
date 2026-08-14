---
id: I007
kind: instrument
status: DISTRUSTED
created: 2026-08-14
distrusted_on: 2026-08-14
---

## Instrument

info.py claim falsify downstream grep suggestion

## Validated by

C019 falsification emitted git grep -n 'An', which does not identify the claim; explicit git grep -E 'C019|M0000_10_06' found the real downstream set

## Known failure modes

(none recorded yet)

## DISTRUSTED 2026-08-14

The generated command grepped the first word of the claim ('An') instead of stable claim id C019 or a meaningful dependency, so it cannot audit downstream reliance.

> Every result this instrument produced is suspect until it is re-validated.
