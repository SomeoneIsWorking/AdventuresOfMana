---
id: C023
kind: claim
status: holds
created: 2026-08-14
tags: progression,equipment,navigation
depends: src/host/main.cpp, src/engine/script.cpp, src/mcf/assets.cpp, src/mcf/mcf.h, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 07:32:23
---

## Claim

An unseeded fixed-step uncapped offscreen run equips and uses the Silver Key and reaches M0013_03_01 with no decoded audio

## Evidence

tools/verify.sh passed on 2026-08-14: the mandatory continuous gate logged equipped item 30 in slot 4, M0000_14_08/in_01, mapjump M0013_03_01, requested stop-room success at frame 11740, offscreen video, and 0 sounds / 0 frames; movement selftest 46/46 and inventory selftest 60/60 also passed.

## What would falsify it

Any unseeded continuous run fails to acquire/equip item 30, does not fire M0000_14_08/in_01, stalls before M0013_03_01, opens a window, decodes audio, or exits nonzero.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Silver Key doorway changes, including all asset/parser negative controls, gameplay gates, selftests, 993-room census, cmd API, world map, and the continuous offscreen/no-audio progression through M0013_03_01.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 after removing unused route branches; all parser negatives, gameplay gates, selftests, 993-room census, command/world-map checks, and continuous offscreen/no-audio progression through M0013_03_01 passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra-dungeon pressure-switch changes, including all negatives, gameplay gates through M0013_00_04, 48/48 movement cases, 993-room census, cmd API, and world-map checks.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra mountain change; all claim-specific self-tests and continuous fixed-step offscreen zero-audio progression gates passed.
