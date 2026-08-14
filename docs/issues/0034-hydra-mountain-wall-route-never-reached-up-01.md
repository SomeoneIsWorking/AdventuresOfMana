---
id: 34
title: Hydra mountain wall route never reached up_01
status: resolved
symptom: The continuous fixed-step route arrived in M0013_06_05 at y=330 but could neither plan to the mountain exit nor preserve the scripted wall transitions needed to reach it.
tags: tooling,navigation,event-box,wall,hydra
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

Four host-instrument defects overlapped. Ordinary event goals required the character centre to enter a box even when static collision correctly stopped its body at the boundary. `ChrSetPos` inside wall callbacks updated the actor but the host then overwrote it from stale player coordinates. Floor type 1 movement was still treated as ground-plane X/Z motion even though `AppCharacterBase::Update` moves on X/Y and holds Z on the authored wall plane. Finally, the router required paired `WALL_UP`/`WALL_DN` volumes, while the mountain entrance is an authored lone `WALL_UP` terrain region.

## What was tried / dead ends

Driving the descending `wall_01` through `wall_07b` chain reached the north edge, but the inferred destination `M0013_06_04` has no world-table room or model. A 30-unit body probe for every event while on a wall made progress but falsely fired `wall_01` from outside its strict box and reset floor type. The broad probe was removed; body contact is used only for a movement attempt that static collision actually blocked.

## Resolution

The route planner models blocked character-body contact, adopts scripted `ChrSetPos` results, and drives floor type 1 in X/Y. Lone `WALL_UP` containment enters wall mode without inventing a paired destination. The driver targets authored `up_01`, remembers that the mountain was climbed, and exits left from the return room instead of immediately selecting `down_01` and looping.

A clean unseeded run reaches `M0013_00_00` after 12,582 fixed-step frames. It uses SDL offscreen, never fires `wall_01` during the mountain ascent, and reports `audio decoded 0 sounds / 0 frames`.
