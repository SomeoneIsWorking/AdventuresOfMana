---
id: C001
kind: claim
status: holds
created: 2026-08-13
tags:
depends: scratch/raw/full.asm#_ZN16AppCharacterBaseC2Ev
---

## Claim

Actor +0xc68 is the fixed 1.0 movement multiplier initialized by AppCharacterBase::C2.

## Evidence

At 0x2a63bc the constructor loads q0 from VA 0x9de50; its four f32 values are 8.0, 1.0, 1.0, -20.0. At 0x2a63dc it executes str q0, [this + 0xc64], covering +0xc68.

## What would falsify it

A later write to actor+0xc68, or evidence that the q store does not execute for live actors.
