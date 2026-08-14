---
id: I022
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

global DecompDump Jython helper with explicit image slide

## Validated by

Failure classes shown first: without #@runtime Jython it emitted SCRIPT ERROR and 0 outputs; without the +0x100000 image slide it wrote 1/3 and emitted two DECOMP FAIL lines. After fixing both, the original three link VAs produced 3/3 nonempty C files at Ghidra VAs 003a631c, 003b0400, and 00458dec.

## Known failure modes

(none recorded yet)
