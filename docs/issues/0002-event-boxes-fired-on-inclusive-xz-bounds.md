---
id: 2
title: Event boxes fired on inclusive XZ bounds
status: resolved
symptom: Transitions could fire at an event-box boundary or at the wrong height
tags: gameplay,event-box,reverse-engineering
created: 2026-08-13
updated: 2026-08-13
---

## Root cause


## What was tried / dead ends


## Resolution

### Resolution (2026-08-13)
The port ignored Y and used inclusive X/Z comparisons. AppEventBoxBase::IsHit at 0x2bb0f8 requires enabled && !no_touch and strict lower < point < upper tests for all three axes. Centralized that predicate and added six boundary/state tests.
