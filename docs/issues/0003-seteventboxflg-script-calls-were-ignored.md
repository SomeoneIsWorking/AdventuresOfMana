---
id: 3
title: SetEventBoxFlg script calls were ignored
status: resolved
symptom: Nine shipping event-box flag mutations had no effect in the port
tags: gameplay,event-box,scripting
created: 2026-08-13
updated: 2026-08-13
---

## Root cause


## What was tried / dead ends


## Resolution

### Resolution (2026-08-13)
The command was absent from the script bridge. Engine SetEventBoxFlg at 0x2c83a0 ORs the mask when its boolean is true and ANDs with its inverse when false. The port now retains flags with that behavior; consumers for wall/push-switch flags remain unreversed.
