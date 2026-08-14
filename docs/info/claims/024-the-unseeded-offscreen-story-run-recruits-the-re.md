---
id: C024
kind: claim
status: holds
created: 2026-08-14
tags: progression,event-box,tooling
depends: src/host/main.cpp, src/engine/script.cpp, src/engine/script.h, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 11:19:33
---

## Claim

The unseeded offscreen story run recruits the Red Mage and operates the first Hydra-dungeon pressure switch

## Evidence

Final ./tools/verify.sh passed on 2026-08-14: the mandatory fixed-step uncapped run entered M0013_02_00/sw_01 before down_1, mapjumped to M0013_00_04 at frame 12250, used SDL offscreen, and decoded 0 audio frames; movement selftest passed 48/48 including both switch payload and object visibility classes.

## What would falsify it

A clean unseeded run bypasses sw_01, fails to enable/enter down_1, fails to reach M0013_00_04, opens a window, decodes audio, or exits nonzero.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra-dungeon pressure-switch changes, including all negatives, gameplay gates through M0013_00_04, 48/48 movement cases, 993-room census, cmd API, and world-map checks.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Hydra mountain change; all claim-specific self-tests and continuous fixed-step offscreen zero-audio progression gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the verifier was made to rebuild mana first; 62 inventory cases and all continuous offscreen zero-audio gates passed.

## Re-confirmed 2026-08-14

Full offscreen/dummy-audio fixed-step gate tools/verify.sh passed on 2026-08-14; scratch/logs/verify-keyring-structure-v4.log ends VERIFICATION OK.

## Re-confirmed 2026-08-14

Commit af6e3c5 passed the complete offscreen/dummy-audio fixed-step tools/verify.sh gate on 2026-08-14; gameplay and static/corpus evidence is recorded under scratch/logs/.

## Re-confirmed 2026-08-14

Commit fe20918 passed the complete fixed-step uncapped offscreen/dummy-audio tools/verify.sh gate through Hydra's authored sccnt=16 defeat transition on 2026-08-14; evidence is scratch/logs/verify-hydra-defeat.log.

## Re-confirmed 2026-08-14

Commit a80ff75: the complete mandatory ./tools/verify.sh passed in scratch/logs/verify-steward-wolf.log after the landed changes, exercising this claim's positive and negative checks offscreen; the extended continuous run settled at sccnt=19 with zero decoded audio frames.

## Re-confirmed 2026-08-14

Reverified unchanged behavior after commit 6e5d104 by full ./tools/verify.sh: scratch/logs/verify-chain-flail.log ALL PARSERS PASSED on 2026-08-14; mandatory story run was fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.

## Re-confirmed 2026-08-14

Reverified after commits adedb5d/addb897 by full ./tools/verify.sh: scratch/logs/verify-sdl3-gpu-negative.log ALL PARSERS PASSED on 2026-08-14; mandatory story remained fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.
