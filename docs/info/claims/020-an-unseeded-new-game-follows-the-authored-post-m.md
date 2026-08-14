---
id: C020
kind: claim
status: holds
created: 2026-08-14
tags: progression,tooling,windowless,audio
depends: src/host/main.cpp#main, src/mcf/assets.cpp#Inventory::Add, src/mcf/mcf.h, src/engine/item_uses.inc, tools/asset/item_table.py, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 07:51:12
---

## Claim

An unseeded new game follows the authored post-Matock route into M0011_00_00, consumes two of seven Mattock uses on breakable rocks, and reaches M0011_00_02 windowlessly and silently

## Evidence

2026-08-14 ./tools/verify.sh pass: fixed-step uncapped route reached M0011_00_02 in 6785 frames via the two-vine M0000_08_05/M0000_09_05 detour and M0000_09_06 in_1; SDL video driver was offscreen, gameplay decoded 0 sounds/frames, two object-id-9 contacts consumed Mattock uses from 7 to 5, and generated item_uses.inc matched tblItem bytes.

## What would falsify it

The mandatory gate selects M0000_10_06, bypasses either breakable rock, consumes a non-binary-derived number of uses, falls into the M0011_00_01 pit, creates a visible window, decodes audio, or fails to reach M0011_00_02.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Silver Key doorway changes, including all asset/parser negative controls, gameplay gates, selftests, 993-room census, cmd API, world map, and the continuous offscreen/no-audio progression through M0013_03_01.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 after removing unused route branches; all parser negatives, gameplay gates, selftests, 993-room census, command/world-map checks, and continuous offscreen/no-audio progression through M0013_03_01 passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra-dungeon pressure-switch changes, including all negatives, gameplay gates through M0013_00_04, 48/48 movement cases, 993-room census, cmd API, and world-map checks.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra mountain change; all claim-specific self-tests and continuous fixed-step offscreen zero-audio progression gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the verifier was made to rebuild mana first; 62 inventory cases and all continuous offscreen zero-audio gates passed.
