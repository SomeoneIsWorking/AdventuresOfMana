---
id: 1
title: AI +0xc68 writer missed by scalar-store scan
status: resolved
symptom: Mode 9 distance timer appeared unimplementable because actor +0xc68 had no identified writer
tags: reverse-engineering,ai,disassembly
created: 2026-08-13
updated: 2026-08-13
---

## Root cause


## What was tried / dead ends


## Resolution

### Resolution (2026-08-13)
The scan only matched scalar stores. AppCharacterBase::C2 loads a 16-byte vector from 0x9de50 and stores it at this+0xc64; its second lane initializes +0xc68 to 1.0. SetEnemyId overwrites only +0xc64 with the per-enemy speed.
