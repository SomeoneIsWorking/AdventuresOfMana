---
id: 4
title: Floor-anchored event boxes stayed at y=-1
status: resolved
symptom: Transitions and switches on elevated terrain could never trigger because AddEventBox floor sentinels were treated as literal heights
tags: gameplay,event-box,collision
created: 2026-08-13
updated: 2026-08-13
---

## Root cause


## What was tried / dead ends


## Resolution

### Resolution (2026-08-13)
In AddEventBox at 0x2c8598, y0 == -1 triggers a collision-floor probe; its floor becomes the lower bound and is added to the upper bound. The port now marks that sentinel in the script bridge and resolves it against the room collision mesh after loading.
