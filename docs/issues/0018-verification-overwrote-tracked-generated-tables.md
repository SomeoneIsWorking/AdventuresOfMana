---
id: 18
title: Verification overwrote tracked generated tables
status: resolved
symptom: Running verify.sh regenerated object, weapon, and item docs/includes directly in the worktree, including on validation-failure paths
tags: tooling,generators,worktree,diagnostics,verification
created: 2026-08-13
updated: 2026-08-13
---

## Root cause

The full verifier invoked three emitters from the repository root. Each writes tracked files as part of normal execution; some emit even when their accumulated validation result is false. A read-only verification command could therefore overwrite user edits or leave generated diffs behind. The source binary used by those generators was also optional even though other verifier stages require it.

## Fix

`verify.sh` runs all three emitters from `scratch/logs/generated-tables` with absolute read-only inputs, then byte-compares five outputs against their tracked counterparts. It removes only the known scratch outputs before generation. A deliberately altered scratch copy proves the comparator rejects a mismatch. `scratch/raw/libmcfandroid.so` is now a mandatory preflight input with a zero-work negative.

## Evidence

Full verification reports all five generated artifacts byte-identical and rejects the altered copy. A hash of the only pre-existing worktree diff is identical before and after the run, and `git status` shows no generator-created tracked changes. Missing game, runtime assets, and source binary are all rejected by the shared preflight.
