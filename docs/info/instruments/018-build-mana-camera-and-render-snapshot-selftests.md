---
id: I018
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

build/mana camera and render-snapshot selftests

## Validated by

Six Lua camera-command cases, seven backend-independent CameraTracker cases, and three RenderSnapshot cases all pass. The complete tools/verify.sh run in scratch/logs/verify-render-snapshot-final.log also rendered shipping capture paths through the snapshot, completed 21961 uncapped offscreen frames with 0 audio frames, and passed every parser.

## Known failure modes

(none recorded yet)
