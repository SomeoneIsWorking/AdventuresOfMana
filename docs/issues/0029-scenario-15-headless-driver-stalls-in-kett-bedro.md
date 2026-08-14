---
id: 29
title: Scenario 15 headless driver stalls in Kett bedroom
status: resolved
symptom: An unseeded --opening-story --continue-story --stop-item 30 run reaches sccnt 15, then targets local (30,30) in M0012_00_00 and times out instead of leaving Kett Manor.
tags: headless-driver,story-route,kett,silver-key
created: 2026-08-14
updated: 2026-08-14
---

## Evidence

Windowless fixed-step no-audio run in `scratch/logs/silver-key-route.log`
reached the authored `bed_01` scene and scenario 15, then logged a route from
the post-scene position back toward local `(30,30)`; it produced no subsequent
room transition before the 240-second external timeout.

## Root-cause hypothesis

The scenario driver has an explicit `M0012_00_00` bed target only while
`sccnt == 14` and no scenario-15 departure route. It therefore falls through
to the generic initial `(30,30)` walk target. Confirm the authored reverse
route and outside destination before implementing.

### Resolution (2026-08-14)
The scenario driver had no sccnt>=15 reverse route out of Kett Manor, and an older post-Matock M0000_10_09 branch remained active after scenario 14. After reaching the lizardmen, auto-combat used X/Z-only range and swung forever at enemies 30 units below; the story driver also left the mandatory regimen menu unattended. The fix adds the authored manor/east/south route, scopes the stale branch to sccnt 14, measures auto-combat range in 3D, logs every target transition, selects Warrior through the existing level-up path, and targets the live Silver Key _BOX actor. An unseeded offscreen/no-audio run now acquires item 30 in 10,831 frames.
