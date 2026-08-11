# The game's data model, from its own scripts

`sk1.lua` defines the enums the map scripts use, and the original developers
annotated **every field in Japanese**. These are the game's own slot numbers, not
an interpretation:

| Enum | Members | Purpose |
|------|--------:|---------|
| `eChrGetData` | 48 | character slots for `ChrGetData`/`ChrSetData` |
| `eWepGetData` | 13 | weapon slots for `WepGetData`/`WepSetData` |
| `eCamGetData` | 16 | camera slots for `CamGetData`/`CamSetData` |
| `eMOTION` | 301 | motion IDs |
| `eDoor` | 5 | `SetDoor` kinds |
| `eArrow` | 4 | direction constants |

Examples, with the shipped comments:

    eChrGetData: HP=0, MAXHP=1, MP=2, MAXMP=3, SCALE=100,
      FLIGHT=101        -- 飛行フラグ。落ちないように
      MAPCOLLISION=102  -- マップコリジョンを判定するか(0:判定しない,1:判定する)
      FLOORTYPE=106     -- 現状の床タイプ 0:地面, 1:壁
    eWepGetData: VEC_X..Z=0..2, ACC_X..Z=3..5, LIFETIME=6 (frames),
      HITDEAD=8         -- 当たったら消える（0:貫通武器）
    eDoor: FREE=0 (体当たりで開く), KEY=1, CLOSE=2, BLOCK=3 (壊れる壁), WALL=4

## Motion IDs map to `.smot` filenames — by NUMBER, not name

`.smot` files are named `<model>_<NNN>_<LABEL>.smot`, and `NNN` is the `eMOTION`
index. Verified: **1584 of 1592** parseable files agree.

The disagreements are instructive. 8 files have a `NNN` that does not match their
`LABEL`'s index, and 129 use labels absent from `eMOTION` entirely (`BEND`,
`IVY_DEAD`). In every case the *number* is consistent with how scripts call
`ChrMotion` and the *label* is a per-model description. **Look motions up by
number and ignore the label**; matching on names would fail on 137 files and,
worse, would appear to work on the other 1584.
