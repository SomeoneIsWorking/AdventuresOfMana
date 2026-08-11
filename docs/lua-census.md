# Lua API census — measured, not guessed

Produced by `./build/mana --script-census`, which runs **all 715 shipping Lua
scripts** against recording stubs for the 200-function `cmd` API, then invokes
every event handler each script defines.

    executed 714 map scripts, 0 failed
    invoked 1898 event handlers, 1017 raised errors (they need live game state)
    132 of 200 cmd functions exercised, 16011 total calls

This exists so engine work is ordered by evidence. Implementing the top 20
functions covers 68.7% of all observed calls; the 68 never-called functions can
wait indefinitely.

## Two harness bugs this surfaced

- **`SystemInit` must be called after loading `sk1.lua`.** Its own comment marks
  it startup-only; it establishes the scenario globals (`sccnt`, `scflagNN`).
  Without it 102 scripts died on `attempt to compare nil with number`.
- **Load-time calls alone undercount by 3x.** Only 44 functions are touched
  while scripts load; the real surface is in the event handlers, and firing
  those raises it to 132.

## Called, by frequency

| Calls | Function |
|------:|----------|
| 1368 | `WepSetData` |
| 814 | `GetGameTimeMs` |
| 803 | `ChrMoveTo` |
| 802 | `ChrGetLocalPosX` |
| 788 | `ChrIsAlive` |
| 781 | `ChrGetLocalPosZ` |
| 584 | `WepIsAlive` |
| 569 | `AddEventBox` |
| 530 | `ChrSetData` |
| 471 | `ChrGetLocalPosY` |
| 408 | `SetDoor` |
| 406 | `LogOut` |
| 363 | `ChrBoneGetLocalPosY` |
| 358 | `ChrBoneGetLocalPosX` |
| 357 | `ChrBoneGetLocalPosZ` |
| 333 | `SetPlayerControllEnable` |
| 332 | `SetFade` |
| 332 | `SetFadeColor` |
| 309 | `ChrGetData` |
| 287 | `WepDel` |
| 185 | `ChrLookTarget` |
| 182 | `GetPlayerType` |
| 182 | `ChrMotion` |
| 181 | `GetIDString` |
| 167 | `CamSetData` |
| 162 | `SetEventBoxNoTouchEvent` |
| 154 | `BgmPlay` |
| 146 | `SetMessageWnd` |
| 137 | `WepSetPos` |
| 136 | `ParticleEmitterAdd` |
| 132 | `IsInsideRoom` |
| 130 | `GetBgmID` |
| 127 | `AddNPC` |
| 122 | `ObjVisible` |
| 122 | `ChrAttackBoneValid` |
| 118 | `SePlay` |
| 106 | `ShopAdd` |
| 103 | `GetParty` |
| 102 | `math_atan2` |
| 99 | `NewCoroutine` |
| 99 | `SetEventBoxEnable` |
| 96 | `ObjMotion` |
| 87 | `ChrMoveUse` |
| 79 | `GetEquipID` |
| 71 | `GetRoomY` |
| 71 | `GetRoomX` |
| 68 | `SetEfficacy` |
| 66 | `ChrSetPos` |
| 65 | `ChrAttackBoneSet` |
| 65 | `ChrAttackBoneSize` |
| 63 | `ChrDamageBoneValid` |
| 52 | `SetRoomCover` |
| 50 | `ChrDamageBoneSet` |
| 50 | `WepAllDead` |
| 50 | `AddEnemy` |
| 50 | `ChrDamageBoneSize` |
| 48 | `AddEnemyZaco` |
| 44 | `WepGetLocalPosX` |
| 44 | `WepGetLocalPosZ` |
| 44 | `WepGetLocalPosY` |
| 42 | `MinimapSetFlag` |
| 36 | `GetRoomNo` |
| 35 | `ChrStandAdd` |
| 34 | `MapJump` |
| 34 | `CamGetData` |
| 30 | `GetRC` |
| 30 | `SetNameWnd` |
| 29 | `ChrStandColor` |
| 29 | `ChrStandPos` |
| 29 | `SetInfoWnd` |
| 29 | `CamAutoMove` |
| 28 | `CamSetTargetChr` |
| 27 | `CamSetPos` |
| 27 | `CamSetTargetPos` |
| 25 | `ChrMotionForce` |
| 24 | `ChrAttackBoneSE` |
| 24 | `ParticleEmitterScale` |
| 24 | `ChrStandMotion` |
| 22 | `ShopInit` |
| 22 | `AddBox` |
| 21 | `SetCinema` |
| 20 | `ObjAddTransBox` |
| 18 | `CamIsAutoMove` |
| 18 | `GamePause` |
| 18 | `SeStop` |
| 17 | `ObjSetCollisionFlg` |
| 17 | `ObjIsVisible` |
| 15 | `SetCheckLevelup` |
| 13 | `CamReset` |
| 13 | `OpenDoor` |
| 13 | `ChrColorA` |
| 13 | `ChrMoveYTo` |
| 12 | `ObjMotionGetID` |
| 12 | `ParticleEmitterRotate` |
| 12 | `ChrMotionGetFrame` |
| 9 | `CamSetTargetPosSub` |
| 9 | `SePlayLoop` |
| 9 | `SetEventBoxFlg` |
| 8 | `AchievementUnlock` |
| 8 | `ChrLookFixDegOff` |
| 7 | `AddBoss` |
| 7 | `SetMessageWndPrmString` |
| 6 | `ChrLookFixDeg` |
| 6 | `MapMaterialReverse` |
| 6 | `WepIsHit` |
| 6 | `WepHitReset` |
| 6 | `ChrStandDel` |
| 5 | `PaintGroundAttribute` |
| 5 | `ChrDamageBoneSubPos` |
| 4 | `ParticleEmitterDelType` |
| 4 | `math_LerpL` |
| 4 | `ChrAttackBoneAttackRate` |
| 4 | `ChrLookTargetOff` |
| 3 | `SetDoorForce` |
| 3 | `math_LerpSin` |
| 3 | `math_LerpH` |
| 3 | `GetGroundAttribute` |
| 3 | `IsChrMotionFinish` |
| 3 | `CamSetTargetSubChr` |
| 3 | `ParticleEmitterAddSnow` |
| 2 | `ChrColorRGB` |
| 2 | `bit_and` |
| 2 | `ObjSetViewSubPos` |
| 1 | `ChrThrowWeapon` |
| 1 | `ChrStandScale` |
| 1 | `AddParty` |
| 1 | `ChrMotionGetID` |
| 1 | `IsChrAutoMove` |
| 1 | `WepMotion` |
| 1 | `ChrLookAutoAhead` |
| 1 | `IsEfficacy` |
| 1 | `ObjSetPmFlg` |

## Never called by any shipping script (68)

`GetRealTimeMs`, `GetBgmTimeMs`, `GetGameLanguage`, `SysSaveAccess`, `math_LerpN`, `bit_or`, `bit_xor`, `bit_not`, `bit_lshift`, `bit_rshift`, `GetDevInfo`, `SetMessageWndIcon`, `SetItemStackWnd`, `GetIDStringCtrl`, `SelectInit`, `SelectAdd`, `Select`, `InputString`, `Shop`, `SetRoomOut`, `MapInfoOneShopCancel`, `SetRoomInfo`, `GetRoomInfo`, `ItemName`, `ItemArticleName`, `ItemPriceBuy`, `ItemPriceSell`, `MinimapGetFlag`, `IsFadeFinish`, `CaptureCrossFade`, `AddNPCSubType`, `DelNPC`, `DeadEnemy`, `SeStopAll`, `IsSePlayLoop`, `CamSetPosLock`, `ChgPlayerType`, `IsAddItem`, `AddItem`, `DelItem`, `DelItemGetCnt`, `AddRC`, `AddExp`, `GetPaladinFlg`, `SetPaladinFlg`, `ChrStandAddSubType`, `ChrMotionGetEndFrame`, `ChrLookAutoAheadOff`, `ChrAccessoryBone`, `ChrToGround`, `ChrMotionCmdStopNext`, `WepGetData`, `WepHitIsShield`, `WepHitTarget`, `ObjSetActionSE`, `ParticleEmitterRotateX`, `ParticleEmitterRotateZ`, `ParticleEmitterColorDirect`, `ParticleEmitterColorAmbient`, `ParticleEmitterChrAttach`, `ParticleEmitterDelName`, `RideOnChocobo`, `RideOffChocobo`, `SetStill`, `Ending`, `StaffRollMsg`, `StaffRollEnable`, `StaffRollPos`

Never-called does not mean dead: several are plainly driven by native code or by
menus rather than map scripts (`Ending`, `StaffRoll*`, `Shop*`, `InputString`,
the `bit_*` and `math_*` helpers). It means no *map script* reaches them, which
is exactly the ordering signal wanted.
