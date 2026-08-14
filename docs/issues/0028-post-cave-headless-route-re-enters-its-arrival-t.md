---
id: 28
title: Post-cave headless route re-enters its arrival transition
status: resolved
symptom: After M0011_00_02/in_1 exits at M0000_09_06/in_2, the windowless unpaced driver immediately enters in_2 and loops through the cave forever.
tags: tooling,navigation,event-box,progression
created: 2026-08-14
updated: 2026-08-14
---

## Evidence

- `scratch/logs/post-cave-manor-try6.log` repeatedly records
  `M0011_00_02/in_1 -> M0000_09_06`, then the planned `WALL_UP` route enters
  `in_2` and maps back.
- The planner reports 442/1353 reachable samples and an exact 43-waypoint path,
  so this is not an empty-corpus or unreachable-target result.

## Root cause

`MapJump` places the character centre just outside an elevated arrival box, but
the shipping character volume still overlaps that box and owns its floor. The
port grounded only the centre point, did not retain the arrival overlap while
moving onto the ledge, and therefore treated the first contact as a fresh entry
which invoked the reverse callback.

## Acceptance

A real windowless, no-audio, unpaced continuation leaves `M0000_09_06` through
the authored forward route without invoking `in_2`, reaches Kett Manor, enters
`bed_01`, and settles at scenario 15. A missing route target exits nonzero and
reports scanned samples, reachable side bands, floor maxima, and rise-frontier
rejection classes.

## Resolution

Map-jump grounding now samples every arrival box touched by the established
15-unit body radius, retains that box's measured floor until point ground at the
same level takes ownership, and suppresses only the continuing overlap with the
same arrival volume. The route instrument also gained exact fallback paths,
midpoint height validation, outward contact-coordinate handoff, progressive
vertical goals, measured 30-unit stair stepping, and nonzero diagnostic failure.
`scratch/logs/kett-bed.log` is the positive: 9,259 fixed-step uncapped frames,
offscreen video, zero decoded audio, `M0011_00_02/in_1` through Kett's three
visible steps and `bed_01`, ending settled at `sccnt=15`.
