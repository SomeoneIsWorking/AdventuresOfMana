---
id: C029
kind: claim
status: holds
created: 2026-08-14
tags: progression,hydra,player-state
depends: src/engine/script.cpp, src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 13:07:20
---

## Claim

MainPlayer Lua HP and MP access shares the authoritative combat PlayerStats, and the continuous unseeded Hydra route preserves Silver Key 30 for the main M0013_02_02 gate and completes the shipping M0013_11_00 recovery spring offscreen with zero decoded audio

## Evidence

Full ./tools/verify.sh on 2026-08-14: movement selftest executed shipping _HEALSPRING from damaged HP/MP through its authored wait and reached both maxima; continuous fixed-step uncapped run consumed key 30 on side 0, entered Recovery, logged completed spring, video driver offscreen, and decoded 0 sounds / 0 frames.

## What would falsify it

if Lua MainPlayer HP/MP reads diverge from combat PlayerStats, the shipping recovery coroutine fails to refill damaged live state, the main gate does not consume the equipped key, the continuous run fails to complete Recovery, opens a window, or decodes audio

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the PlayerStats bridge and corrected recovery route: 51 movement cases including the shipping damaged/full healing discriminator; main key side 0 consumed; Recovery entered and completed; SDL offscreen; audio decoded 0 sounds / 0 frames.

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

## Re-confirmed 2026-08-14

Reverified after commit f5b4048 by full ./tools/verify.sh: scratch/logs/verify-sdl3-gpu-pipeline-final.log ALL PARSERS PASSED on 2026-08-14; mandatory story remained fixed-step uncapped, SDL-offscreen, and decoded 0 audio frames.

## Re-confirmed 2026-08-14

Reverified after commit bff6051 by the complete mandatory ./tools/verify.sh run in scratch/logs/verify-sdl3-gpu-assets-final.log on 2026-08-14; ALL PARSERS PASSED, the continuous story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, and audio decoded 0 sounds / 0 frames.

## Re-confirmed 2026-08-14

Reverified after commit 7a2b1ed by the complete tools/verify.sh gate in scratch/logs/verify-sdl3-gpu-skinning-final.log: all focused positives and negatives passed, the unseeded story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, audio decoded 0 frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after commit 9dea2fe by the complete tools/verify.sh gate in scratch/logs/verify-sdl3-gpu-scene-final.log: all focused positives and negatives passed, the SDL3 GPU shipping scene wrote its checked capture, the unseeded story settled at sccnt=20 after 21961 fixed-step uncapped offscreen frames, audio decoded 0 frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after commit 7b7e22c by the complete tools/verify.sh gate in scratch/logs/verify-render-snapshot-final.log: all focused positives and negatives passed, CameraTracker reported 7/7 and RenderSnapshot 3/3, both 21961-frame story runs stayed fixed-step uncapped/offscreen with 0 audio frames, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified after b07f16a and the external-pass changes by the complete tools/verify.sh gate in scratch/logs/verify-external-gpu-pass-final.log: all focused positives and negatives passed, both 21,961-frame story runs stayed fixed-step uncapped/offscreen with 0 audio frames, the live SDL3 pair and external-pass parity passed, and ALL PARSERS PASSED.

## Re-confirmed 2026-08-14

Reverified at implementation commit 3bee232 by complete tools/verify.sh in scratch/logs/verify-sdl3-fade-final.log: all focused positives and negatives passed, 21/21 portable shaders regenerated, the 0.500 scene/fade pair stayed offscreen with 0 audio frames, both 21,961-frame story runs passed, and ALL PARSERS PASSED.
