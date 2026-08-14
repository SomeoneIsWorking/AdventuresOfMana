---
id: I008
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

PyGhidra 12 DecompDump workflow

## Validated by

Wrong unrebased 0x2abb54 target produced no function; rebased 0x3abb54 through pyghidra --skip-analysis emitted build/decomp/003abb54.c and 003ae460.c; analyzeHeadless Python postScript failed before execution and is not the Ghidra-12 path

## Known failure modes

Ghidra's imported image base can differ from the binary VMA. An unrebased
address produces no function, so validate one known-positive target and apply
the measured image-base delta rather than treating empty output as evidence.
Ghidra 12 Python scripts require the `pyghidra` launcher; `analyzeHeadless`
selects the wrong provider and fails before the script executes.
