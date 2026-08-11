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

## Actor type ids map to model names

Verified against the enums in `sk1.lua`:

| Enum | Spawner | Model | Coverage |
|------|---------|-------|----------|
| `eENEMY` | `AddEnemy` | `E<id>_00.smdl` | **74/74** |
| `eBOSS` | `AddBoss` | `B<id>_00.smdl` | **24/24** |
| `eNPC` | `AddNPC` | `N<id>_00.smdl` | 34/53 |

The 19 unmatched NPC ids have no `N<id>_00` model; several are aliases
(`WOLFMAN`/`WOLF_MAN`) and others likely reuse character (`C####`) models. The
host logs each unmatched actor by handle rather than dropping it silently.

## Room coordinates

Scripts give actor positions in **room-local** coordinates; map models are
authored in **world** space. Rooms tile a fixed **300 x 240** grid indexed by the
two numbers in the model name, `M<map>_<gx>_<gy>`, so

    room_origin = (gx * 300, 0, gy * 240)

For the `M0000_00_*` column, mesh `min.z` is exactly `gy * 240` for every room.

**The filename is the anchor, not the geometry.** Testing the grid against
measured bounds fails: only 295/993 map meshes have `min` on it (walls and
overhangs extend past the room), and only 330/992 collision AABBs do — the
dominant collision box is 330x270, i.e. a uniform 15-unit margin around the
300x240 room. Either measurement would have produced a plausible-looking
per-room offset that was actually geometry noise.

### Actor Y comes from collision, not from the script

The scripts' Y argument (e.g. `AddEnemy(id, x, 30, z)`) is **not** a world
offset — using it directly floated actors at wall height. Ground height is
queried from the room's `.scol` via `Collision::GetFloor`, which is what the
engine does (`GetGroundAttribute` -> `ModeGame::GetGroundAttribute` ->
`SiCollisionMesh::GetFloor`). For `M0000_03_06` that returns 0.00 at all four
spawns — the grass — against a script Y of 30.

## Camera

`eCamGetData` slots are driven directly by the map scripts. Values actually used:

| Slot | Values seen | Notes |
|------|-------------|-------|
| `ANGLE` (3) | 20 (x55), 15 (x13) | field of view, degrees |
| `DISTANCE` (4) | 450 (x32), 300 (x16), 380 (x9) | |
| `ROTATE_Y` (1) | -10, -40, -30, -20, 10, -50 | yaw |
| `ROTATE_X` (0) | 0 (x11) | |
| `SPEED` (8) | 0.1 | lerp rate; sk1.lua documents the default as 0.3 |
| `NEAR`/`FAR` (13/14) | — | sk1.lua documents defaults 40.0 / 5000.0 |

`M0012_01_01` sets three of them (angle 20.27, distance 168.9, rotY -60) and the
port's camera follows suit, which is how the wiring is verified.

### One value is a port choice, not reversed

The default **pitch**. `sk1.lua` states defaults for `NEAR`, `FAR` and `SPEED`
but not `ROTATE_X`, and scripts only ever set it to 0. The engine's own default
must live in `AppCameraGame`, which is not reversed. The port uses 38 degrees
because it looks right — flagged here so it is not mistaken for a measured value.

`SPEED` is a per-frame lerp in the original; the port scales it by delta time so
the result does not depend on frame rate.

## Event boxes and map transitions

`AddEventBox(name, x0,y0,z0, x1,y1,z1, flag)` is the most-called cmd function
(552 calls). Entering the volume makes the engine call the **global Lua function
of the same name**. That is how transitions, cutscenes and shops all start.

Boxes are **edge-triggered** — fire on entry, not every frame — or a transition
re-enters itself forever.

### Handlers are COROUTINES, not calls

This is the part that cannot be guessed from the signatures. A handler like:

    function in_01()
      CHOCOBO_BYE()
      bgmfield()
      mapjump(10, 2, 2, 150, 205, 0)
    end

reaches `mapjump` -> `mapjumpY` -> `fadeout()`, and `fadeout` runs

    SetFade(1,600)
    coroutine.yield()
    while IsFadeFinish() == false do coroutine.yield() end

so calling a handler with `lua_pcall` dies with *attempt to yield from outside a
coroutine*. The engine runs them as threads — which is why `NewCoroutine` is in
the cmd API and `GameScript::NewCoroutine` / `GameScript::Update` exist in the
binary. The host starts each handler with `lua_newthread` + `lua_resume` and
resumes live threads every frame.

### The fade must actually take time

`IsFadeFinish` could be stubbed to return true and transitions would "work" — but
that deletes the wait the transition is built around, and every fade would be
instant. `Fade` is a real timer ticked by the frame delta.

`MapJump(mapid, mapx, mapy, plx, ply, plz, arrow)` loads
`M<mapid>_<mapx>_<mapy>`, places the player at room-local `(plx, ply, plz)`
snapped to the collision floor, and faces them per `eArrow` (UP=0 RI=1 DN=2 LF=3).

## Combat volumes

Both attach to a named bone:

    ChrDamageBoneSet(chr, i, "r_hand"); ChrDamageBoneSize(chr, i, 15)
      -> a SPHERE of that radius

    ChrAttackBoneSet(chr, i, "c_spine"); ChrAttackBoneSize(chr, i, 35, 180, ...)
      -> an ARC: radius, then the fan angle in degrees (scripts use 35@180,
         50@360, 20@60)

`ChrAttackBoneValid` / `ChrDamageBoneValid` enable them; `ChrAttackBoneAttackRate`
scales damage. 21 scripts configure attack volumes, 18 configure damage volumes.

`HitArcSphere` is the overlap predicate, kept as pure geometry so it can be
exercised without a running game — `--combat-selftest` runs 9 cases covering
both a hit and a miss, the exact-radius boundary, arc inclusion and exclusion,
attacker rotation, and vertical separation. This matters because in the rooms
tested so far nothing happens to overlap, so a detector that never fires and one
that always fires would produce identical logs.
