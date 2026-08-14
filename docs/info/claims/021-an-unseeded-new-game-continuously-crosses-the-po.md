---
id: C021
kind: claim
status: holds
created: 2026-08-14
tags: progression,tooling,windowless,audio
depends: src/host/main.cpp, tools/verify.sh
reconfirmed: 2026-08-14
verified_at: 2026-08-14 05:29:40
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
