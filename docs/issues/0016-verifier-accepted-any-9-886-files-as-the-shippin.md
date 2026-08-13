---
id: 16
title: Verifier accepted any 9,886 files as the shipping corpus
status: resolved
symptom: verify.sh treated a total file count of at least 9,886 as proof that scratch/dump was the complete MPK extraction
tags: tooling,assets,mpk,corpus,diagnostics,verification
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

The verifier counted all regular files under `scratch/dump` and compared only `n >= 9886`. It did not compare archive paths or declared sizes. One missing shipping member plus one unrelated file retained the count, and per-format parsers generally accepted reduced denominators.

## Fix

`mpk.py --check-dir` reads the authoritative archive directory, requires an exact relative-path set, and compares every present member against its declared uncompressed size. `verify.sh` now requires the archive and this identity check before any parser.

## Evidence

The real extraction reports 9886 expected / 9886 present / 0 missing / 0 extra / 0 wrong size. The shipping verifier atomically exercises one missing member, restores its pathname with a zero-byte wrong-size payload, and adds one extra file; all three exit nonzero and the EXIT traps restore the corpus. The subsequent full parser/game verifier passes.
