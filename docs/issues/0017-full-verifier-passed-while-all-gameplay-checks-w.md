---
id: 17
title: Full verifier passed while all gameplay checks were skipped
status: resolved
symptom: verify.sh treated a missing build/mana or runtime asset directory as a skip and could still print ALL PARSERS PASSED
tags: tooling,gameplay,diagnostics,verification,preflight
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

All game self-tests, opening lifecycle checks, audio, and room census lived inside an optional `if [ -x build/mana ] && [ -d scratch/raw/assets ]` block. The else branch merely printed SKIPPED; parser checks could then leave the final status at success.

## Fix

The verifier now runs one shared `check_runtime` precondition before corpus or test work. Missing/non-executable `build/mana` and missing runtime assets each report that zero gameplay checks ran and terminate the verifier. The optional wrapper and skip branch are removed.

## Evidence

The same shipping preflight function is exercised with a nonexistent binary and a nonexistent asset directory; both return nonzero with explicit zero-work diagnostics. With real prerequisites, the complete verifier passes and contains no SKIPPED branch.
