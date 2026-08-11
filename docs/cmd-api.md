| # | Lua name | native implementation | returns |
|---|----------|-----------------------|---------|
| 1 | `LogOut` | `LogOut(char const*)` | nothing |
| 2 | `GetGameTimeMs` | `GetGameTimeMs()` | number |
| 3 | `GetRealTimeMs` | `GetRealTimeMs()` | number |
| 4 | `GetBgmTimeMs` | `GetBgmTimeMs()` | number |
| 5 | `NewCoroutine` | `NewCoroutine(char*)` | boolean |
| 6 | `GetGameLanguage` | `GetGameLanguage()` | number |
| 7 | `AchievementUnlock` | `AchievementUnlock(char*)` | nothing |
| 8 | `SysSaveAccess` | `SysSaveAccess(char*)` | boolean |
| 9 | `math_atan2` | `math_atan2(float, float)` | number |
| 10 | `math_LerpN` | `math_LerpN(int, int, int, float, float)` | number |
| 11 | `math_LerpH` | `math_LerpH(int, int, int, float, float)` | number |
| 12 | `math_LerpL` | `math_LerpL(int, int, int, float, float)` | number |
| 13 | `math_LerpSin` | `math_LerpSin(int, int, int, float, int, int)` | number |
| 14 | `bit_and` | `bit_and(unsigned int, unsigned int)` | number |
| 15 | `bit_or` | `bit_or(unsigned int, unsigned int)` | number |
| 16 | `bit_xor` | `bit_xor(unsigned int, unsigned int)` | number |
| 17 | `bit_not` | `bit_not(unsigned int)` | number |
| 18 | `bit_lshift` | `bit_lshift(unsigned int, int)` | number |
| 19 | `bit_rshift` | `bit_rshift(unsigned int, int)` | number |
| 20 | `GetDevInfo` | `GetDevInfo(int)` | number |
| 21 | `GamePause` | `GamePause(bool)` | nothing |
| 22 | `SetMessageWnd` | `SetMessageWnd(char const*)` | nothing |
| 23 | `SetMessageWndIcon` | `SetMessageWndIcon(int)` | nothing |
| 24 | `SetItemStackWnd` | `SetItemStackWnd(int)` | nothing |
| 25 | `SetInfoWnd` | `SetInfoWnd(char const*, int)` | nothing |
| 26 | `SetNameWnd` | `SetNameWnd(char const*)` | nothing |
| 27 | `GetIDString` | `GetIDString(char const*)` | string |
| 28 | `GetIDStringCtrl` | `GetIDStringCtrl(char const*)` | string |
| 29 | `SetMessageWndPrmString` | `SetMessageWndPrmString(int, char const*)` | nothing |
| 30 | `SelectInit` | `SelectInit()` | nothing |
| 31 | `SelectAdd` | `SelectAdd(char const*)` | nothing |
| 32 | `Select` | `Select(int, int)` | number |
| 33 | `InputString` | `InputString(char*, char*, char*)` | nothing |
| 34 | `ShopInit` | `ShopInit()` | nothing |
| 35 | `ShopAdd` | `ShopAdd(int)` | nothing |
| 36 | `Shop` | `Shop(int)` | number |
| 37 | `SetRoomOut` | `SetRoomOut(bool)` | nothing |
| 38 | `GetRoomNo` | `GetRoomNo()` | number |
| 39 | `GetRoomX` | `GetRoomX()` | number |
| 40 | `GetRoomY` | `GetRoomY()` | number |
| 41 | `MapInfoOneShopCancel` | `MapInfoOneShopCancel()` | nothing |
| 42 | `SetRoomCover` | `SetRoomCover(int)` | nothing |
| 43 | `SetRoomInfo` | `SetRoomInfo(int, float)` | nothing |
| 44 | `GetRoomInfo` | `GetRoomInfo(int)` | number |
| 45 | `ItemName` | `ItemName(int)` | string |
| 46 | `ItemArticleName` | `ItemArticleName(int)` | string |
| 47 | `ItemPriceBuy` | `ItemPriceBuy(int)` | number |
| 48 | `ItemPriceSell` | `ItemPriceSell(int, int)` | number |
| 49 | `MinimapGetFlag` | `MinimapGetFlag(int, int, int)` | number |
| 50 | `MinimapSetFlag` | `MinimapSetFlag(int, int, int, int, bool)` | nothing |
| 51 | `GetGroundAttribute` | `GetGroundAttribute(float, float)` | number |
| 52 | `PaintGroundAttribute` | `PaintGroundAttribute(int, int, int, int, int, int)` | nothing |
| 53 | `AddEventBox` | `AddEventBox(char const*, float, float, float, float, float, float, int)` | nothing |
| 54 | `SetEventBoxEnable` | `SetEventBoxEnable(char const*, bool)` | nothing |
| 55 | `SetEventBoxNoTouchEvent` | `SetEventBoxNoTouchEvent(char const*)` | nothing |
| 56 | `SetEventBoxFlg` | `SetEventBoxFlg(char const*, int, bool)` | nothing |
| 57 | `SetFadeColor` | `SetFadeColor(int, int, int)` | nothing |
| 58 | `SetFade` | `SetFade(int, int)` | nothing |
| 59 | `IsFadeFinish` | `IsFadeFinish()` | boolean |
| 60 | `CaptureCrossFade` | `CaptureCrossFade(int)` | nothing |
| 61 | `SetPlayerControllEnable` | `SetPlayerControllEnable(bool)` | nothing |
| 62 | `MapJump` | `MapJump(int, int, int, float, float, float, int)` | nothing |
| 63 | `MapMaterialReverse` | `MapMaterialReverse(char*, bool)` | nothing |
| 64 | `AddNPC` | `AddNPC(char*, int, float, float, float, float)` | nothing |
| 65 | `AddNPCSubType` | `AddNPCSubType(char*, int, int, float, float, float, float)` | nothing |
| 66 | `DelNPC` | `DelNPC(char*)` | nothing |
| 67 | `AddEnemy` | `AddEnemy(int, float, float, float)` | nothing |
| 68 | `AddEnemyZaco` | `AddEnemyZaco(int, int, int, int, int, int)` | nothing |
| 69 | `AddParty` | `AddParty(int, float, float, float)` | nothing |
| 70 | `GetParty` | `GetParty()` | number |
| 71 | `AddBoss` | `AddBoss(int, float, float, float)` | nothing |
| 72 | `DeadEnemy` | `DeadEnemy(char*)` | nothing |
| 73 | `AddBox` | `AddBox(float, float, float, int)` | nothing |
| 74 | `BgmPlay` | `BgmPlay(int, int)` | nothing |
| 75 | `GetBgmID` | `GetBgmID()` | number |
| 76 | `SePlay` | `SePlay(int)` | nothing |
| 77 | `SePlayLoop` | `SePlayLoop(int)` | nothing |
| 78 | `SeStop` | `SeStop(int)` | nothing |
| 79 | `SeStopAll` | `SeStopAll()` | nothing |
| 80 | `IsSePlayLoop` | `IsSePlayLoop(int)` | boolean |
| 81 | `CamReset` | `CamReset()` | nothing |
| 82 | `CamSetData` | `CamSetData(int, float)` | nothing |
| 83 | `CamGetData` | `CamGetData(int)` | number |
| 84 | `CamSetTargetChr` | `CamSetTargetChr(char*)` | nothing |
| 85 | `CamSetTargetPos` | `CamSetTargetPos(float, float, float)` | nothing |
| 86 | `CamSetTargetPosSub` | `CamSetTargetPosSub(float, float, float)` | nothing |
| 87 | `CamSetTargetSubChr` | `CamSetTargetSubChr(char*, float, float)` | nothing |
| 88 | `CamAutoMove` | `CamAutoMove(int, int)` | nothing |
| 89 | `CamIsAutoMove` | `CamIsAutoMove()` | boolean |
| 90 | `CamSetPosLock` | `CamSetPosLock(bool)` | nothing |
| 91 | `CamSetPos` | `CamSetPos(float, float, float)` | nothing |
| 92 | `ChgPlayerType` | `ChgPlayerType(int)` | nothing |
| 93 | `GetPlayerType` | `GetPlayerType()` | number |
| 94 | `IsAddItem` | `IsAddItem(int)` | boolean |
| 95 | `AddItem` | `AddItem(int)` | boolean |
| 96 | `DelItem` | `DelItem(int)` | boolean |
| 97 | `DelItemGetCnt` | `DelItemGetCnt(int)` | boolean |
| 98 | `GetRC` | `GetRC()` | number |
| 99 | `AddRC` | `AddRC(int)` | boolean |
| 100 | `AddExp` | `AddExp(int)` | boolean |
| 101 | `GetEquipID` | `GetEquipID(int)` | number |
| 102 | `GetPaladinFlg` | `GetPaladinFlg()` | number |
| 103 | `SetPaladinFlg` | `SetPaladinFlg(int)` | nothing |
| 104 | `SetCheckLevelup` | `SetCheckLevelup(bool)` | nothing |
| 105 | `SetEfficacy` | `SetEfficacy(int, bool)` | nothing |
| 106 | `IsEfficacy` | `IsEfficacy(int)` | boolean |
| 107 | `ChrStandAdd` | `ChrStandAdd(char*, int)` | nothing |
| 108 | `ChrStandAddSubType` | `ChrStandAddSubType(char*, int, int)` | nothing |
| 109 | `ChrStandPos` | `ChrStandPos(char*, int)` | nothing |
| 110 | `ChrStandScale` | `ChrStandScale(char*, float)` | nothing |
| 111 | `ChrStandColor` | `ChrStandColor(char*, unsigned int)` | nothing |
| 112 | `ChrStandMotion` | `ChrStandMotion(char*, int, bool)` | nothing |
| 113 | `ChrStandDel` | `ChrStandDel(char*)` | nothing |
| 114 | `ChrGetData` | `ChrGetData(char*, int)` | number |
| 115 | `ChrSetData` | `ChrSetData(char*, int, float)` | nothing |
| 116 | `ChrSetPos` | `ChrSetPos(char*, float, float, float)` | nothing |
| 117 | `ChrGetLocalPosX` | `ChrGetLocalPosX(char*, int)` | number |
| 118 | `ChrGetLocalPosY` | `ChrGetLocalPosY(char*, int)` | number |
| 119 | `ChrGetLocalPosZ` | `ChrGetLocalPosZ(char*, int)` | number |
| 120 | `ChrBoneGetLocalPosX` | `ChrBoneGetLocalPosX(char*, char*)` | number |
| 121 | `ChrBoneGetLocalPosY` | `ChrBoneGetLocalPosY(char*, char*)` | number |
| 122 | `ChrBoneGetLocalPosZ` | `ChrBoneGetLocalPosZ(char*, char*)` | number |
| 123 | `ChrMotion` | `ChrMotion(char*, int)` | nothing |
| 124 | `ChrMotionForce` | `ChrMotionForce(char*, int)` | nothing |
| 125 | `ChrMotionGetID` | `ChrMotionGetID(char*)` | number |
| 126 | `IsChrMotionFinish` | `IsChrMotionFinish(char*)` | boolean |
| 127 | `ChrMotionGetFrame` | `ChrMotionGetFrame(char*)` | number |
| 128 | `ChrMotionGetEndFrame` | `ChrMotionGetEndFrame(char*)` | number |
| 129 | `ChrMoveUse` | `ChrMoveUse(char*, bool)` | nothing |
| 130 | `ChrLookTargetOff` | `ChrLookTargetOff(char*)` | nothing |
| 131 | `ChrLookTarget` | `ChrLookTarget(char*, char*)` | nothing |
| 132 | `ChrLookFixDegOff` | `ChrLookFixDegOff(char*)` | nothing |
| 133 | `ChrLookFixDeg` | `ChrLookFixDeg(char*, float)` | nothing |
| 134 | `ChrLookAutoAheadOff` | `ChrLookAutoAheadOff(char*)` | nothing |
| 135 | `ChrLookAutoAhead` | `ChrLookAutoAhead(char*, float)` | nothing |
| 136 | `ChrMoveTo` | `ChrMoveTo(char*, float, float, float)` | nothing |
| 137 | `ChrMoveYTo` | `ChrMoveYTo(char*, float, float, float, float)` | nothing |
| 138 | `IsChrAutoMove` | `IsChrAutoMove(char*)` | boolean |
| 139 | `ChrIsAlive` | `ChrIsAlive(char*)` | boolean |
| 140 | `ChrColorA` | `ChrColorA(char*, int, int)` | nothing |
| 141 | `ChrColorRGB` | `ChrColorRGB(char*, int, int, int, int)` | nothing |
| 142 | `ChrAccessoryBone` | `ChrAccessoryBone(char*, char*)` | nothing |
| 143 | `ChrAttackBoneSet` | `ChrAttackBoneSet(char*, int, char*)` | nothing |
| 144 | `ChrAttackBoneSize` | `ChrAttackBoneSize(char*, int, float, float, float, int, int, float, float, float)` | nothing |
| 145 | `ChrAttackBoneAttackRate` | `ChrAttackBoneAttackRate(char*, int, float)` | nothing |
| 146 | `ChrAttackBoneSE` | `ChrAttackBoneSE(char*, int, int)` | nothing |
| 147 | `ChrAttackBoneValid` | `ChrAttackBoneValid(char*, int, bool)` | nothing |
| 148 | `ChrDamageBoneSet` | `ChrDamageBoneSet(char*, int, char*)` | nothing |
| 149 | `ChrDamageBoneSize` | `ChrDamageBoneSize(char*, int, float)` | nothing |
| 150 | `ChrDamageBoneSubPos` | `ChrDamageBoneSubPos(char*, int, float, float, float)` | nothing |
| 151 | `ChrDamageBoneValid` | `ChrDamageBoneValid(char*, int, bool)` | nothing |
| 152 | `ChrToGround` | `ChrToGround(char*)` | nothing |
| 153 | `ChrMotionCmdStopNext` | `ChrMotionCmdStopNext(char*)` | nothing |
| 154 | `ChrThrowWeapon` | `ChrThrowWeapon(char*, char*)` | nothing |
| 155 | `WepGetData` | `WepGetData(char*, int)` | number |
| 156 | `WepSetData` | `WepSetData(char*, int, float)` | nothing |
| 157 | `WepSetPos` | `WepSetPos(char*, float, float, float)` | nothing |
| 158 | `WepGetLocalPosX` | `WepGetLocalPosX(char*, int)` | number |
| 159 | `WepGetLocalPosY` | `WepGetLocalPosY(char*, int)` | number |
| 160 | `WepGetLocalPosZ` | `WepGetLocalPosZ(char*, int)` | number |
| 161 | `WepDel` | `WepDel(char*)` | nothing |
| 162 | `WepIsHit` | `WepIsHit(char*)` | boolean |
| 163 | `WepHitIsShield` | `WepHitIsShield(char*)` | boolean |
| 164 | `WepHitTarget` | `WepHitTarget(char*)` | string |
| 165 | `WepHitReset` | `WepHitReset(char*)` | nothing |
| 166 | `WepIsAlive` | `WepIsAlive(char*)` | boolean |
| 167 | `WepMotion` | `WepMotion(char*, int)` | nothing |
| 168 | `WepAllDead` | `WepAllDead()` | nothing |
| 169 | `OpenDoor` | `OpenDoor(int)` | nothing |
| 170 | `SetDoor` | `SetDoor(int, int)` | nothing |
| 171 | `SetDoorForce` | `SetDoorForce(int, int)` | nothing |
| 172 | `ObjMotion` | `ObjMotion(int, int)` | nothing |
| 173 | `ObjMotionGetID` | `ObjMotionGetID(int)` | number |
| 174 | `ObjVisible` | `ObjVisible(int, bool)` | nothing |
| 175 | `ObjIsVisible` | `ObjIsVisible(int)` | boolean |
| 176 | `ObjAddTransBox` | `ObjAddTransBox(int, int, float, float, float, float, float, float)` | nothing |
| 177 | `ObjSetCollisionFlg` | `ObjSetCollisionFlg(int, int, bool)` | nothing |
| 178 | `ObjSetPmFlg` | `ObjSetPmFlg(int, int, bool)` | nothing |
| 179 | `ObjSetViewSubPos` | `ObjSetViewSubPos(int, float, float, float)` | nothing |
| 180 | `ObjSetActionSE` | `ObjSetActionSE(int, int)` | nothing |
| 181 | `IsInsideRoom` | `IsInsideRoom(float, float)` | boolean |
| 182 | `ParticleEmitterScale` | `ParticleEmitterScale(int, float)` | nothing |
| 183 | `ParticleEmitterRotate` | `ParticleEmitterRotate(int, float)` | nothing |
| 184 | `ParticleEmitterRotateX` | `ParticleEmitterRotateX(int, float)` | nothing |
| 185 | `ParticleEmitterRotateZ` | `ParticleEmitterRotateZ(int, float)` | nothing |
| 186 | `ParticleEmitterColorDirect` | `ParticleEmitterColorDirect(int, float, float, float, float)` | nothing |
| 187 | `ParticleEmitterColorAmbient` | `ParticleEmitterColorAmbient(int, float, float, float, float)` | nothing |
| 188 | `ParticleEmitterChrAttach` | `ParticleEmitterChrAttach(char*)` | nothing |
| 189 | `ParticleEmitterAdd` | `ParticleEmitterAdd(char*, int, int, float, float, float)` | nothing |
| 190 | `ParticleEmitterAddSnow` | `ParticleEmitterAddSnow(char*, float, float, float, float)` | nothing |
| 191 | `ParticleEmitterDelType` | `ParticleEmitterDelType(int, int)` | nothing |
| 192 | `ParticleEmitterDelName` | `ParticleEmitterDelName(char*)` | nothing |
| 193 | `RideOnChocobo` | `RideOnChocobo()` | nothing |
| 194 | `RideOffChocobo` | `RideOffChocobo()` | boolean |
| 195 | `SetCinema` | `SetCinema(bool)` | nothing |
| 196 | `SetStill` | `SetStill(int, int)` | nothing |
| 197 | `Ending` | `Ending(int)` | nothing |
| 198 | `StaffRollMsg` | `StaffRollMsg(char*)` | nothing |
| 199 | `StaffRollEnable` | `StaffRollEnable(bool)` | nothing |
| 200 | `StaffRollPos` | `StaffRollPos(int, int)` | nothing |
