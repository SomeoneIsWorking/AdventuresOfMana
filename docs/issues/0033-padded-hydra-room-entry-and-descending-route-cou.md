---
id: 33
title: Padded Hydra room entry and descending route could not reach down_01
status: resolved
symptom: After crossing the Hydra upper log, the fixed-step driver entered M0013_01_00 in its outer padding strip and reported no reachable down_01 target; partial fixes then planned wall-crossing slope edges that live movement rejected.
tags: tooling,navigation,collision,map-object,hydra
created: 2026-08-14
updated: 2026-08-14
---

## Root cause

Three instrumentation gaps overlapped. A 330x270 room includes a 15-unit format margin around the ordinary playable area, but room entry moved only the 15-unit event-box centre offset inward instead of margin plus the measured 30-unit room-edge body radius. The route planner only supported ascending floor-following and its whole-edge stair exemption could hide a flat wall at the beginning of a descending edge. Finally, upward vertical staging remained enabled for an event volume below the player and descent permission was dropped as soon as the player centre entered the target Y range.

The placed-object bridge also discarded ODT +0x40. The binary proves this is the script id: AppObjectServer::GetScpId compares its argument to object +0x1a4, and AppObjectModel::DamageMove formats _BREAKOBJ_%d from the same field before starting a coroutine.

## What was tried / dead ends

Routing west was refuted by 0 reachable west-band samples. Allowing arbitrary large drops regressed the verified Kett return. Using the broad collision AABB for destination placement moved a verified 300x240 vine-room landing into non-walkable margin geometry. A global bidirectional stair rule selected an unrelated -15 floor pocket and was reverted.

## Resolution

Padded room entry crosses 15 units of format margin plus the 30-unit room-edge body radius while non-padded rooms retain their verified inset. Descending event routes persist descent ownership, validate the first edge from the live point, sweep every shipping movement sample against its current floor, and do not invoke upward staging. ODT parsers retain +0x40; placed-object visibility shares ObjVisible state; breaking an object starts its shipping _BREAKOBJ_<script-id> callback.

A fixed-step uncapped SDL-offscreen no-audio run now follows M0013_01_00 from y=30 down to y=0, consumes a Mattock use on object id 9/script id 1306, starts _BREAKOBJ_1306, enters down_01, and mapjumps to M0013_06_05 at y=330 after 12,328 frames with zero decoded audio.
