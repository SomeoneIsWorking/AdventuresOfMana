| # | Lua name | native implementation |
|---|----------|-----------------------|
| 1 | `LogOut` | `LogOut(char const*)` |
| 2 | `GetGameTimeMs` | `GetGameTimeMs()` |
| 3 | `GetRealTimeMs` | `GetRealTimeMs()` |
| 4 | `GetBgmTimeMs` | `GetBgmTimeMs()` |
| 5 | `NewCoroutine` | `NewCoroutine(char*)` |
| 6 | `GetGameLanguage` | `GetGameLanguage()` |
| 7 | `AchievementUnlock` | `AchievementUnlock(char*)` |
| 8 | `SysSaveAccess` | `SysSaveAccess(char*)` |
| 9 | `math_atan2` | `math_atan2(float, float)` |
| 10 | `math_LerpN` | `math_LerpN(int, int, int, float, float)` |
| 11 | `math_LerpH` | `math_LerpH(int, int, int, float, float)` |
| 12 | `math_LerpL` | `math_LerpL(int, int, int, float, float)` |
| 13 | `math_LerpSin` | `math_LerpSin(int, int, int, float, int, int)` |
| 14 | `bit_and` | `bit_and(unsigned int, unsigned int)` |
| 15 | `bit_or` | `bit_or(unsigned int, unsigned int)` |
| 16 | `bit_xor` | `bit_xor(unsigned int, unsigned int)` |
| 17 | `bit_not` | `bit_not(unsigned int)` |
| 18 | `bit_lshift` | `bit_lshift(unsigned int, int)` |
| 19 | `bit_rshift` | `bit_rshift(unsigned int, int)` |
| 20 | `GetDevInfo` | `GetDevInfo(int)` |
| 21 | `GamePause` | `GamePause(bool)` |
| 22 | `SetMessageWnd` | `SetMessageWnd(char const*)` |
| 23 | `SetMessageWndIcon` | `SetMessageWndIcon(int)` |
| 24 | `SetItemStackWnd` | `SetItemStackWnd(int)` |
| 25 | `SetInfoWnd` | `SetInfoWnd(char const*, int)` |
| 26 | `SetNameWnd` | `SetNameWnd(char const*)` |
| 27 | `GetIDString` | `GetIDString(char const*)` |
| 28 | `GetIDStringCtrl` | `GetIDStringCtrl(char const*)` |
| 29 | `SetMessageWndPrmString` | `SetMessageWndPrmString(int, char const*)` |
| 30 | `SelectInit` | `SelectInit()` |
| 31 | `SelectAdd` | `SelectAdd(char const*)` |
| 32 | `Select` | `Select(int, int)` |
| 33 | `InputString` | `InputString(char*, char*, char*)` |
| 34 | `ShopInit` | `ShopInit()` |
| 35 | `ShopAdd` | `ShopAdd(int)` |
| 36 | `Shop` | `Shop(int)` |
| 37 | `SetRoomOut` | `SetRoomOut(bool)` |
| 38 | `GetRoomNo` | `GetRoomNo()` |
| 39 | `GetRoomX` | `GetRoomX()` |
| 40 | `GetRoomY` | `GetRoomY()` |
| 41 | `MapInfoOneShopCancel` | `MapInfoOneShopCancel()` |
| 42 | `SetRoomCover` | `SetRoomCover(int)` |
| 43 | `SetRoomInfo` | `SetRoomInfo(int, float)` |
| 44 | `GetRoomInfo` | `GetRoomInfo(int)` |
| 45 | `ItemName` | `ItemName(int)` |
| 46 | `ItemArticleName` | `ItemArticleName(int)` |
| 47 | `ItemPriceBuy` | `ItemPriceBuy(int)` |
| 48 | `ItemPriceSell` | `ItemPriceSell(int, int)` |
| 49 | `MinimapGetFlag` | `MinimapGetFlag(int, int, int)` |
| 50 | `MinimapSetFlag` | `MinimapSetFlag(int, int, int, int, bool)` |
| 51 | `GetGroundAttribute` | `GetGroundAttribute(float, float)` |
| 52 | `PaintGroundAttribute` | `PaintGroundAttribute(int, int, int, int, int, int)` |
| 53 | `AddEventBox` | `AddEventBox(char const*, float, float, float, float, float, float, int)` |
| 54 | `SetEventBoxEnable` | `SetEventBoxEnable(char const*, bool)` |
| 55 | `SetEventBoxNoTouchEvent` | `SetEventBoxNoTouchEvent(char const*)` |
| 56 | `SetEventBoxFlg` | `SetEventBoxFlg(char const*, int, bool)` |
| 57 | `SetFadeColor` | `SetFadeColor(int, int, int)` |
| 58 | `SetFade` | `SetFade(int, int)` |
| 59 | `IsFadeFinish` | `IsFadeFinish()` |
| 60 | `CaptureCrossFade` | `CaptureCrossFade(int)` |
| 61 | `SetPlayerControllEnable` | `SetPlayerControllEnable(bool)` |
| 62 | `MapJump` | `MapJump(int, int, int, float, float, float, int)` |
| 63 | `MapMaterialReverse` | `MapMaterialReverse(char*, bool)` |
| 64 | `AddNPC` | `AddNPC(char*, int, float, float, float, float)` |
| 65 | `AddNPCSubType` | `AddNPCSubType(char*, int, int, float, float, float, float)` |
| 66 | `DelNPC` | `DelNPC(char*)` |
| 67 | `AddEnemy` | `AddEnemy(int, float, float, float)` |
| 68 | `AddEnemyZaco` | `AddEnemyZaco(int, int, int, int, int, int)` |
| 69 | `AddParty` | `AddParty(int, float, float, float)` |
| 70 | `GetParty` | `GetParty()` |
| 71 | `AddBoss` | `AddBoss(int, float, float, float)` |
| 72 | `DeadEnemy` | `DeadEnemy(char*)` |
| 73 | `AddBox` | `AddBox(float, float, float, int)` |
| 74 | `BgmPlay` | `BgmPlay(int, int)` |
| 75 | `GetBgmID` | `GetBgmID()` |
| 76 | `SePlay` | `SePlay(int)` |
| 77 | `SePlayLoop` | `SePlayLoop(int)` |
| 78 | `SeStop` | `SeStop(int)` |
| 79 | `SeStopAll` | `SeStopAll()` |
| 80 | `IsSePlayLoop` | `IsSePlayLoop(int)` |
| 81 | `CamReset` | `CamReset()` |
| 82 | `CamSetData` | `CamSetData(int, float)` |
| 83 | `CamGetData` | `CamGetData(int)` |
| 84 | `CamSetTargetChr` | `CamSetTargetChr(char*)` |
| 85 | `CamSetTargetPos` | `CamSetTargetPos(float, float, float)` |
| 86 | `CamSetTargetPosSub` | `CamSetTargetPosSub(float, float, float)` |
| 87 | `CamSetTargetSubChr` | `CamSetTargetSubChr(char*, float, float)` |
| 88 | `CamAutoMove` | `CamAutoMove(int, int)` |
| 89 | `CamIsAutoMove` | `CamIsAutoMove()` |
| 90 | `CamSetPosLock` | `CamSetPosLock(bool)` |
| 91 | `CamSetPos` | `CamSetPos(float, float, float)` |
| 92 | `ChgPlayerType` | `ChgPlayerType(int)` |
| 93 | `GetPlayerType` | `GetPlayerType()` |
| 94 | `IsAddItem` | `IsAddItem(int)` |
| 95 | `AddItem` | `AddItem(int)` |
| 96 | `DelItem` | `DelItem(int)` |
| 97 | `DelItemGetCnt` | `DelItemGetCnt(int)` |
| 98 | `GetRC` | `GetRC()` |
| 99 | `AddRC` | `AddRC(int)` |
| 100 | `AddExp` | `AddExp(int)` |
| 101 | `GetEquipID` | `GetEquipID(int)` |
| 102 | `GetPaladinFlg` | `GetPaladinFlg()` |
| 103 | `SetPaladinFlg` | `SetPaladinFlg(int)` |
| 104 | `SetCheckLevelup` | `SetCheckLevelup(bool)` |
| 105 | `SetEfficacy` | `SetEfficacy(int, bool)` |
| 106 | `IsEfficacy` | `IsEfficacy(int)` |
| 107 | `ChrStandAdd` | `ChrStandAdd(char*, int)` |
| 108 | `ChrStandAddSubType` | `ChrStandAddSubType(char*, int, int)` |
| 109 | `ChrStandPos` | `ChrStandPos(char*, int)` |
| 110 | `ChrStandScale` | `ChrStandScale(char*, float)` |
| 111 | `ChrStandColor` | `ChrStandColor(char*, unsigned int)` |
| 112 | `ChrStandMotion` | `ChrStandMotion(char*, int, bool)` |
| 113 | `ChrStandDel` | `ChrStandDel(char*)` |
| 114 | `ChrGetData` | `ChrGetData(char*, int)` |
| 115 | `ChrSetData` | `ChrSetData(char*, int, float)` |
| 116 | `ChrSetPos` | `ChrSetPos(char*, float, float, float)` |
| 117 | `ChrGetLocalPosX` | `ChrGetLocalPosX(char*, int)` |
| 118 | `ChrGetLocalPosY` | `ChrGetLocalPosY(char*, int)` |
| 119 | `ChrGetLocalPosZ` | `ChrGetLocalPosZ(char*, int)` |
| 120 | `ChrBoneGetLocalPosX` | `ChrBoneGetLocalPosX(char*, char*)` |
| 121 | `ChrBoneGetLocalPosY` | `ChrBoneGetLocalPosY(char*, char*)` |
| 122 | `ChrBoneGetLocalPosZ` | `ChrBoneGetLocalPosZ(char*, char*)` |
| 123 | `ChrMotion` | `ChrMotion(char*, int)` |
| 124 | `ChrMotionForce` | `ChrMotionForce(char*, int)` |
| 125 | `ChrMotionGetID` | `ChrMotionGetID(char*)` |
| 126 | `IsChrMotionFinish` | `IsChrMotionFinish(char*)` |
| 127 | `ChrMotionGetFrame` | `ChrMotionGetFrame(char*)` |
| 128 | `ChrMotionGetEndFrame` | `ChrMotionGetEndFrame(char*)` |
| 129 | `ChrMoveUse` | `ChrMoveUse(char*, bool)` |
| 130 | `ChrLookTargetOff` | `ChrLookTargetOff(char*)` |
| 131 | `ChrLookTarget` | `ChrLookTarget(char*, char*)` |
| 132 | `ChrLookFixDegOff` | `ChrLookFixDegOff(char*)` |
| 133 | `ChrLookFixDeg` | `ChrLookFixDeg(char*, float)` |
| 134 | `ChrLookAutoAheadOff` | `ChrLookAutoAheadOff(char*)` |
| 135 | `ChrLookAutoAhead` | `ChrLookAutoAhead(char*, float)` |
| 136 | `ChrMoveTo` | `ChrMoveTo(char*, float, float, float)` |
| 137 | `ChrMoveYTo` | `ChrMoveYTo(char*, float, float, float, float)` |
| 138 | `IsChrAutoMove` | `IsChrAutoMove(char*)` |
| 139 | `ChrIsAlive` | `ChrIsAlive(char*)` |
| 140 | `ChrColorA` | `ChrColorA(char*, int, int)` |
| 141 | `ChrColorRGB` | `ChrColorRGB(char*, int, int, int, int)` |
| 142 | `ChrAccessoryBone` | `ChrAccessoryBone(char*, char*)` |
| 143 | `ChrAttackBoneSet` | `ChrAttackBoneSet(char*, int, char*)` |
| 144 | `ChrAttackBoneSize` | `ChrAttackBoneSize(char*, int, float, float, float, int, int, float, float, float)` |
| 145 | `ChrAttackBoneAttackRate` | `ChrAttackBoneAttackRate(char*, int, float)` |
| 146 | `ChrAttackBoneSE` | `ChrAttackBoneSE(char*, int, int)` |
| 147 | `ChrAttackBoneValid` | `ChrAttackBoneValid(char*, int, bool)` |
| 148 | `ChrDamageBoneSet` | `ChrDamageBoneSet(char*, int, char*)` |
| 149 | `ChrDamageBoneSize` | `ChrDamageBoneSize(char*, int, float)` |
| 150 | `ChrDamageBoneSubPos` | `ChrDamageBoneSubPos(char*, int, float, float, float)` |
| 151 | `ChrDamageBoneValid` | `ChrDamageBoneValid(char*, int, bool)` |
| 152 | `ChrToGround` | `ChrToGround(char*)` |
| 153 | `ChrMotionCmdStopNext` | `ChrMotionCmdStopNext(char*)` |
| 154 | `ChrThrowWeapon` | `ChrThrowWeapon(char*, char*)` |
| 155 | `WepGetData` | `WepGetData(char*, int)` |
| 156 | `WepSetData` | `WepSetData(char*, int, float)` |
| 157 | `WepSetPos` | `WepSetPos(char*, float, float, float)` |
| 158 | `WepGetLocalPosX` | `WepGetLocalPosX(char*, int)` |
| 159 | `WepGetLocalPosY` | `WepGetLocalPosY(char*, int)` |
| 160 | `WepGetLocalPosZ` | `WepGetLocalPosZ(char*, int)` |
| 161 | `WepDel` | `WepDel(char*)` |
| 162 | `WepIsHit` | `WepIsHit(char*)` |
| 163 | `WepHitIsShield` | `WepHitIsShield(char*)` |
| 164 | `WepHitTarget` | `WepHitTarget(char*)` |
| 165 | `WepHitReset` | `WepHitReset(char*)` |
| 166 | `WepIsAlive` | `WepIsAlive(char*)` |
| 167 | `WepMotion` | `WepMotion(char*, int)` |
| 168 | `WepAllDead` | `WepAllDead()` |
| 169 | `OpenDoor` | `OpenDoor(int)` |
| 170 | `SetDoor` | `SetDoor(int, int)` |
| 171 | `SetDoorForce` | `SetDoorForce(int, int)` |
| 172 | `ObjMotion` | `ObjMotion(int, int)` |
| 173 | `ObjMotionGetID` | `ObjMotionGetID(int)` |
| 174 | `ObjVisible` | `ObjVisible(int, bool)` |
| 175 | `ObjIsVisible` | `ObjIsVisible(int)` |
| 176 | `ObjAddTransBox` | `ObjAddTransBox(int, int, float, float, float, float, float, float)` |
| 177 | `ObjSetCollisionFlg` | `ObjSetCollisionFlg(int, int, bool)` |
| 178 | `ObjSetPmFlg` | `ObjSetPmFlg(int, int, bool)` |
| 179 | `ObjSetViewSubPos` | `ObjSetViewSubPos(int, float, float, float)` |
| 180 | `ObjSetActionSE` | `ObjSetActionSE(int, int)` |
| 181 | `IsInsideRoom` | `IsInsideRoom(float, float)` |
| 182 | `ParticleEmitterScale` | `ParticleEmitterScale(int, float)` |
| 183 | `ParticleEmitterRotate` | `ParticleEmitterRotate(int, float)` |
| 184 | `ParticleEmitterRotateX` | `ParticleEmitterRotateX(int, float)` |
| 185 | `ParticleEmitterRotateZ` | `ParticleEmitterRotateZ(int, float)` |
| 186 | `ParticleEmitterColorDirect` | `ParticleEmitterColorDirect(int, float, float, float, float)` |
| 187 | `ParticleEmitterColorAmbient` | `ParticleEmitterColorAmbient(int, float, float, float, float)` |
| 188 | `ParticleEmitterChrAttach` | `ParticleEmitterChrAttach(char*)` |
| 189 | `ParticleEmitterAdd` | `ParticleEmitterAdd(char*, int, int, float, float, float)` |
| 190 | `ParticleEmitterAddSnow` | `ParticleEmitterAddSnow(char*, float, float, float, float)` |
| 191 | `ParticleEmitterDelType` | `ParticleEmitterDelType(int, int)` |
| 192 | `ParticleEmitterDelName` | `ParticleEmitterDelName(char*)` |
| 193 | `RideOnChocobo` | `RideOnChocobo()` |
| 194 | `RideOffChocobo` | `RideOffChocobo()` |
| 195 | `SetCinema` | `SetCinema(bool)` |
| 196 | `SetStill` | `SetStill(int, int)` |
| 197 | `Ending` | `Ending(int)` |
| 198 | `StaffRollMsg` | `StaffRollMsg(char*)` |
| 199 | `StaffRollEnable` | `StaffRollEnable(bool)` |
| 200 | `StaffRollPos` | `StaffRollPos(int, int)` |
