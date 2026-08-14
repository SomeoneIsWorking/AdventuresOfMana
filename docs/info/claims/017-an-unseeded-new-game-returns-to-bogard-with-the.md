---
id: C017
kind: claim
status: holds
created: 2026-08-14
tags: gameplay,progression,party,verification
depends: src/engine/script.cpp#Dispatch, src/host/main.cpp#main, src/engine/world.h#PartyHandle, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 06:11:36
---

## Claim

An unseeded new game returns to Bogard with the live heroine party and completes the Matock objective at scenario state 14

## Evidence

2026-08-14 full ./tools/verify.sh pass: fixed-step offscreen --opening-story --continue-story restored binary-named PARTY_HEROINE across nine room loads, completed four total Bogard talks including the pendant and Matock dialogue, and stopped after 5202 frames at settled sccnt=14 with zero live coroutines and zero decoded audio frames. Movement selftest covers all nine party handles, idempotent AddParty, and AddParty(0) removal.

## What would falsify it

Falsified if the mandatory unseeded gate does not restore a live PARTY_HEROINE through the authored return, does not complete the pendant/Matock dialogue and settle at sccnt=14, opens a visible window, decodes audio, or the party identity/removal discriminator fails.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after persistent binary-named party identities and the sccnt=12-to-14 return route landed; all continuous story gates, focused self-tests, negative discriminators, exact asset corpus checks, and the 993-room census passed. The newest gate restored live PARTY_HEROINE across nine room loads and settled at sccnt=14 after 5202 offscreen fixed-step frames with zero decoded audio.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 with the repository-owned RE-frontier validator and its zero-entry negative enabled. All continuous story gates, self-tests, negative discriminators, exact 9886-member corpus checks, and 993-room census passed; PARTY_HEROINE remained live at settled sccnt=14 after 5202 offscreen frames with zero decoded audio.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the windowless render bypass, OpenDoor, inventory bridge, and Matock chest changes; all gameplay, self-test, corpus, frontier, and generated-artifact gates passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the post-Matock route-planner fix; all continuous fixed-step offscreen gates, focused self-tests, negative discriminators, exact asset corpus checks, the 993-room census, and the new 6279-frame silent route to M0000_10_06 passed.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed 2026-08-14 after replacing the false Chocobot-detour gate: all existing discriminators passed, and the corrected unseeded continuation reached M0011_00_02 in 6785 fixed-step uncapped offscreen frames with zero gameplay audio decode.

## Re-confirmed 2026-08-14

Reconfirmed against pushed commit 61c7781; its pre-commit full ./tools/verify.sh pass exercised the identical tree, including all existing discriminators and the corrected 6785-frame offscreen silent M0011_00_02 continuation.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the Silver Key doorway changes, including all asset/parser negative controls, gameplay gates, selftests, 993-room census, cmd API, world map, and the continuous offscreen/no-audio progression through M0013_03_01.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 after removing unused route branches; all parser negatives, gameplay gates, selftests, 993-room census, command/world-map checks, and continuous offscreen/no-audio progression through M0013_03_01 passed.
