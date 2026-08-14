---
id: C029
kind: claim
status: holds
created: 2026-08-14
tags: progression,hydra,player-state
depends: src/engine/script.cpp, src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 10:31:40
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
