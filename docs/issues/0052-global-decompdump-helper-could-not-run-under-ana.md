---
id: 52
title: Global DecompDump helper could not run under analyzeHeadless
status: resolved
symptom: Ghidra 12 reported 'DecompDump.py: Ghidra was not started with PyGhidra. Python is not available' before decompiling any requested shading function
tags: tooling,ghidra,decompdump,reverse-engineering,workflow
created: 2026-08-14
updated: 2026-08-14
---

Root cause identified in the helper source: the skill documentation and file
comment both say every Jython script starts with `#@runtime Jython`, but
`$HOME/.agents/skills/decomp-port/DecompDump.py` omitted that directive. Ghidra
therefore selected the PyGhidra provider instead of Jython when invoked by
`analyzeHeadless`. Fix the global helper itself, rerun the same three-address
query, and only resolve after all three `.c` outputs exist.

### Resolution (2026-08-14)
Added the missing #@runtime Jython directive to the global DecompDump helper, then added documented DECOMP_SLIDE support because this ELF project's Ghidra image is rebased +0x100000 from readelf link VAs. The exact original target list with DECOMP_SLIDE=0x100000 now writes all three requested outputs (14,225 / 2,290 / 670 bytes) with no DECOMP FAIL or SCRIPT ERROR. The inventory's explicit 003a631c/003b0400/00458dec entries supplied the discriminator rather than guessing the slide.
