---
id: 15
title: Command API extraction accepted partial coverage
status: resolved
symptom: extract_cmd_api.py could report fewer than 200 resolved Lua commands yet return success, allowing verify.sh to pass
tags: tooling,lua,api,diagnostics,verification
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

The extractor only failed when it found zero registrations. Its printed `X/Y` name and implementation counts were informational, so any partial nonzero result exited 0. The verifier redirected the report and trusted that exit code.

## Fix

The shipping extractor now requires exactly 200 registrations, 200 names, and 200 implementations. The verifier mechanically rewrites the first real `tolua_function` call in a scratch disassembly, preserving the other 199, and requires the resulting 199/200 run to fail. Expected negative diagnostics are checked silently so a passing transcript does not contain a misleading `FATAL` line.

## Evidence

The intact binary reports 200/200 for all three dimensions. The one-registration-removed discriminator reports 199/200 and exits nonzero. Full `tools/verify.sh` passes with an explicit negative-gate summary and no leaked expected fatal diagnostic.
