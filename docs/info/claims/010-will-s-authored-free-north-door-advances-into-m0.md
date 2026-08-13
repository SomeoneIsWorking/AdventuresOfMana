---
id: C010
kind: claim
status: holds
created: 2026-08-13
tags: gameplay,doors,progression
depends: src/engine/script.cpp#SetDoor, src/engine/world.cpp#SetDoor, src/host/main.cpp#RoomExitReq, tools/verify.sh
---

## Claim

Will's authored free north door advances into M0001_00_01 while that room's locked south door does not traverse

## Evidence

2026-08-13 tools/verify.sh live free-door run logged room exit 0 -> M0001_00_01 and ended there; paired KEY-door run ended in M0001_00_01 with no room-exit log; ordinary-edge run reached M0001_00_00; movement selftest covers door states; full verifier passed

## What would falsify it

Any change to SetDoor dispatch/state/reset, room-edge contact geometry, collision blocking, world-table lookup, room loading, or the paired free/key verifier runs
