---
id: C021
kind: claim
status: holds
created: 2026-08-14
tags: progression,tooling,windowless,audio
depends: src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 10:18:31
---

## Claim

An unseeded new game continuously crosses the post-Matock cave, reaches Kett Manor, acquires Cure, and settles at scenario 15 windowlessly and silently

## Evidence

2026-08-14 full ./tools/verify.sh pass: mandatory continuation reached M0012_00_00 bed_01 in 9259 fixed-step uncapped frames, logged Cure acquisition and settled sccnt=15, SDL offscreen, and 0 decoded sounds/frames.

## What would falsify it

The mandatory gate reverses through M0000_09_06 in_2, fails to traverse Kett stairs and bed_01, does not settle at sccnt=15, creates a visible window, decodes audio, or any dependent navigation/event-box code changes without re-verification.

## Re-confirmed 2026-08-14

Full ./tools/verify.sh passed on 2026-08-14 after the combined change; mandatory run reached bed_01 and settled sccnt=15 in 9259 fixed-step uncapped offscreen frames with zero decoded audio, and every existing parser/self-test/negative/generation gate passed.

## Re-confirmed 2026-08-14

Final full ./tools/verify.sh passed on 2026-08-14 after removing unused exploratory branches; mandatory run still reached bed_01 and settled sccnt=15 in 9259 fixed-step uncapped offscreen frames with zero decoded audio, and all prior gates passed.

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

## Re-confirmed 2026-08-14

Full offscreen/dummy-audio fixed-step gate tools/verify.sh passed on 2026-08-14; scratch/logs/verify-keyring-structure-v4.log ends VERIFICATION OK.

## Re-confirmed 2026-08-14

Commit af6e3c5 passed the complete offscreen/dummy-audio fixed-step tools/verify.sh gate on 2026-08-14; gameplay and static/corpus evidence is recorded under scratch/logs/.
