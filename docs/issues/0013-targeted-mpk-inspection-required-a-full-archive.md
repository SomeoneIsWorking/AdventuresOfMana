---
id: 13
title: Targeted MPK inspection required a full archive extraction
status: resolved
symptom: Inspecting one authoritative archive member forced decompression of all 9,886 payloads and took minutes
tags: tooling,assets,mpk,diagnostics
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

`tools/asset/mpk.py` coupled directory lookup to its full-corpus extraction loop. It had list-all and extract-all modes, but no exact member operation, so inspecting one Lua script performed thousands of unrelated LHA decodes.

## Fix

Added `-e/--entry`: decode the directory once, require exactly one exact pathname match, and inflate only that payload through the existing exact-compressed-byte-consumption validator. Zero matches print `scanned N entries, matched 0`; duplicate matches are refused as ambiguous.

## Evidence

The shipping `sk1/M0001_00_00.lua` extraction reports 9,886 scanned / 1 extracted and byte-compares equal to `scratch/dump`; a nonexistent name reports 9,886 scanned / 0 matched and exits nonzero. Both classes run in `tools/verify.sh`.
