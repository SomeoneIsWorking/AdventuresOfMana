---
id: I009
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

Ghidra extension manifest parser

## Validated by

Before cleanup, PyGhidra printed Module manifest file error for both GHIDRA_MODULE_NAME and GHIDRA_MODULE_DESC lines; after removing unsupported KEY=value lines, the same DecompDump command emitted 003deb78.c with zero manifest errors, while an explicit rg negative found none

## Known failure modes

(none recorded yet)
