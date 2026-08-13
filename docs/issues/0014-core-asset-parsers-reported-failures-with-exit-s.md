---
id: 14
title: Core asset parsers reported failures with exit status zero
status: resolved
symptom: stex, smdl, smot, and scol could print FAILED or parse 0 files while verify.sh still passed
tags: tooling,assets,parsers,diagnostics,verification
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

The four parser CLIs accumulated and printed parse errors but never converted them into a process exit status. Their default empty glob also produced a successful 0/0 summary. `tools/verify.sh` already used `python ... || fail=1`, but that gate was inert because the tools always returned zero.

## Fix

Each CLI now refuses an empty corpus with an explicit scanned-0 fatal message and exits 1 when any input fails. The verifier runs both negative classes for every parser: an empty default corpus and a zero-byte malformed file. The stale handwritten 18-case combat label was also removed; the live self-test reports its own current 30-case denominator.

## Evidence

Full `tools/verify.sh` passes on the real corpus: 1319 texture descriptors, 1375 models, 1721 motions, and 992 collision meshes. All eight negative invocations exit nonzero and print a nonzero denominator or scanned-0 message.
