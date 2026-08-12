# Extracted assets (`sk1.mpk` -> 9886 files, 956 MB)

Extract with `tools/asset/mpk.py sk1.mpk -o <outdir>`. All 9886 entries decode
and validate; 0 failures.

## Census

| Count | Ext | Magic | What |
|------:|-----|-------|------|
| 1721 | `.smot` | `Smot` | skeletal motion / animation |
| 1375 | `.smdl` | `Smd3` | model, format v3 |
| 1019 | `.dat`  | — | per-map data table |
| 994 | `.stexinfo` | u32 count + names | texture set index |
| 992 | `.scol` | `SCol` | collision mesh |
| 750 | `.stex` | `SMDI` | texture container |
| **715** | **`.lua`** | — | **plain-text game script** |
| 657 | `.gdt` | — | ground/geometry data |
| 424 | `.odt` | — | object data |
| 370 | `.seff` | `Seff` | particle / effect definition |
| 247 | `.edt` | — | event data |
| 186 | `.mtex` | raw w/h/fmt header | raw texture (e.g. 128x128, fmt 7) |
| 176 | `.wav` | — | sound effect |
| 153 | `.mcmd` | — | motion command track |
| 71 | `.bin` | — | misc (minimap tables, area-name strings) |
| 20 | `.png` | — | UI image |
| 8 | `.txb` | — | text bank |
| 6 | `.txt` | — | plain text |
| 1 | `.sfont` | — | bitmap font (`BasicFont.sfont`) |

## The Lua layer — 50,643 lines across 715 files

- **`sk1.lua`** is the prelude: 2961 lines, 138 functions. It defines the
  gameplay-level helpers the map scripts call (`mapjump`, `bgmfield`,
  `EvBoxOne`, `EvBoxOneY`, `EvBoxWallUp`, `CHOCOBO_PUT`, …) on top of the 200
  native `cmd` functions, plus `SystemInit()` and the global save/scenario flags.
- **`M<map>_<x>_<y>.lua`** are the per-room scripts: event boxes, NPC placement,
  map transitions, cutscene choreography.

### ENCODING TRAP

`sk1.lua` is **Shift-JIS**, while every map script is **UTF-8** — despite both
carrying a header comment that claims `UTF8(BOM無し)`:

    sk1.lua:         Non-ISO extended-ASCII text, with CRLF, NEL line terminators
    M0000_00_00.lua: Unicode text, UTF-8 text, with CRLF line terminators

This is not cosmetic. GNU grep classifies `sk1.lua` as *binary* and silently
returns no matches without `-a`, which made "where is `mapjump` defined?" answer
"nowhere" — a false negative that looked like a real result. Any tool that walks
these scripts must handle both encodings explicitly and must not treat a failed
decode as an empty file.

---

# `.stex` texture container (`SMDI`) — REVERSED

Layout derived from `SiSurfaceTextureArray::SetBinary`. Its loop reads 32-byte
descriptors and forwards them to `SiSurfaceTexture::Create(int w, int h, int
mips, SiTextureFormat, void const* pixels, unsigned size, SiTextureRepeat,
SiTextureRepeat, SiTextureFilter, SiDrawMemoryArea, char const* name)` — the 11
call arguments pin every field unambiguously.

## Header

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | char[4] | `"SMDI"` (written, **never validated** — the engine dispatches on `ResourceKind`, not magic) |
| 0x04 | u32 | unused by the loader |
| 0x08 | u32 | offset of descriptor table |
| 0x0C | u32 | texture count (loader returns early if < 1) |

## Descriptor — 32 bytes

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | u32 | unused by the loader |
| 0x04 | u32 | format code |
| 0x08 | u32 | width |
| 0x0C | u32 | height |
| 0x10 | u32 | mip levels |
| 0x14 | u32 | offset to pixel data |
| 0x18 | u32 | pixel data size |
| 0x1C | u32 | offset to NUL-terminated name |

Format code maps through a table at `.rodata:0x9dda0` to `SiTextureFormat`:
`2->3, 3->2, 4->4, 5->5`, anything else `->1`.

## Verification

`tools/asset/stex.py` parses **1319 descriptors across all 750 files, 0
failures**. Bytes-per-texel over the full mip chain comes out to exactly **4.0**,
i.e. every shipped texture is format code 0 -> `SiTextureFormat 1` = RGBA8888.
Mip 0 decodes to a correct image with no swizzle and no row flip — verified by
rendering `B0000_00_face.tga` (512x512, 9 mips) to PNG and looking at it.

---

# `.smdl` model container (`Smd3`) — REVERSED

Layout derived from `SiModelBase::SetBinary`. Its prologue loads `ldrsw` values
at 0x0C, 0x14, 0x1C … 0x64 — a table of `(u32 count, s32 offset)` section pairs
beginning at 0x08 with stride 8. Two sections are forwarded to typed
constructors, which pin the descriptor shape exactly:

    SiBufferVertex::Create(void const* data, unsigned stride, unsigned count)
    SiBufferIndex::Create (void const* data, unsigned size,   SiDrawIndicesType)

## Header

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | char[4] | `"Smd3"` (never validated) |
| 0x04 | u32 | header size, 32 |
| 0x08 + 8k | u32 | section *k* count |
| 0x0C + 8k | s32 | section *k* offset |

## Section slots

| k | Contents |
|---|----------|
| 1 | skeleton — count is the bone count, and it **cross-checks against `.smot` header 0x08** for the same character |
| 6 | vertex buffer |
| 7 | index buffer |
| 8 | string table (count = length in bytes) |

## Buffer descriptor — shared shape

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | u32 | element count |
| 0x04 | s32 | offset to data |
| 0x08 | u32 | element size — vertex stride, or bytes-per-index |

The index loader accepts **only 2 or 4** bytes per index; anything else skips the
buffer entirely.

## Vertex strides

| Stride | Count | Layout |
|--------|------:|--------|
| 24 | 1118 | pos(12) + uv(8) + color(4) — static geometry |
| 44 | 257 | pos(12) + normal(12) + uv(8) + color(4) + weight(4) + boneidx(4) — skinned |

Matches the skinning vertex shader in `.rodata`, which declares exactly
`position, texcoord0, color, weight, incidence`.

## Verification

`tools/asset/smdl.py` parses **1375/1375 models, 0 failures.** The format is
self-validating: data regions tile the file with no gaps, so

    vertex_offset + count*stride == index_offset
    index_offset  + count*size   == string_table_offset

and for `B0001_02.smdl` the string table ends exactly at EOF. Every index in
every model is also in range of that model's vertex count.

Structural consistency alone would not prove the positions are really positions,
so `tools/asset/render_smdl.py` software-rasterizes a model to PNG. `B0000_00`
(2433 verts, 3476 tris, 43 bones) renders as a coherent, correctly-wound boss
model — see `scratch/png/B0000_00_model.png`.

---

# `.smot` skeletal animation (`Smot`) — REVERSED

`SiModelMotion::SetBinary` copies the file verbatim and runs a single fixup pass,
so motion data is consumed in place. That pass still pins the spine: it reads a
track count at 0x08 and a table offset at 0x0C, walks 32-byte entries reading a
name offset at +0x14, and ORs bit 5 into flags at +0x00 for every track whose
name matches the wildcard `c_eye*`.

## Header

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | char[4] | `"Smot"` (never validated) |
| 0x08 | u32 | track count — **equals the `.smdl` bone count** for the same character |
| 0x0C | s32 | track table offset |
| 0x1C | f32 | duration in frames |
| 0x20 | u32 | total file size |
| 0x24 | s32 | offset of name block |

## Track entry — 32 bytes

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | u32 | channel flag bitmask (below) |
| 0x04 | u32 | key count |
| 0x08 | s32 | offset of key **value** array (`count * stride`) |
| 0x0C | u32 | value stride |
| 0x10 | s32 | offset of key **time** array (`count * 4`), immediately preceding the values |
| 0x14 | s32 | offset of NUL-terminated bone name |
| 0x18 | s32 | offset of a 48-byte per-track record (3x4 bind matrix) |
| 0x1C | u32 | unused in every shipped file |

### Channel flags

One 16-byte channel per set bit:

| Bit | Value | Channel | Adds |
|-----|-------|---------|------|
| 2 | 4 | rotation (quaternion) | 16 B |
| 1 | 2 | translation | 16 B |
| 3 | 8 | scale | 16 B |
| 4 | 16 | modifier — carries **no** data | 0 B |
| 5 | 32 | set at load time for `c_eye*` bones; never in a file | 0 B |

So `value_stride == 16 * popcount(flags & 0b1110)` — asserted, not assumed, and
it holds for **all 60,803 tracks in all 1721 motions**. Observed combinations:
`4, 6, 12, 14, 20, 22, 28`.

## Verification

`tools/asset/smot.py` parses **1721/1721 motions (60,803 tracks), 0 failures**,
enforcing that time and value arrays abut, consecutive tracks tile with no gaps,
the 48-byte records tile up to the name block, and `hdr[0x20]` equals the real
file size.

Structure alone would not prove the rotation channel holds quaternions, so that
is checked semantically: over 394,248 rotation samples, **99.985% are unit
length**, worst deviation 0.0079 (quantization). Bone names are plausible
throughout (`cog`, `c_hip`, `l_tibia`, `l_foot`, `c_tail_a`).

## OPEN: 98 non-monotonic time arrays

98 of 60,803 tracks (0.161%), across **9** files (prefixes `E` enemy, `O` object,
`W` weapon), have time arrays that restart rather than increase — e.g.
`O0020_03_001/joint3` has 12 keys with times `[1,10,30,70]` repeated three times.

This is **not** explained by per-channel sub-arrays: the tiling checks confirm the
value array is `count * stride` bytes, so all 12 keys are real 32-byte entries,
and 3 groups does not match that track's 2 channels either. The meaning is
genuinely unknown — possibly multi-segment takes.

It does not block anything (0.16% of tracks, all on non-player actors), but it is
recorded rather than smoothed over, because a playback bug here would otherwise
look like an animation blending problem rather than a parsing gap.

---

# `.scol` collision mesh (`SCol`) — REVERSED (loader-level)

`SiCollisionMesh::SetBinary` is only 39 instructions: it copies the file, then
reads a 12-byte-stride array at `[0x2C]` and pulls two entries indexed by `[0x10]`
and `[0x14]`. Those entries decode as float triples, and the pair it extracts is
the AABB — for `M0000_00_00` they are `(0, -15, 0)` and `(300, 150, 240)`, a
300x240 room.

## Header

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | char[4] | `"SCol"` (never validated) |
| 0x04 | u32 | header size, 20 |
| 0x08 | u32 | node count |
| 0x0C | s32 | node array offset |
| 0x10 | u32 | index of AABB **min** vec3 |
| 0x14 | u32 | index of AABB **max** vec3 |
| 0x18 | u32 | unknown (15..45, varies per map) |
| 0x1C | u32 | grid width — **80** in all 992 files |
| 0x20 | u32 | grid height — **50** in all 992 files |
| 0x24 | u32 | total file size |
| 0x28 | u32 | vec3 count |
| 0x2C | s32 | vec3 pool offset |

## Verification

Two *independent* tilings pin the layout, and both hold across all 992 files:

    [0x0C] + [0x08]*40 == [0x2C]      node array runs exactly to the vec3 pool
    [0x2C] + [0x28]*12 == filesize    vec3 pool runs exactly to EOF

`tools/asset/scol.py` parses **992/992, 0 failures**, additionally checking that
both AABB indices are in range and that min <= max componentwise — a semantic
check the structure alone would not give. Largest room span seen is
1200 x 1260 x 480.

## The full structure — from `SiCollisionMesh::GetFloor`

The loader never reads the 40-byte records, but the **query** does, and it pins
them completely. `GetFloor` is a two-level spatial lookup:

### Cell array — `[0x18]` entries of 32 bytes at `[0x1C]`

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | u32 | vec3 index — cell AABB **min** |
| 0x04 | u32 | vec3 index — cell AABB **max** |
| 0x0C | u32 | triangle count in this cell |
| 0x10 | u32 | byte offset of this cell's triangle-index list (u32 each) |

`[0x1C]` is 80 in every file (the 52-byte header, padded), and `[0x18]` — the
field previously marked unknown — is the cell count.

### Triangle array — `[0x08]` entries of 40 bytes at `[0x0C]`

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | u32 | vec3 index, vertex A |
| 0x04 | u32 | vec3 index, vertex B |
| 0x08 | u32 | vec3 index, vertex C |
| 0x24 | u32 | attribute mask, tested against the query's mask argument |

#### Attribute mask bits

Established two independent ways that agree:

- The engine's own `GetFloor` call sites pass masks **3** and **7** — bits 0..2
  only, never 3 or 4.
- A census of 40 rooms measured each triangle's normal:

| Bit | Value | Triangles | mean \|normal.y\| | Reading |
|-----|-------|----------:|-----------------:|---------|
| 0 | 1 | 2928 | 0.994 | floor |
| 1 | 2 | 5053 | 0.316 | mixed — applied to both |
| 2 | 4 | 1830 | 1.000 | floor |
| 3 | 8 | 625 | 0.000 | **wall** |
| 4 | 16 | 4 | 0.000 | **wall** |

So `kFloorMask = 0x7` and `kWallMask = 0x18`. This matches `eChrGetData.FLOORTYPE`
in `sk1.lua`, documented as `0:地面, 1:壁` (ground, wall).

Querying floors with `~0u` — as this port did at first — accepts vertical
surfaces as floor.

`GetFloor` early-outs on the room AABB (`[0x10]`/`[0x14]`), then per cell whose
XZ AABB contains the point, walks its index list, skips triangles whose mask
misses, and does a barycentric XZ point-in-triangle test.

### Verification

All **992/992** files satisfy the complete layout: cells precede triangles,
triangles abut the vec3 pool, the pool runs to EOF, and **every cell's
triangle-index list lies inside the gap between the cell array and the triangle
array with every index in range**.

Implemented as `Collision::GetFloor` and used for actor placement — for
`M0000_03_06` it returns y = 0.00 at all four enemy spawns, i.e. the grass, where
the scripts' own Y argument of 30 would have floated them at wall height.

## `.smdl` section 5 — vertex declaration

`SiModelBase::SetBinary` reads an array of 32-byte records from **section slot 5**
and remaps each into a 16-byte `SiVertexDeclarationParam` before handing them to
`SiVertexStream::Initialize`. The file therefore *states* its own vertex layout —
no inference required.

Record: `{u32 usage, u32 type, u32 offset, u32 stream, …}` (remaining 16 bytes are
zero in every shipped model).

| Usage | Meaning | | Type | Meaning | Size |
|-------|---------|---|------|---------|------|
| 0 | position | | 1 | float2 | 8 |
| 1 | weight | | 2 | float3 | 12 |
| 2 | incidence (bone index) | | 3 | float4 | 16 |
| 5 | color | | 4 | ubyte4 | 4 |
| 7 | texcoord0 | | 5 | ubyte4 | 4 |

Exactly two layouts exist across all 1375 models, and `max(offset+size) == stride`
holds for every one:

| Stride | Models | Layout |
|--------|-------:|--------|
| 24 | 1118 | `+0 position float3`, `+12 color ubyte4`, `+16 texcoord0 float2` |
| 44 | 257 | above, plus `+24 incidence ubyte4`, `+28 weight float4` |

### Correction

There is **no normal** in either layout. An earlier guess placed one at +12
(stride 44) and appeared to be supported — 69% of those vectors were unit length.
That was coincidence: bone weights sum to 1. The game's own skinning vertex
shader in `.rodata` declares exactly `position, texcoord0, color, weight,
incidence` and no normal, matching the declaration. Lighting comes from the
`mLight` uniform block, not per-vertex normals.

## `.smdl` sections 0 and 4 — materials and draw ranges

Record sizes fall straight out of the section-offset gaps (sections are laid out
back to back), and both were confirmed over the whole corpus.

### Section 0 — materials, 80 bytes each

| Off | Type | Meaning |
|-----|------|---------|
| 0x10 | u32 | index into the companion `.stex` texture array |
| 0x14, 0x18 | u32 | `0xFFFFFFFF` — unset secondary texture slots |
| 0x28 | u32 | material name offset (into the section-8 string table) |
| 0x2C | u32 | source texture path offset |

The strings are the original Square Enix authoring paths, e.g.
`m_b0000_00_head` and
`S:/svn_design/trunk/maya/Character/B0000_00/sourceimages/B0000_00_face.tga`.

### Section 4 — draw ranges, 32 bytes each

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | u32 | material index |
| 0x04 | u32 | index count |
| 0x08 | u32 | byte offset into the index buffer |

### Verification

Across **all 1375 models**, the draw ranges tile the index buffer exactly — each
range starts where the previous ended, and the counts sum to the buffer's index
count. **0 violations.**

Field 0 is the material index, not a sequence number: it matches the record
ordinal in simple models, but 16,016 records disagree, repeating values where a
material is reused (`M0000_02_07` ranges 10–13 read 8, 9, 8, 9).

**20,641 of 20,642** draw-range records reference a material that exists. The one
exception is a defect in the shipped data, not in this reading: `B0000_00`
range 2 is a 2-triangle range pointing at material 2 in a model declaring 2
materials. The host logs it by name and draws it untextured.

### Other sections, for completeness

| k | Record | Contents |
|---|--------|----------|
| 1 | 352 B | bone / skeleton record (count = bone count) |
| 3 | 32 B | mesh record; field 0 = number of draw ranges |
| 6, 7 | 16 B | vertex / index buffer descriptors (4th word unused) |
| 11 | — | the raw vertex + index data blob |

---

# Map texturing: `.stexinfo` + `.mtex` — REVERSED

Characters and maps take **different texture paths**, which is why map models
carry no `.stex`:

- **Characters** ship a `.stex` holding their atlases inline.
- **Maps** ship a `.stexinfo` *name list*; each name resolves to a shared
  `sk1/<name>.mtex`. Map textures are pooled across rooms rather than duplicated
  per room — 186 `.mtex` files serve 994 `.stexinfo` lists.

A map material's `texture_index` (`.smdl` section 0, +0x10) indexes the
`.stexinfo` list, whose entry name gives the `.mtex`. Confirmed on
`M0010_03_02`: 13 materials -> 13 `.stexinfo` entries -> 13 `.mtex` files, all
present, and the material's own Maya source path
(`.../sourceimages/house_01.tga`) matches the `.stexinfo` name (`house_01`).

## `.stexinfo`

    u32 count, then `count` x 256-byte STEXINFOFILE_BODY records.
    Each record begins with a NUL-terminated name; +0x80 holds a small u32
    (2 in the sampled files), meaning not yet determined.

## `.mtex` — 128-byte header, pixels at +0x80

Layout from `AppMapTexture::SetBinary`, which forwards the fields straight into
`MCFSiSurfaceTexture::Create`:

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | u32 | format code — through the **same** table as `.stex` (`.rodata:0x9dda0`) |
| 0x04 | u32 | width |
| 0x08 | u32 | height |
| 0x0C | u32 | **max mip level** — the level count is this value **+ 1** |
| 0x10 | u32 | pixel data size |
| 0x80 | — | pixel data |

The +1 matters and is easy to get wrong: `anvil_01` stores 7 with a 128x128
image, and 4 x (128^2 + ... + 1^2) over **8** levels is 87380, exactly the stored
size, whereas 7 levels gives 87376.

## Verification

**185/186 `.mtex` files** satisfy `128 + data_size == filesize` **and**
`data_size == 4 * sum(mip areas)`. All 186 are format code 0 -> RGBA8888.

The single failure is `room_field.mtex`, the only non-power-of-two texture in the
game (240x240): its mip chain is 58 bytes larger than the exact sum, so NPOT
levels are padded. Not resolved — it is one file, and the padding rule should be
derived rather than guessed.

---

# `.smdl` section 1 — skeleton (352-byte `SiModelBone`) — REVERSED

`SiModelBase::CreateSkeleton` passes the file pointer straight to
`SiModelSkeleton::_SetBinary`, so the 352-byte record **is** the runtime struct.
Field offsets come from `SiModelBase::GetBoneName` / `GetBoneIDByName`, which
index at stride `0x160` and read the name at `+0x14C`.

| Off | Size | Meaning |
|-----|------|---------|
| 0x000 | 64 | **local bind transform**, parent-relative, column-major 4x4 |
| 0x040 | 64 | second matrix (identical to 0x000 in only 711/5344 bones — role unknown) |
| 0x080 | 64 | **inverse world bind matrix** — what skinning needs |
| 0x0C0 | 64 | as 0x040 |
| 0x100 | 64 | identity in every bone sampled |
| 0x144 | 4 | **parent index**, -1 for root; always < own index |
| 0x14C | 4 | name offset into the section-8 string table |

## Verification

Chaining the local transforms at `+0x00` up the parent hierarchy and inverting
reproduces the matrix at `+0x80` for **5342 of 5344 bones** across 250 models.

The two exceptions are not errors. `B0021_00`'s `l_effect_01` and `r_effect_01`
are **zero-scale** bones — their 3x3 block is all zeros, so the inverse is
genuinely infinite, and the shipped file stores `inf`/`nan` there. The
derivation and the data agree; the apparent mismatch was an artifact of
comparing infinities. Any vertex weighted to such a bone yields NaN, which
renders as scattered garbage rather than an obvious failure, so the host pins
degenerate bones to identity.

Bones are topologically sorted in every shipped model, so a single forward pass
computes all world transforms.

## Skinning

The game's skinning vertex shader takes `uniform vec4 vJoint[80*3]` — three vec4
per bone, the **rows** of a 3x4 matrix — and blends two bones with weights
`(weight[0], 1 - weight[0])` indexed by `incidence.x` / `incidence.y`. Skin
matrix is `world_animated * inv_world_bind`.

### Control test

With no animation the skin matrix collapses to `world_bind * inv_world_bind` =
identity, so the skinned path must reproduce the unskinned render. It does:
**71 differing bytes out of 2,074,320, max delta 1** — pure float rounding. That
single comparison exercises bone parsing, world accumulation, the inverse bind
matrix, the `vJoint` row layout and the two-bone blend at once; any of them wrong
would scramble the model.

Animation then plays correctly: `B0000_00_002_DASH` at frame 12 produces a
coherent crouched dash pose. Rotation quaternions are stored **xyzw**.

---

# `.txb` is a TEXTURE bank, not a text bank

Corrects an assumption made from the extension alone. The 8 `.txb` files hold UI
sprite-atlas entries — `button_system_base`, `button_system_press`,
`cursor_select`, `help_wnd`, `icon_system_dustbox` — not strings.

# OPEN: where is the dialogue text?

`GetIDString(char const*)` is called 181 times by the shipping scripts with ids
like `SYS_COMMON_STATUS_LABEL_6` and `SYS_SHOP_TITLE`. **That text is not in
anything extracted so far**, and this was checked rather than assumed:

- Not as plain text in any of the 9886 MPK entries; the only file containing
  `SYS_COMMON_STATUS_LABEL` is the Lua script that *calls* it.
- Not in the APK's `resources.arsc` (checked as both UTF-8 and UTF-16).
- Not in `sk1patch.mpk`, which contains exactly one entry: `sk1/dummy`.
- No asset file contains a meaningful run of CJK text — a scan for UTF-8 CJK
  sequences returns only textures and PNGs matching the byte pattern by chance.

Two candidate explanations, neither confirmed:

1. **It is in the OBB expansion file.** The manifest declares
   `net.gorry.expansion.downloader.ObbDownloaderService`, so the retail game
   downloads an expansion. This repack fused `sk1.mpk` and the BGM into
   `assets/`, and may simply not include the rest of the OBB.
2. `GetIDString` resolves through a **hashed** table inside one of the
   undecoded binary formats (`.dat`, `.gdt`, `.odt`, `.edt`), in which case the
   ids would never appear as literal strings.

Distinguishing them means reversing `GetIDString` ->
`ModeGame`/`GameParameter`. Until then dialogue cannot be displayed, and no
amount of UI work changes that.

## Per-room data tables — `.odt`, `.gdt`, `.edt`

Three sibling tables ship next to each room's `.smdl`/`.scol`. All three layouts
come from the engine's own loaders in `libmcfandroid.so`, not from pattern
matching the bytes. Parser: `tools/asset/roomdata.py` (in `tools/verify.sh`).

### `.odt` — map objects, `ModeGame::ObjFileLoad(char*, float, float)` @ `0x2e6904`

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | u32 | version, **must be 2** (`cmp w8, #0x2`) |
| 0x04 | i32 | record count (`cmp w8, #0x1; b.lt` skips the file) |
| 0x40 | — | first record; stride **0xC0** (`add x8, x27, #0xc0`) |

Each record is an `AppObjectModel::PARAMETERIMAGE` handed to
`ModeGame::CreateMapObject`. Decoded fields:

| Offset in record | Type | Meaning |
|---|---|---|
| 0x00 | i32 | kind — **1 in all 3284 shipping records** |
| 0x04 | i32 | object id; looked up in a table of 0x138-byte entries |
| 0x08 | f32×3 | position X, Y, Z, in **world** coordinates |

The remaining 0xA8 bytes are mostly 1.0f scale-looking values and zeroes and are
NOT decoded. **424/424 files satisfy `len == 0x40 + count*0xC0` exactly** — a
real test, since a wrong header size or stride fails the equation outright.

### `.gdt` — ground attributes, `ModeGame::Load_GroundAttribute(char*,int,int)` @ `0x2e6cf4`

A per-room grid of 7.5-unit cells. The engine computes the grid itself and then
validates **all five header fields** against its own values before copying the
payload, so the layout is not inferred:

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | u32 | version, must be 1 |
| 0x04 | i32 | columns — `ceil(room_width / 7.5)` |
| 0x08 | i32 | rows — `ceil(room_depth / 7.5)` |
| 0x0C | f32 | cell width, **7.5** (`fmov v0.2s, #7.5`) |
| 0x10 | f32 | cell height, 7.5 |
| 0x14 | u32×cols×rows | attribute per cell |

**657/657 files satisfy `len == 0x14 + cols*rows*4`**, and the cell size is 7.5
in every one. Grid dimensions are 40×32 (385 files) and 44×36 (271 files), i.e.
implied extents of 300×240 and 330×270.

**That 330×270 independently corroborates the collision finding**: `.scol` AABBs
carry a uniform 15-unit margin around a 300×240 room, which is exactly
300+2×15 by 240+2×15. Two unrelated formats agreeing is real evidence for the
300×240 world cell. Both grid sizes appear *within* the same map, so this is a
per-room padding choice, not a per-map room size.

### `.edt` — effect placements, `ModeGame::EffFileLoad(char*, int, int)` @ `0x2e6b54`

Headerless array of **28-byte** records. The size is not guessed: the loader
rejects anything below `0x1c` bytes (`cmp w0, #0x1c; b.lo`) and derives the
count with a divide-by-7-words (`umull` by `0x24924925`, `lsr #32`).

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | i32 | effect id |
| 0x04 | i32 | flags / sub-type |
| 0x08 | f32×3 | position X, Y, Z |
| 0x14 | f32 | scale — 1.0 in every shipping record |
| 0x18 | i32 | 0 in every shipping record |

**247/247 parse.** Three files (`M0022_00_09`, `M0022_06_06`, `M0022_08_10`) are
a single byte `0x61`; the engine's own size check discards them, so they are
empty tables and are read as such rather than counted as failures.

### RESOLVED: rooms are not all one grid cell

An earlier version of this document listed "303 of 3284 objects sit outside
their own room cell" as an open question, with a per-map origin as the likely
cause. **That was the test being wrong, not the data.**

Checked against the thing that actually defines a room's extent -- its own mesh
-- **3284 of 3284 objects are inside their own room's XZ bounds. Zero outside.**

The bad assumption was that every room occupies exactly one 300x240 cell. It
does not: measuring all 993 room meshes, 853 span one cell but **140 span
several** (43 are 2x2, 42 are 2x1, 16 are 1x2, and the rest up to 4x2 and 3x4).
Dungeon maps are where the large rooms live, which is why the failures looked
map-correlated and invited a per-map origin theory.

The tell was in the data before the mesh check: the spill was **only ever in +x
and +z, never negative**, in offsets of one or two cells. A wrong origin would
scatter in both directions; a room bigger than its anchor cell can only spill
forward. The 300x240 cell is still correct as the room ANCHOR, which is what the
port uses, and is still pinned uniquely by the overworld's 2331 objects.

`roomdata.py` now checks against mesh bounds and carries a self-test: it
displaces a real object outside its room and requires a reject, because a check
that passes 3284 of 3284 is exactly the kind that might be measuring nothing.

### The `.odt` object id is NOT the `O####_##.smdl` number

Tested and **refuted**: only 15 of the 97 distinct ids have a model whose
filename number matches, and that overlap is explained by both being small
integers. The real mapping is a table the engine carries in `.rodata`:

`ModeGame::LoadMapObject()` @ `0x2e10b8` memcpy's `0xC570` bytes from `0xa625c`
and loops `w23 = 162` times. `0xC570 == 162 * 0x138`, so it is 162 entries of
312 bytes. `ModeGame::CreateMapObject` @ `0x2e7460` then linear-searches the
table comparing entry word 0 against the object id, and formats the entry's
`+0x84` string through `"sk1/%s"` to build the model path.

Extractor: `tools/asset/object_table.py` -> `docs/object-table.md` and
`src/engine/object_table.inc`. **All 162 names exist as shipping models and all
3284 placements across 424 rooms resolve** — a result that would have been
impossible under the refuted filename-number theory.

(The shadow planes originally drew as opaque black quads. Fixed -- see the
material blend flag below.)

## Player damage, from the engine

`AppCharacterPlayer::DamageProcess(int)` @ `0x2b5b40` is short and complete:

    if (f32 @ +0x1ff0 > 0) return;              // invulnerability window
    hp = [oG][+0x174] - damage;
    hp = hp & ~(hp >> 31);                      // branchless clamp to 0
    [oG][+0x174] = hp;
    [this][+0x1ff0] = [this][+0x3aa0];          // reload the i-frame timer

and its caller `AppCharacterPlayer::Damage` floors the amount at 1
(`csinc w1, w20, wzr, gt`) after randomising with `GameRandom`. The port's
floor-at-1, which had been labelled a port choice on the enemy side, is
therefore the engine's own behaviour.

The player's defence comes from `DataTableGetDefence` @ `0x2c3bd8`, which
indexes `tblHelm` and `tblArmor` (stride 20). A new game is granted helm 201 and
armor 301 by `GameParameter::Init`, giving **defence 2 + 2 = 4**.

**NOT modelled: the player's HP pool.** `[oG][+0x174]` and `[oG][+0x17c]` are
only ever READ in the binary -- they are populated from save data, not from a
table -- so the port applies damage and reports the running total instead of
inventing a maximum. The i-frame DURATION is likewise a per-character field the
port cannot source and is a named constant.

## Material blend flag — section 0 word 9 (+0x24)

Alpha-blended materials set word 9 of the 80-byte material record to 1.

**Status: strongly evidenced, NOT confirmed from the engine's GL calls.**
`SiVertexStream::_SetBlendingInfo` @ `0x35f63c` is where blending is actually
programmed — it gates `glEnable(GL_BLEND)` on its own `+0x6c`, takes the
equation from `+0x54/+0x58` and the four `glBlendFuncSeparate` factors from
`+0x5c..+0x68` through lookup tables at `0x1e0e64` and `0x1e0e70`. The hop that
copies a material's fields into that object is **not** reversed, so the port
does not claim to reproduce the game's exact blend equation; it uses ordinary
`SRC_ALPHA / ONE_MINUS_SRC_ALPHA` with depth-write off.

What is measured, over all **6145** shipping materials, checking both classes
rather than only the positive one:

| | word9 == 1 | word9 != 1 |
|---|---|---|
| name contains "shadow" | **377** | **0** |
| does not | 425 | 5343 |

Zero false negatives. The 425 others are not noise: every one is a `wave_*`,
`sea_*` or `water` surface, i.e. independently the other thing a blend flag
should catch. Three rival candidate words were run through the same test and
all failed badly (word13: 282 false negatives; word4: 312; word12: 4006 false
positives).

### Verified by what it changes on screen

Enabling blending on this flag was checked against pixels, not by eye:

- **Object shadows.** 15,766 pixels in the play area changed, and every one of
  them was the flat `(50, 50, 50)` shadow-quad colour before and a blended grass
  colour after. Nothing else in that region moved.
- **Water, which was not targeted.** The room mesh's `sea_01_Mat` band went from
  opaque `(255, 255, 255)` to blended blue over 5,556 pixels.

One flag, two visually distinct defects, in two different asset classes (an
`.odt` object model and the room mesh). The water case is the strong one: it was
predicted by the corpus statistics before it was looked at.

Note the first attempt to measure this reported "no change" — a `< 40`
near-black threshold, while the shadow quads are exactly 50. The instrument was
wrong, not the fix; the counts above come from a direct before/after pixel diff
with no threshold.

## `sk1/enemydat.bin` — the game's data tables

Found via the `DataTable*` exports. `DataTableInit()` @ `0x2c36bc` reads exactly
one file, `sk1/enemydat.bin`, and stores the buffer and its byte size in `.data`
at `0x420ce8` / `0x420ce0`. Every `DataTableGet*` accessor indexes into it:

| Accessor | Record stride | Keyed by |
|---|---|---|
| `DataTableGetEnemy(int)` @ `0x2c3d4c` | **0x198 (408)** | linear search on word 0 |
| `DataTableGetItem(int)` @ `0x2c38f4` | 0x14 (20) | direct index, bounds `< 0x25` |

`DataTableGetEnemy` derives its record count by dividing the stored file size by
0x198 (the `umulh` by `0xa0a0…a1` then `lsr #8`). **43656 / 408 = 107 exactly**,
so the enemy table is 107 records and the stride is confirmed by the file
dividing evenly.

Sibling accessors, not yet followed: `DataTableGetLevelUp`, `DataTableGetName`,
`DataTableGetItemName`, `DataTableGetItemArticleName`, `DataTableGetWeapon`,
`DataTableGetThrowWeapon`, `DataTableGetDefence`, `DataTableGetMagic`,
`DataTableGetHelpString`, `DataTableGetBgm`, `DataTableGetEffectPrm`,
`DataTableGetIdType`, `DataTableIconPictureID`.

### Enemy record fields the engine actually reads

`AppCharacterEnemy::SetEnemyId` / `::SetBossId` memcpy the whole 408-byte record
into the character at `+0x3a24`, so every later `[x19, #0x3a**]` access is a
record field at a known offset. Those functions read exactly ten:

| Record offset | Read as |
|---|---|
| +0x000 | i32 — the id the accessor searches on |
| +0x008, +0x020 | i32 |
| +0x058, +0x05C, +0x060, +0x068 | f32 |
| +0x064, +0x06C, +0x078, +0x07C | i32 |

Ids are sparse (0..209 across 107 records), not sequential; the last three
records are entirely zero.

### Fields identified, each pinned to an engine read

| Offset | Meaning | How it is pinned |
|---|---|---|
| +0x00 | enemy id | `DataTableGetEnemy` compares word 0 against its argument |
| +0x04 | **max HP** | `AppCharacterEnemy::GetStatusMaxHp` @ `0x2b2318` reads the id at `+0x3a24`, calls `DataTableGetEnemy`, returns `[x0, #0x4]`. `Damage`'s death path also stores it back into the live HP field to reset the enemy |
| +0x08 | **attack power** | `SetEnemyId` passes it to `SetCollisionAttackParam`, which stores it at the param's `+0x24`; `AppCharacterPlayer::Damage` loads exactly that field (`ldp s0, s1, [x21, #0x24]`) as the attacker's power |
| +0x0C | **defence** | the subtrahend in `Damage`'s `sub w22, w28, w27`, with `w27` loaded from `[x20, #0x3a30]` |
| +0x10 | **EXP reward** | passed to `GameParameter::AddEXP` |
| +0x14 | **money reward** | passed to `GameParameter::AddRC` |

Current HP is not a separate field: `SetEnemyId` memcpy's the whole record into
the character at `+0x3a24`, and `GetStatusHp` @ `0x2b2310` returns the character's
own mutable copy of `+0x04` while `GetStatusMaxHp` re-reads the pristine table.
`GetStatusMp`/`GetStatusMaxMp` return a literal 0 -- enemies have no MP.

The values corroborate the offsets independently: id 0 is 4 HP / 1 defence /
1 EXP, id 26 (the werewolf) is 60 / 15 / 48, and the table tops out at 4480 HP.
That is a clean difficulty progression by id, which a wrong offset would not
produce.

### The damage formula, and the one number that is invented

`AppCharacterEnemy::Damage` @ `0x2b2b00` computes **attack - defence** (there is
a branch that first quarters the defence, and a later scale by a rate parameter
at `[x21, #0x2c]`).

The quartering branch is a SPECIAL CASE, not the normal path: it is guarded by
an element/attribute value of 9 or 10 plus a flag at `[x25, #0xa5f]`. The normal
path subtracts the full defence.

The port applies that formula with the real defence and a real weapon attack
(see `docs/weapon-table.md`). What is still **not** modelled:

- **Which weapon is equipped.** No inventory or save system, so the port always
  uses weapon 101. That id is **verified, not assumed**: `GameParameter::Init`
  @ `0x2c6d14` grants it via `AddItem(0x65, 1)` on a new game, along with 201,
  301 and 401 (the other starting equipment slots). What is unmodelled is the
  player ever changing weapons, not which one they start with.
- **The charge meter.** `[oG][+0x1b8]` is a float the player winds up, and it
  DOES scale damage -- see the formula below -- but the port does not drive it
  yet, and an empty meter is the neutral 1x, which is also what
  `GameParameter::Init` leaves it at. An earlier version of this note claimed
  the meter WAS the attack power passed to the attack volume. **That was
  wrong.** `AppCharacterPlayer::SetCollisionAttackParam` @ `0x2b7e8c` stores the
  meter at param `+0x2c` and the attack power separately at `+0x30`, straight
  out of `GameParameter+0x124`, with the magical attack at `+0x34`.
- **Enemy weaknesses.** Bytes at the enemy record's `+0xa5c`..`+0xa5f` gate the
  quartered defence below, each against an attack-type id (`0x7b`, `0x7c`, 9,
  10). The ids are not decoded, so the port never sets `weak`.
- ~~**The floor at 1 damage** is a port choice.~~ **It is the engine's.**
  `cmp w8, #1` followed by `csinc w22, w8, wzr, gt` @ `0x2b349c` clamps damage
  up to 1, so an outmatched attacker is slow rather than harmless.

The consequence is visible and left visible: the starting weapon (attack 4-8)
against the werewolf's defence of 15 deals the floor of 1, so a 60 HP enemy
takes 60 hits. That is what the reversed data actually says; the missing piece
is the level bonus, not the defence reading, which is pinned to an engine read.
Combat balance is not faithful until the attack chain is finished.

`DataTableGetName` and `DataTableGetHelpString` are a live lead on the missing
text problem recorded above and have not been followed yet.

## Room-local -> world: the engine uses a PER-ROOM size

`ModeGame::RoomLocalToWorldX(float)` @ `0x2e3584` and its Z twin are short and
unambiguous:

    world_x = room_size_x * grid_x + local_x

where `room_size_x` is **not a constant**. It is read from a per-room record
(`f32 @ +0x9dc` for X, `+0x9e0` for Z, indexed through the room's own entry),
which is also the value `ModeGame::AddCharacterRandomPos` divides by 30 to get
the room's chip grid.

The port uses a fixed 300x240, which is right for the overworld (uniquely fitted
by 2331 objects) but wrong for the 140 of 993 rooms whose mesh spans more than
one cell. Measured cost, over every script-placed actor in the game:
**85 of 116 stand on floor; 31 do not** and keep their script Y instead.
`--room-census` reports this ratio every run.

### A hypothesis that was tried and FALSIFIED

Using the room's collision AABB as the origin (`aabb_lo + 15`, from the uniform
15-unit margin found earlier) scored **116 of 116** on the actor-on-floor test
and was very nearly shipped. It is wrong:

- the AABB's `lo` corner differs from `grid * 300` by multiples of 30 -- the
  chip size -- in **659 of 992** rooms, not by a constant margin;
- its size is not one value but 330x270 (520 rooms), 300x240 (281), 300x300,
  300x210, 300x180, 300x270;
- applied to `M0000_00_00`, whose AABB is exactly `(0,0)..(300,240)`, it puts
  the origin at (15,15) where the grid correctly says (0,0).

It is a tight bound on the collision geometry, not the room's extent. It scored
perfectly only because floors are broad enough to absorb a 15-unit error, which
is exactly the way a test passes without measuring what it claims to. The fixed
rule is kept, with its cost stated, until the per-room size table is reversed.

## `str_en.bin` / `str_ja.bin` — the string table (dialogue IS present)

This document previously stated that dialogue text was absent from the extracted
data and that it probably lived in an OBB this repack omits. **That was wrong.**
The text is in the archive, in two files whose names do not say "text":

| | |
|---|---|
| `sk1/str_en.bin` | 1906 strings, English |
| `sk1/str_ja.bin` | 1906 strings, UTF-8 Japanese, ids byte-identical to `en` |

The earlier search failed because it looked for CJK byte runs and inside the
per-room assets. The lead came from `NN_minimap_lst.bin`, which holds string
IDs (`AREA_NAME_1`, ...); grepping the corpus for one of those ids found these
two files immediately.

    +0x00  u32  version, 1
    +0x04  u32  count (1906)
    +0x08  u32  44
    +0x0C  u32  offset of the text block
    +0x10  u32[count]   permutation of 0..count-1 (the ids in sorted order,
                        i.e. the index GetIDString binary-searches)
    ...    count records of 48 bytes, each a NUL-padded ASCII id
    text   count NUL-terminated UTF-8 strings, in record order

### Why the id/text pairing is proven, not assumed

The original developers left the dialogue as comments beside the calls that show
it in `sk1.lua`. Those comments are an independent source for the same strings:

| id | `sk1.lua` comment | decoded `str_ja.bin` |
|---|---|---|
| `SYS_PARTYMSG_1_1` | `お怪我は　大丈夫ですか？` | identical |
| `SYS_PARTYMSG_2_1` | `洞くつには　マトックや\nモーニングスターで　こわせる壁が\nあるという` | identical |

`tools/asset/strings.py` re-runs both cross-checks, and refuses a truncated
table rather than returning a partial one.

`sk1.lua`'s `msgId(id)` is `SetMessageWnd(GetIDString(id))`, so those two calls
are the whole dialogue path at the data level. Both are implemented: an id that
is not in the table echoes back and is counted, so a miss is visible instead of
becoming an empty line. **There is no on-screen message window yet** -- the text
is resolved and logged, not drawn.

## `BasicFont.sfont` — the bitmap font

Layout from `SiFont::SetBinary` @ `0x35a534`, metrics from
`SiFont::DrawString` @ `0x35a858`.

| Offset | Meaning |
|---|---|
| +0x0C, +0x10 | atlas width, height (128 x 128) |
| +0x14 | offset of the pixel data — 8-bit coverage, one byte per texel |
| +0x18, +0x1C | count and offset of the codepoint map (`u16`, `0xFFFF` = none) |
| +0x20, +0x24 | count and offset of the glyph records, 10 bytes each |

The record size is the engine's: `SetBinary` allocates `count * 5 << 1`. The
three sections **tile the file exactly** (map ends where glyphs begin, glyphs end
where pixels begin, pixels end at EOF), which is what validates the header.

Glyph record, with the fields `DrawString` actually reads (all as SIGNED bytes):

| Offset | Meaning |
|---|---|
| +0x00, +0x02 | `u16` atlas x, y |
| +0x04, +0x05 | width, height |
| +0x06 | not read by `DrawString`; undecoded |
| +0x07, +0x08 | left and right side bearing |
| +0x09 | vertical offset from the line origin |

**The pen advances by `width + [+7] + [+8]`** — that is the engine's own
expression, not an assumption, and it yields sensible values (`A` 8, `B` 7,
`a` 7, space 4).

Verified by rendering: the record for `A` claims atlas (78,12) size 8x9, and
cropping exactly that rectangle out of the atlas produces the letter A.

**Limitation:** the font covers exactly ASCII 32..126 (95 glyphs) — confirmed
from the codepoint map, not assumed. Japanese therefore has no glyphs; the game
draws CJK with the Android system font, which is not in the archive. The port
warns once per line when characters cannot be drawn, so a blank panel is not
mistaken for a broken message window.


## Dialogue control codes -- expanded, from the engine's own parser

393 of the 1906 English strings (435 Japanese) carry `@` control codes, e.g.
`@N(36):\nAren't you...`. They are not part of the text: every window runs its
string through **`CnvFormatString` @ `0x2c33b4`** before drawing it
(`SetMessageWnd` @ `0x2c7874`, `SetInfoWnd`, `SetNameWnd`, ...). The whole
branch table, read off that one function:

| code | expands to | evidence |
|---|---|---|
| `@N(n)` / `@n(n)` | the `CHARACTER_NAME_<n>` string | `0x2c34d8` formats `"CHARACTER_NAME_%d"` (`.rodata` `0x943ce`) and calls `StrFileGetString` |
| `@H` / `@h` | the hero's name (`oG+0x68`) | `0x2c3590` |
| `@G` / `@g` | the girl's name (`oG+0xe8`) | `0x2c356c` |
| `@P` | parameter slot 0 | `0x2c35ec` -> `pCnvFormatStringPrm[0]` |
| `@i` | parameter slot 1 | `0x2c35e4` |
| `@I` | parameter slot 2 | `0x2c35d4` |
| `@S` | parameter slot 3 | `0x2c35dc` |
| `@@` | a literal `@` | `0x2c35c4` |
| `@<digits>` | the caller's argument array | `0x2c3618` |
| anything else | the `@` is dropped, the letter kept | fall-through at `0x2c3618` |

So `@N(36):` is `Prisoner:` and `@H` is the player's name.

The two name fields are not invented. `oG+0x68` and `oG+0xe8` are both handed to
`SaveAccessStr` by `_GameSaveAccess` @ `0x30c874`, so they are save data, and
`ModeInit::Process` @ `0x2f65e4` seeds them from `SYS_DEFAULTNAME_HERO` ("Sumo")
and `SYS_DEFAULTNAME_GIRL` ("Fuji") before the name-entry screen can overwrite
them. The four parameter slots are `szCnvFormatStringPrm`, four 256-byte buffers
written by `SetMessageWndPrmString(slot, text)` @ `0x2c7860` -- which is one of
the 200 `cmd` functions, so the scripts fill them.

**Not ported, and it does not matter here:** the `@N` argument goes through
`GetIntFromString`, a general operator-precedence expression evaluator (it
mallocs an 8 KB work buffer). All 577 `@N` occurrences across both string tables
are a parenthesised integer literal, so the port parses that form and refuses
anything else rather than half-evaluating an expression.

`--text-selftest` checks both classes -- strings that must change and strings
that must not -- and then sweeps the whole table, reporting how many strings
still contain a `@` after expansion. That residue is 1 of 393: the
`SYS_INFO_ITEM_AUTOSTACK` string `"@i (@1)"`, whose `@1` indexes the caller's
argument array, and every caller reached from the dialogue path passes NULL.


## Room extent, and where a room sits in the world

A room's world position is NOT a fixed 300x240 cell. `ModeGame::RoomLocalToWorldX`
@ `0x2e3584` is the whole rule:

```
size   = this->size_table[ this->rooms[width*gy + gx].size_class ]   // stride 16
world  = size.w * gx + local      // RoomLocalToWorldZ: size.h * gy + local
```

`MakeRoomMinMax` @ `0x2e61b8` gives the same room the box
`[w*gx, w*(gx+1)] x [h*gy, h*(gy+1)]`, and `RoomSizeW`/`RoomSizeH` @ `0x2e3654`
return the two fields. `AddNPC` @ `0x2c8a10` runs the script's x and z through
`RoomLocalToWorld{X,Z}` before spawning, so script coordinates are room-local and
this is the conversion.

**The size is readable per room, from the room's own `.gdt`.**
`ModeGame::Load_GroundAttribute` @ `0x2e6cec` computes the ground-attribute grid
from the room size -- `ceil(size / 30)` chips, four cells per chip, cell = the
literal `7.5` (`fmov v0.2s, #7.5`) -- and then REFUSES the file unless its header
matches what it computed, comparing all four of cols, rows, cell width and cell
height. So `w = cols * 7.5` and `h = rows * 7.5` is the engine's own number, not
a measurement of the geometry. Across the 657 `.gdt` files:

| size | rooms |
|---|---|
| 300 x 240 | 385 |
| 330 x 270 | 271 |
| 600 x 240 | 1 (`M0023_05_00`) |

**335 rooms ship no `.gdt`**, and the table the engine indexes lives in
`ModeGame` at `+0x9dc` (sizes, stride 16) and `+0xa64` (per-room records, stride
136, map width at `+0xa5c`); what fills it has not been found. For those rooms
the port infers the size, and says so: it takes whichever of the two common
sizes puts the room's collision AABB at exactly `size * grid_index`, defaulting
to 300x240 when the grid index is 0 (which decides nothing) or neither matches.

That inference is scored where the truth is known rather than only where it is
not: run on the 656 rooms that have BOTH a `.gdt` and a `.scol`, it agrees with
the `.gdt` in **654** and disagrees in two (`M0000_03_03`, `M0020_04_04`).
`tools/asset/roomdata.py` runs that comparison, and a self-test feeds it one
corner at `330*gx` and one at `300*gx` to prove it can answer either way.

The independent check is actor placement: every script-placed actor should stand
on walkable floor. With a fixed 300x240 cell, 85 of 116 did. With the per-room
size, **116 of 116** do -- and the 16 that the earlier `.dat`-class guess still
got wrong (shop and inn interiors, whose rooms are 300 wide despite their class)
are among them.

Two dead ends worth not repeating: the first byte of the room's `.dat` looks
like a size class and predicts the `.gdt` size in 655 of 656 rooms, but it is
wrong for exactly the shop interiors, so it was dropped. And using the collision
AABB as the ORIGIN (rather than to choose between two sizes) was falsified
earlier -- see the note above on the +15 margin.


## The player's own numbers (`GameParameter`)

`GameParameter::Init` @ `0x2c6d14` sets what a new game starts with, and
`GameParameter::Update` @ `0x2c6f14` derives everything else from it every
frame. `GameParameter` lives at `oG+0x60`.

| field | offset | new game |
|---|---|---|
| level | `+0x110` | 1 |
| HP current / max | `+0x114` / `+0x11c` | 19 / 19 |
| MP current / max | `+0x118` / `+0x120` | 6 / 6 |
| attack | `+0x124` | derived |
| defence | `+0x128` | derived |
| EXP | `+0x12c` | 0, capped at 999999 |
| EXP for next level | `+0x130` | derived |
| money | `+0x134` | 50, capped at 65535 |
| effective stats | `+0x138`..`+0x144` | derived, each capped at 99 |
| base stats | `+0x148`..`+0x154` | 2, 2, 2, 2 (one `movi v0.4s, #2`) |
| equipment | `+0x36c`, `+0x374`, `+0x37c`, `+0x384` | 101, 201, 301, 401 |

`Update` copies the four base stats into the effective ones with a **shuffle** --
`+0x148 -> +0x13c`, `+0x14c -> +0x138`, `+0x150 -> +0x140`, `+0x154 -> +0x144` --
adds 15 to any stat with an active temporary effect (`+0x3b8`..), caps each at
99, and then:

```
max_hp   = stamina^2 / 10 + 19          (999 once stamina^2 >= 9810, i.e. 100)
max_mp   = wisdom * 94 / 100 + 5
attack   = power   + tblWeapon[weapon].atk_LOW      // the record's +0x04
defence  = stamina + 1 + tblHelm[helm] + tblArmor[armor]
next_exp = 12*L + 3*L^2 + 103*L^3/100               (capped at 999999)
```

Both derived formulas reproduce `Init`'s own literals at stat 2 -- `2*2/10+19`
is 19 and `2*94/100+5` is 6 -- which is the check that they are right rather
than merely plausible, since `Init` wrote those numbers and the derivation never
saw them. `--player-selftest` runs that, plus grown stats and the caps.

Note that **stamina carries both HP and defence**; there is no separate defence
stat. And the attack figure is the weapon's LOW end, not its high one -- the
port used the high end before this was read off the instruction.

The fourth equipment slot is a shield, and `Update` does not add it to defence.
`tblShield`'s defence word is zero in all nine records; the field that varies is
a bit mask at `+0x10` (1, 3, 7, 15, 23, 31, 55, 87, 127), so a shield blocks
kinds of damage rather than reducing it. What the bits mean is not established.
See `docs/weapon-table.md`.

### Levelling up (`tblLevelup`)

`tblLevelup` is **64 bytes** -- the dynamic symbol's own size -- i.e. four rows
of 16, the stride being `DataTableGetLevelUp`'s `lsl #4` @ `0x2c375c`. Four rows,
because the game asks the player to pick a training regimen on each level-up.
`ModeGame::Process_LevelUp` @ `0x2e0100` does the whole thing:

1. level += 1
2. load the chosen row and **swap its first two lanes** --
   `rev64 v1.4s, v0.4s` then `mov v1.d[1], v0.d[1]` gives `[w1,w0,w2,w3]`
3. add that to the four base stats at `+0x148`
4. call `Update`, then copy max HP and max MP into the current values: **a
   level-up is a full heal**
5. at level 99, fire achievement `AC0033`

| choice | row as stored | after the swap | `SYS_LEVELUP_TYPE_n` |
|---|---|---|---|
| 0 | 1, 2, 0, 1 | power +2, stamina +1, will +1 | Warrior |
| 1 | 2, 1, 0, 1 | stamina +2, power +1, will +1 | Monk |
| 2 | 1, 0, 2, 1 | wisdom +2, stamina +1, will +1 | Mage |
| 3 | 1, 0, 1, 2 | will +2, stamina +1, wisdom +1 | Sage |

That lane swap is the one thing here that could plausibly be off by one, and the
game's own help text settles it independently of the disassembly:

| id | text | stat it must raise |
|---|---|---|
| `SYS_HELP_LEVELUP_FIGHTER` | "improving physical **ATK**" | power |
| `SYS_HELP_LEVELUP_MONK` | "improving **DEF** and increasing **HP**" | stamina |
| `SYS_HELP_LEVELUP_WIZARD` | "increasing magical ATK and **MP**" | wisdom |
| `SYS_HELP_LEVELUP_WISEMAN` | "increasing **limit gauge** build speed" | will |

Every one agrees with the swapped reading and none with the unswapped one, and
this closes two other loops at the same time: Monk's text naming *both* DEF and
HP is exactly why `Update` derives both from stamina, and Sage's text explains
why nothing in `Update` reads `will` at all -- it feeds the limit gauge, the
field `Init` zeroes at `GameParameter+0x158`, which is `oG+0x1b8` -- the charge
meter, which scales damage (see "The damage formula" below).

`--player-selftest` asserts each regimen raises the stat its own help text names
and raises it more than any rival does. Deleting the lane swap fails exactly
Warrior and Monk, so the check is not decoration.

Still missing, and not faked: there is no save/load path, so the port always
starts a new game at level 1 and nothing calls `LevelUp` yet -- EXP is tracked
but the level-up *screen* (choosing a regimen) is UI that has not been built.


## Who may damage whom

`AppCharacterBase::GetType()` is the engine's faction tag, and each subclass
returns a literal:

| class | `GetType()` |
|---|---|
| `AppCharacterPlayer` @ `0x2b8034` | 1 |
| `AppCharacterParty` @ `0x2b5784` | 2 |
| `AppCharacterNPC` @ `0x2b5118` | 3 |
| `AppCharacterEnemy` @ `0x2b49d4` | 4 |

Both `Damage` overrides test the ATTACKER's type before anything else. They
fetch the attacker with `CollisionBase::GetParentPointer` on the incoming
`CollisionParam` and call its `GetType` through vtable slot `+0x198`:

- `AppCharacterEnemy::Damage` @ `0x2b2b00` returns unless the type is **1 or 2**
- `AppCharacterPlayer::Damage` @ `0x2b5b7c` returns unless the type is **4**

So an enemy cannot damage another enemy, and only an enemy can damage the
player. That is a filter read off the binary, not a house rule about how a game
"should" behave.

This mattered: without it the port had enemies fighting each other, and doing it
with the PLAYER's attack value, because the damage branch assumed any non-player
defender had been hit by the player. The combat summary now prints how many
overlaps the filter rejected, since a filter that silently ate every hit would
otherwise look the same as one that worked.


## The damage formula

`AppCharacterEnemy::Damage` @ `0x2b2b00` computes it in one run at
`0x2b3418`..`0x2b34a0`:

```
def_eff = weak_to_this_attack ? defence / 4 : defence   // asr #2, +3 bias
base    = (attack - def_eff + magic) * (gauge + 16000) / 16000
damage  = base + base * GameRandom(25) / 100            // +0..24%
damage  = max(1, damage)                                // cmp #1 / csinc
```

- `attack` is `w28` and `defence` is `w27`, loaded from the enemy actor's own
  `+0x3a30` -- the same block that holds its EXP at `+0x3a34` and money at
  `+0x3a38`.
- `magic` is the attack param's `+0x34`, which
  `SetCollisionAttackParam` @ `0x2b7e8c` fills from `GameParameter+0x140`, the
  effective **wisdom** stat. The physical attack sits beside it at `+0x30`.
- `gauge` is the attack param's `+0x2c`, read from `oG+0x1b8`. An empty meter
  is 1x and a full one (16000) is 2x. `ModeGame::Render` draws it and
  `UseInventoryFunc` fills it to 16000, so 16000 is the top.
- The **quartered defence** is conditional: four bytes at the enemy record's
  `+0xa5c`..`+0xa5f` each pair with an attack-type id, and a match quarters the
  defence. The ids are not decoded.
- The **floor at 1** is the engine's, which retires an old note calling it a
  port choice.
- A global byte (at `+0xc35a` off the same base as several other debug flags)
  replaces the computed damage with the target's **current HP** -- a one-hit
  kill switch. Not implemented.

On the kill, `0x2b34b4`..`0x2b3524` rolls the rewards the same way:

```
AddEXP( exp   + exp   * GameRandom(11) / 100 )
AddRC ( money + money * GameRandom(11) / 100 )
```

And if the attack param's `+0x28` is `0x6d` (109), the player's HP at
`GameParameter+0x114` gains `damage / 4` -- a life-steal attack type. Not
implemented; the port has no attack-type ids.

`GameRandom` @ `0x3da480` is **not** reversed. The port uses a fixed-seed
`mt19937` with the same range contract, so the SHAPE of every roll is the
engine's and the sequence is not; headless runs stay reproducible.
`--combat-selftest` checks each term of the formula separately, so a simpler
formula cannot pass: magic must add, a full gauge must double, half a gauge must
be 1.5x, a weakness must quarter the defence, and a hopeless attack must still
land 1.


## How the charge meter fills

`ModeGame::Process` @ `0x2d5e88` drives it, at `0x2d7828`..`0x2d788c`:

```
if (player->IsUpSpGauge()) {                       // vtable +0x470
    dt_ms = frame_delta_ms
    will  = oG[+0x1a4]                             // GameParameter+0x144, effective WILL
    gauge = oG[+0x1b8]
    oG[+0x1b8] = (float)(will * dt_ms * 100) / 1000.0f * (float)rate + gauge
}
if (flag) oG[+0x1b8] = 16000.0f                    // fill it outright
```

So the meter gains **`will * 100 * rate` per second**, and 16000 is full -- which
makes `will` the "limit gauge build speed" that `SYS_HELP_LEVELUP_WISEMAN`
promises the Sage regimen improves. That is the third independent confirmation
of the stat mapping, after `Update`'s arithmetic and the level-up rows.

`AppCharacterBase::_StepMotion` @ `0x2af304` stores zero into it, so a swing
spends the meter. `ModeGame::UseInventoryFunc` @ `0x2deb78` writes 16000.0
outright, so an item can fill it.

**Not implemented, and this is why.** `AppCharacterPlayer::IsUpSpGauge` @
`0x2b6228` decides when the meter may grow at all, and it depends on a debug
flag, a per-character byte at `+0x3aac`, another virtual at slot `+0x248`, and
the current motion id at `+0x9f0`. That gate is not reversed, and `rate`
(`[x25+0x40]`) comes from a struct that is probably the debug settings. Guessing
the gate would swing every hit in the game between 1x and 2x on a rule this
project invented, so the port leaves the meter at the 0 `GameParameter::Init`
gives a new game -- the neutral 1x -- and `mcf::DamageInput::gauge` is ready for
it the day the gate is read.


## `enemydat.bin`: 408 bytes per enemy, six of them used

`AppCharacterEnemy::SetEnemyId` @ `0x2b2344` settles the record's size and where
it goes: `DataTableGetEnemy(id)`, then one `memcpy` of `0x198` (408) bytes into
the actor at `+0x3a24`. So **record offset R is actor offset R + 0x3a24**, which
is what pins the six fields the port reads:

| record | actor | field |
|---|---|---|
| `+0x00` | `+0x3a24` | id |
| `+0x04` | `+0x3a28` | max HP |
| `+0x08` | `+0x3a2c` | attack |
| `+0x0C` | `+0x3a30` | defence (`AppCharacterEnemy::Damage` reads it here) |
| `+0x10` | `+0x3a34` | EXP |
| `+0x14` | `+0x3a38` | money |

**384 of the 408 bytes are unread by this port.** `SetEnemyId` immediately
distributes several of them into the actor's own fields, which is where enemy
behaviour will come from:

| record | goes to | identified by its consumer |
|---|---|---|
| `+0x51` | (a flag) | sets actor `+0xcbc` to `{0.0f, 180.0f}` -- unidentified |
| `+0x53` | (a flag) | clears bit 4 of the collision target mask |
| `+0x58` | actor `+0xc6c` | read by `UpdateSlanted` -- slope handling |
| `+0x5c` | — | float, passed to vtable slot `+0x1e8` -- unidentified |
| `+0x60` | actor `+0xaf8` | **shadow size** (`GetShadowSize` @ `0x2b1f9c`) |
| `+0x64` | actor `+0x3930` | **AI type** (the switch in `UpdateAI`) |
| `+0x68` | actor `+0xc64` | **move speed** (`UpdateAI`, `_UpdateGroundAttribute`) |
| `+0x6c` | actor `+0x3938` | **thrown weapon id** (`WeaponThrow` @ `0x2aec00`) |

`AppCharacterBase::UpdateAI` @ `0x2a894c` switches **27 ways** on the AI type --
`cmp w8, #0x1a` then `b.hi` to the default, dispatched through a `ldrh` jump
table at `.rodata 0x9dfb0`. 25 of the 27 cases are used by the shipping table,
type 0 by 59 of the 107 enemies. Those 27 behaviours are the enemy AI and none
of them is reversed.

`tools/asset/enemydat.py` censuses the table, and each check can fail: every
field it claims must VARY across the records (a constant would read the same
whether the offset were right or wrong), the AI types must fall inside the
switch's 0..26, and the speeds must be plausible. What it reports:

| field | values |
|---|---|
| ids | unique and ascending in three blocks: 0..73, 100..123, 201..209 |
| move speed | 12 (34 enemies) and 24 (43) dominate; 9 enemies are 0 and never move |
| shadow size | 10 (82), 25 (15), 15 (1), 0 (9) |
| thrown weapon | 41 of 107 carry one, ids 123..156 |

The port now moves each enemy at its own speed instead of an invented 30, and
leaves an enemy with speed 0 standing. The AI type is carried on the actor and
**not acted on**, because acting on it means writing 27 behaviours that have not
been read.

This remains the gap that matters: enemy movement is still a placeholder that
walks an enemy into the player and holds it there, so every enemy attacks as
fast as the player's i-frames allow. Against a level-1 player (19 HP, 7 defence)
a werewolf's 40 attack is 33 damage a hit, and the run ends in seconds. That
number is right; the behaviour producing it is not.


## Enemy AI: the architecture, and how much of it is read

`AppCharacterBase::UpdateAI` @ `0x2a894c` is the enemy AI, and it is large. Its
shape, established:

1. It switches **27 ways** on the enemy's AI type (`enemydat +0x64`, actor
   `+0x3930`): `cmp w8, #0x1a`, `b.hi` to the default, dispatch through a `ldrh`
   jump table at `.rodata 0x9dfb0`. Case bodies start at `0x2a8ea8`.
2. Each case is short. It sets a **movement mode** at actor `+0x3934` and a
   **byte at actor `+0xc7b`**, sometimes conditionally on a state
   word at actor `+0x38e8`, and several cases roll `GameRandom` to decide.
   `+0x3894` is **not** a probability, as an earlier version of this said: it is
   an index into the AI parameter block described below (`smaddl` by 140).
3. A **second switch**, at `0x2a95b4`, branches on that movement mode (0..5 and
   above) and does the actual work -- it reaches into `AppEventBoxServer::Enum`
   and the route tables (`MakeRouteTable`, `MakeShortRoute`) from there.

So "27 behaviours" is really *27 small selectors over a handful of movement
modes*, which is far more tractable than it first looked. The mode switch is
where the work is.

The dispatch itself is now located exactly: `ldr w8, [x19, #0x3930]` (the AI
type), `cmp w8, #0x1a` with `b.hi` to the mode switch, then a jump table at
`.rodata` `0x9dfb0` with branch base `0x2a8ea8` — so cases 0..26, 27 distinct
handlers, and an out-of-range type falls through to the mode switch unchanged.

**The AI type does not map statically to a mode**, and the 27 handlers fall into
two shapes. Four are unconditional; the other 23 share one template.

*The simple four* set a mode outright:

| type | enemies | mode |
|---|---|---|
| 0 | 59 | 1 |
| 1 | 3 | 2 |
| 2 | 8 | 0 when the state is 0, else another path |
| 4 | 1 | 0, and clears the state |

*The other 23* run one idiom, parameterised three ways — the `+0xc7b` byte, the
mode on a failed roll, and the mode on a passed roll:

```
+0xc7b := 0 or 1                       per type
if (state == 0)                 -> mode 0            tail 0x2a9534
else if (GameRandom(100) >= prob) -> mode 1 or 2     tails 0x2a953c / 0x2a94c0
else                            -> a per-type mode
```

with `prob` = `rec[+0x88]` = enemy `+0xe8`.

| type | fail | pass | | type | fail | pass |
|---|---|---|---|---|---|---|
| 3 | 1 | 3 | | 15 | 2 | 9 |
| 5 | 1 | 4 | | 16 | 2 | 5 or 9 |
| 6 | 2 | 5 | | 17 | 1 | 10 |
| 7 | 1 | 6 | | 18 | 2 | 10 |
| 8 | 1 | 6 | | 19 | 2 | 5 or 10 |
| 9 | 2 | 7 | | 20 | 2 | 9 or 10 |
| 10 | 2 | 5 or 7 | | 21 | 2 | 11 |
| 11 | 1 | 8 | | 22 | 2 | 10 |
| 12 | 2 | 8 | | 23 | 2 | 3-way |
| 13 | 1 | 5 | | 24 | 2 | 5/9/10 by table |
| 14 | 1 | 9 | | 25 | 1 | 9 |
| | | | | 26 | 1 | 5 |

The seven that no scanner resolved all share one reason: **they have no single
pass mode.** On a passed roll they roll *again* and pick among several:

| type | second roll | outcome |
|---|---|---|
| 10 | `GameRandom(2)` | 0 → mode 5, else mode 7 |
| 16 | `GameRandom(2)` | 0 → mode 5, else mode 9 |
| 19 | `GameRandom(2)` | 0 → mode 5, else mode 10 |
| 20 | `GameRandom(2)` | 0 → mode 9, else mode 10 (via `cinc`) |
| 23 | `GameRandom(3)` | a 3-way branch, 0 → mode 5 |
| 24 | `GameRandom(3)` | indexes a **`.rodata` table at `0x9e5b8` = {5, 9, 10}** |
| 14 | — | inverts the test (`b.lt`, not `b.ge`): pass → mode 9, fail → mode 1 |

Types 10, 16 and 19 share the tail at `0x2a9450` — `w9 = 5; csel w8, w9, w8, eq`
— which is where the "0 → mode 5" comes from. Type 24 is the only case whose
mode is *data*: three `int32`s at `.rodata` `0x9e5b8`, and the word after them
is unrelated, confirming the table is exactly three entries.

So the map is complete: **all 27 handlers accounted for.** The extraction failed
on these seven not through another blind spot but because the question was
malformed — asking for "the pass mode" of a handler that has two.



This table cost four wrong extractions, each of which looked complete:

1. a fall-through trace gave a tidy 27/27 with "13 types setting mode 6" —
   refused because 24 of 27 crossed a conditional first;
2. a template matcher then reported 27/27 conformance, including type 0 whose
   handler is three instructions with no roll at all — its window **bled into
   the next handler**, since the handlers are adjacent;
3. bounding each block at the next handler start fixed that, but the pass mode
   for type 9 came out 6 against a hand-read 7;
4. because `0x2a93b4` is a **shared store entered with the value preloaded** —
   type 9 does `mov w8, #7; b 0x2a93b4`. Tracing the store's register
   *backwards* reads whichever `mov` physically precedes it, which belongs to
   another path entirely.

The fix was to simulate the register **forward** along the actual path. That
version agrees with all seven hand-read cases and says "value not seen on this
path" for six others instead of guessing. So mode 6 is the pass mode for types
7 and 8 only; the original "13 types" was entirely an artifact of the shared
tail.

Four cases read instruction by instruction:

| type | enemies | what the case does |
|---|---|---|
| 0 | 59 | mode := 1, `+0xc7b` := 0, unconditionally |
| 1 | 3 | mode := 2, `+0xc7b` := 1, unconditionally |
| 2 | 8 | if state `+0x38e8` == 0: mode := 0, `+0xc7b` := 0; else elsewhere |
| 4 | 1 | mode := 0, state := 0, `+0xc7b` := 0 |

**`+0xc7b` is NOT a "chase flag".** An earlier version of this note called it
one, on the strength of nothing but the shape of the cases. Its two readers, at
`0x2aadd0` and `0x2ab38c`, both sit inside a motion-driven state machine keyed on
motion ids 10, 11 and 12 and on `LerpNf` interpolation, and one of them chooses
between two motion changes through vtable slot `+0x220`. What the byte means is
**not established**; naming it was an invention and is retracted.

**The other 23 are not read.** A quick script that tried to extract all 27
mechanically disagreed with the four above, so its table is not published: an
extractor that contradicts a hand reading is a broken instrument, not a
shortcut. Nothing here is implemented -- the port still runs its placeholder,
which walks an enemy into the player and holds it there.

### `UpdateAI`'s real extent, and a bound that was wrong

`AppCharacterBase::UpdateAI` runs `0x2a894c`..`0x2abb54` -- **12,808 bytes**,
3,202 instructions. Several scans in this file were originally run with a
working bound of `0x2aa400`, which is 70% of the function; that was an arbitrary
window, not a measured end. Re-running the affected scans against the real span:

* the state-word store count is **unchanged at 13**, so everything concluded
  from it stands;
* but all three reads of the move speed at actor `+0xc64` live in the cut-off
  region, and they are what the next section is about.

The lesson is the recurring one -- the truncated bound did not announce itself,
and only recounting against the real end showed which conclusions survived.

### Modes 9 and 11, and a timer that is not rolled

The dispatch table below covers modes 0..8 and lumps everything above into
`0x2a9968`. That branch resolves further: `0x2aa720` reloads the mode and tests
it against **11** and **9**, so the mode space is larger than 0..8.

Mode 9 is a wander, and it is set up immediately before:

```
+0x3948 = <a distance>
+0x394c = +1.0f
if (GameRandom(...) == 0) +0x394c = -1.0f      ; 0xbf800000
```

so `+0x394c` is a random direction SIGN and `+0x3948` the distance to cover.
Then the state timer is **computed rather than rolled**:

```
t = (+0x3948 * 30.0) / (s14 * move_speed * +0xc68 * +0x3918)
+0x38ec = +0x38f0 = (int)t
```

i.e. distance / speed = time in frames, written into the very same timer fields
the `{base, range}` pair fills elsewhere. `s14` is the 1.0/2.0 factor set at
`0x2a9d10` from `+0xc4c`, and the 30.0 is the chip.

#### Chasing the distance timer's three inputs

Of the three, one resolves and one is a well-bounded dead end.

**`+0x3918` = 1.0f**, written once in `AppCharacterBase`'s constructor
(`mov w26, #0x3f800000` at `0x2a63a0`). A default multiplier.

**`+0x3948`** is written only inside `UpdateAI` itself, so it is local to the
wander setup rather than sourced from data.

**`+0xc68` has no writer that could be found**, and the search is worth stating
with its denominators because a bare "not found" is indistinguishable from not
looking:

* no plain `str` at a fixed offset anywhere in the binary writes it, except a
  `strb wzr` in `AppCharacterBase::Dead` — a *byte* zero, while all ten reads
  are `ldr s`, so that store is not the source;
* it is not a script slot: `ChrSetData`'s jump table was extracted in full
  (table at `.rodata` `0xa06a2`, branch base `0x2c9ab0`, 141 slots, 42 distinct
  handlers) and **none** of the 141 maps to `+0xc68`;
* the obvious hypothesis — that it is a multiplier derived from the adjacent
  WALKMODE field at `+0xc60` — was **tested and falsified**: slot 124's handler
  at `0x2c9e04` writes `+0xc60` and branches away without touching `+0xc68`;
* `AppCharacterBase::Update` (`0x2abce0`, 1,390 instructions) does not write it;
* nor does any resolvable register-indexed store — the form that hides from an
  offset scan, and the one that writes the `+0x377c` block. Across all **313**
  character-class functions there are 41 such stores; 25 resolve to a constant
  index and none covers `+0xc68`. **The residual is the other 16**, whose index
  register could not be traced, and that is where it must be.

So it is computed somewhere else, most likely per-frame, and the distance-driven
timer stays unimplementable rather than being filled in with a guessed 1.0.

That table extraction did validate itself on the way past. It says slot 123 maps
to `+0xc64`; `sk1.lua` — the game's own script prelude, in Shift-JIS —
documents slot 123 as `MOVESPEED`, "キャラの移動値"; and `+0xc64` is already
established as where `enemydat`'s move speed lands. Three independent sources,
one answer. The rest of the extracted map is **provisional**: the extractor takes
the first fixed-offset store within six instructions of a handler, which can pick
up a neighbouring arm, so no entry should be used without the same kind of
cross-check.

#### The movement equation, which is what the chase actually turned up

`0x2ab040` and `0x2ab110` are where an enemy is displaced, and both compute the
same product:

```
Normalize(dir)
step = move_speed(+0xc64) * (+0xc68) * (+0x3918) * s14 * dt
ScaleVector(dir, dir, step)
```

The identical product is the divisor in mode 9's timer, which is the consistency
check on both readings: `time = distance / speed`.

With `+0x3918 = 1.0f` (the constructor default, verified) and `s14 = 1.0` (its
default; `0x2a9d10` sets 2.0 only when `+0xc4c == 1`), this reduces to
`move_speed * (+0xc68) * dt`. **The port's `move_speed * dt` is therefore the
engine's own equation with the remaining factors at their defaults**, not an
approximation of it — so the port is the special case rather than wrong, and the
two extra factors are named here for when they can be sourced.

Neither site guards against `+0xc68` being zero, and here it is a MULTIPLIER, so
a zero would freeze every enemy. Enemies do move in the shipping game, so the
field is reliably nonzero and something must write it. Nothing found does — see
the bounded search below — which localises the gap to the 16 register-indexed
stores whose index could not be resolved.

#### A flaw in how "who writes actor+X" was being asked

These scans matched an instruction anywhere in the binary by register and
offset, without checking that the register held an `AppCharacterBase`. That is
wrong, and it produced a concrete false positive: a `str q22, [x19, #0xc60]`
that appeared to write `+0xc68` turned out to be inside `_vp_psy_init` — libvorbis
code, a different `x19` entirely.

Every offset the AI conclusions rest on was re-audited by containing function:
`+0x38e8` 47/47 references in character code, `+0x3894` 36/36, `+0x3918` 4/4,
`+0x3948` 6/6. Clean, so those conclusions stand. `+0xc64` has 24 references of
which 13 are outside character-named functions, but those are `ChrGetData`,
`ChrSetData` and `ModeGame::AddParty` — legitimate, just not name-matched.

This matters for the port: **the `{base, range}` durations are not the only
source of a state's length**. The port currently implements only the rolled
form, which is correct for the states it was verified against but is not the
whole mechanism. The distance-driven form is recorded and NOT implemented,
because what feeds `+0x3948`, `+0xc68` and `+0x3918` is not read.

### The mode switch at `0x2a95b4`, dispatch only

The dispatch is a chain of signed compares on the mode word at actor `+0x3934`,
and it groups the modes rather than giving each its own body:

| mode | goes to | note |
|---|---|---|
| < 0 | `0x2a99dc` | the exit. `cmp w8, #3` / `b.hs` after the mode is known to be `<= 2` can only fire on a negative read as unsigned |
| 0, 1, 2 | `0x2a95d0` | one shared body — **the one 59 of 107 enemies reach**, since AI type 0 sets mode 1 |
| 3, 4, 5 | `0x2a98b0` | |
| 6, 7 | `0x2a9980` | `sub w9, w8, #6` / `cmp w9, #2` / `b.lo` |
| 8 | `0x2a9658` | |
| > 8 | `0x2a9968` | |

The shared body for modes 0..2 opens by resetting state — `strh #0x101` into
`+0xc7f` sets the two bytes at `+0xc7f` and `+0xc80` to 1, `+0xcc5` := 0,
`+0x36f4` := 0.0f, `+0x36fc` := 1.0f — and additionally clears `+0xc7c` for
modes 0 and 1 only, which is the one place the three modes diverge. It then
enumerates the room's event boxes through `AppEventBoxServer::EnumInit` / `Enum`.

The literals 30.0, 15.0 and -10.0 are loaded into callee-saved registers at
`0x2a9618`, immediately before the branch into the loop. An earlier version of
this note called 30 a chip-scaled *reach* for the loop. That was an invention:
across `0x2a9618`..`0x2a9cd4` those registers are only ever **reassigned**
(`0x2a96b0`, `0x2a97ac`, `0x2a9810`) and never read, so the loop body does not
consume them. What they are for is not established.

### What the event-box loop does

Not pathing — a **floor-type state machine**. Per box:

```
mask = box->vtable[0x40]()          // a type mask
if (!(mask & 3)) continue
if (!box->vtable[0x20](actor_pos))  // containment test
    continue
```

The two mask bits are the two directions of one transition on
`CharacterBase::GetFloorType` / `SetFloorType`:

| bit | condition | effect |
|---|---|---|
| 0 | floor type is 0, and bit 31 of `+0x3910` is set | `SetFloorType(1)`, then `vtable[0x2f0](0.0f)` |
| 1 | floor type is 1, and the float at `+0x38c0` is > 0 | `SetFloorType(0)`, then `vtable[0x1c8]()` and `vtable[0x2f0](1.0f)` |

Bit 1 also has an earlier arm: when the floor type is still 0 and either bit 31
of `+0x3910` is set or the float at `+0x38c4` is > 0, it branches to `0x2a9a00`
instead. So `+0x38c0` and `+0x38c4` are the two gates on the transition, one per
direction.

This reframes the older note that described the mode switch as reaching "event
boxes and route tables" as though it were navigation. For modes 0..2 the event
boxes are **terrain regions**, and the body's job is entering and leaving them.
What `vtable[0x2f0]` and `vtable[0x1c8]` do is **not read** — the 0.0f / 1.0f
pairing invites reading the first as a speed multiplier, which is exactly the
kind of guess this file exists to keep out.

### `+0x3910` is a countdown, and what "expired" gated

At the loop's exit, `0x2a9cd4` does `+0x3910 -= 1` unconditionally, once per
`UpdateAI` call. So it is a **frame countdown**, and the `tbnz w8, #0x1f` the
floor-type transitions test is the sign bit — the transitions fire when the
timer has run out, not on an opaque flag.

Directly after, when the floor type is 1, the mode at `+0x3934` is rewritten
from the state word: `mode := (state != 0) ? 1 : 0`. This is why mode 1 is so
common at runtime as well as in the table — 59 of 107 enemies start there by AI
type, and the terrain path puts them back.

### The AI parameter block at actor `+0x377c`

The state timer is not a constant. At `0x2a9e84`:

```
rec  = actor + 0x377c + actor->[0x3894] * 140      // smaddl w8, #0x8c
pair = rec + actor->[0x38e8] * 8                   // indexed by the state word
{base, range} = *(int32[2])pair
t = base + GameRandom(range)
actor->[0x38ec] = actor->[0x38f0] = t              // clamped against w23
```

So each enemy carries its **own** parameter block — `x20 = x19 + 0x377c`, an
actor offset, not a global table — laid out as 140-byte records selected by
`+0x3894`, each holding 8-byte `{base, range}` pairs selected by the current
state. That pair is what makes a state last a randomized number of frames.

The block **is** per-enemy data, from `enemydat.bin`. An earlier version of this
section said it was not, reasoning that `SetEnemyId`'s `memcpy` lands at
`+0x3a24`, past the block. That is true of *that* memcpy and false as a
conclusion: there is a second path out of the same file.

`AppCharacterBase::SetAITblFromEnemyTbl` @ `0x2a6cb0` calls
`DataTableGetEnemy(id)` and copies the record's `+0x80`..`+0x194` into
`+0x377c`..`+0x3894` -- exactly the two 140-byte records -- and seeds a
`GameRandom` value into `+0x38f8`. The copy accounts for **53 of 53** non-stack
stores in the function, so the block is filled entirely from this file. That is
276 of the record's 408 bytes: **two thirds of an enemy record is AI
configuration**, and it was sitting unparsed behind a wrong conclusion.

The block mixes `int32` and `float32`, and which is which comes from the engine
-- whether the copy uses `ldr w` or `ldr s` -- rather than from reading bit
patterns. Six slots are floats (`+0xf8`, `+0x100`, `+0x108`, `+0x184`, `+0x18c`,
`+0x194`). `enemydat.py --ai` checks that split against all 107 records in both
directions: all six decode as plausible floats in every nonzero record, and none
of the 64 int slots do. 46 of the 57 source offsets are typed by a direct load;
the other 11 arrive through SIMD moves and are reported as untyped rather than
assumed.

The dominant int values across the corpus are 1, 120, 2, 3, 30, 60, 240, 180 --
frame counts at 30 fps, matching the shape of the constructor defaults.

#### The 24-byte state descriptor

The block is not a flat bag of slots. It is built from a repeating **24-byte
state descriptor**:

```
+0x00 .. +0x0c   four int32 parameters
+0x10 .. +0x14   {base, range} -- the state's duration in frames,
                 realised as base + GameRandom(range)
```

Four descriptors per 140-byte record, two records, and the eight bases are
pinned by `SetAITblFromEnemyTbl` copying each offset into its matching actor
slot rather than by pattern-matching the bytes:

| record | state 0 | state 1 | state 2 | state 3 |
|---|---|---|---|---|
| 0 | `+0x80` | `+0x98` | `+0xb0` | `+0xc8` |
| 1 | `+0x10c` | `+0x124` | `+0x13c` | `+0x154` |

The stride is `0x18` throughout, and the two groups were derived independently —
record 0's from the four-word copies at `+0x80/+0x98/+0xb0/+0xc8` plus the SIMD
pair stores, record 1's from the copies at `+0x10c`..`+0x160` — and agree.

Enemy 0 reads out as: state 0 `params (1,3,0,0) timer 60+rand(30)`, state 1
`(3,1,0,0) 100+rand(50)`, state 3 `(0,0,0,0) 120+rand(120)`.

#### `enemydat +0xe8` is an AI probability, in percent

Record `+0x88` — from enemy record `+0xe8` — is the **most-read field of the AI
block**, 25 reads off the record pointer in `UpdateAI`, and it is a percentage.
At `0x2a90bc` the engine does exactly:

```
w0 = GameRandom(100)
rec = actor+0x377c + [+0x3894]*140
if (w0 < rec[+0x88]) { mode := 9; state := 1 }     ; 9 is the wander
```

**What it gates varies by AI type**, and the line above is type 15's variant.
The commit that introduced this section described it as "the chance of entering
the wander", which is true for type 15 and over-general: type 7's handler at
`0x2a9138` uses the identical roll to reach **mode 6** instead. So the field is
the probability of taking the case's *alternative* branch, not of one fixed mode.

The roll sits inside an idiom several handlers share verbatim:

```
if (state == 0)                     -> mode 0        (tail 0x2a9534)
else if (GameRandom(100) >= prob)   -> mode 1        (tail 0x2a953c)
else                                -> mode 6, state := 1   (tail 0x2a93a4)
```

with `prob` = `rec[+0x88]` = enemy `+0xe8`. Types 7 and 15 both use it; 15 swaps
the last arm for mode 9. All three tails fall into one epilogue at `0x2a9544`
that resets `+0xc7f`/`+0xc80` to 1, `+0xcc5` to 0 and `+0x36fc` to 1.0f — the
same reset the mode-switch body performs.

This also explains the mode-6 convergence honestly: 13 types appeared to "set
mode 6" because `0x2a93a4` is a **shared probabilistic tail**, reached only when
the roll passes. Mode 6 really is the common alternative; attributing it to those
types unconditionally was what made the earlier table wrong.

The corpus check is deliberately not "is it in 0..100", which a small enum would
also pass. What identifies it is the **vocabulary**: across all 107 enemies the
only values that occur are `0, 10, 20, 30, 60, 80, 90, 95, 100` — round
designer-authored percentages, bounded exactly at 100. 32 of 107 enemies carry a
nonzero one, and the split tracks the AI types: types 0..5 are almost all zero
(type 0 alone is 59 enemies), while every type from 6 upward carries a real
probability.

For contrast, the neighbouring trailing fields are clearly not percentages —
enemy `+0x104` spans 0..4 (a small enum), `+0xf4` is 0..1 (a flag), and `+0xfc`
is 0 in every record, which makes it unidentifiable rather than understood.

#### The four params are transition weights

`UpdateAI` @ `0x2a8d50` picks the next state by **weighted roulette** over the
current state's four params:

```
q0   = ldr q0, [rec + state*0x10 + 0x20]   ; the four weights, as a vector
sum  = addv s0, v0.4s
if (sum < 1) -> no transition at all; the state stands
roll = GameRandom(sum)
roll -= w0;  if (roll < 0) next = 0
roll -= w1;  if (roll < 0) next = 1
roll -= w2;  if (roll < 0) next = 2
next = (roll < w3) ? 3 : unchanged
```

So param *i* of state *s* is the weight of the transition `s -> i`, and a
descriptor whose weights sum to zero is one the enemy does not leave by this
path. That is why the params are always small integers: they are weights, and
the corpus bears it out — every sum is a small non-negative int, 520 of 856
descriptors roll for a next state and 336 stand.

Enemy 0, the commonest AI type, reads out as a wander/pause cycle:

| state | transitions | duration |
|---|---|---|
| 0 | `->0` 25%, `->1` 75% | 60 + rand(30) frames |
| 1 | `->0` 75%, `->1` 25% | 100 + rand(50) frames |
| 2, 3 | none (sum 0) | 120 + rand(120) frames |

There are **77 distinct state machines** across the 214 descriptor sets.

**All four states are real, and an earlier note here was wrong.** That note said
the state word is only ever stored as 0, 1 or 3, from a scan of the store sites.
The four branches above converge on a single store at `0x2a8e0c`, so a linear
backward walk from it only ever sees the nearest `mov`, which is `#3`; the
branch that sets `#2` at `0x2a8dec` is invisible to that method. The corpus
disagreed with the scan — 167 of 214 records carry a populated state-2
descriptor — and the corpus was right. The scan was recorded as incomplete
rather than as proof, which is the only reason this resolved cleanly.

The descriptor check in `enemydat.py` began as "a timer is 1..3600 frames" and a
deliberate sabotage — shifting a base by 4 — **passed it**, because that stays
true after a shift. The discriminator was then measured over the corpus instead
of assumed: at the correct offsets 0 of 2688 params exceed 20 and no timer base
lands in 1..14, while shifts of +4, -4 and +8 put 671, 416 and 1229 params over
20. Both conditions are now the test, and all three shifts fail it.

What the base constructor writes there is only a **default**, overwritten for
any enemy:

| destination | source | `{base, range}` pairs | at 30 fps |
|---|---|---|---|
| `+0x377c` | `.rodata` `0x9ddd0` | `{120, 120}`, `{120, 10}` | 4–8 s, 4–4.3 s |
| `+0x378c` | `.rodata` `0x9dc80` | `{240, 60}`, `{120, 120}` | 8–10 s, 4–8 s |

(`.rodata` is at VMA `0x8ca90` == file offset, so these are read directly.)

**There are exactly two records.** The index is a toggle, not a counter:
`0x2a8d2c` writes `1 - old` into `+0x3894`, so its domain is `{0, 1}`. That
closes the arithmetic — `0x377c + 2*140 == 0x3894`, i.e. the block is 280 bytes
and the index field sits immediately after it. The same site resets the state
and its timer together with one store, `+0x38e8 := 0` and `+0x38ec := -1`.

Within a record, reads are observed at `+0x80` (int, `0x2a8ba4`) and `+0x84`
(float, `0x2a8d10`, compared to decide the toggle). With a stride of 140 that
leaves `0x00..0x7f` for the `{base, range}` pairs — **at most 16** of them,
which bounds the number of AI states. The 16 is arithmetic from the stride, not
an observed count, and is recorded as a bound rather than a fact.

`0x2b4a54` is a second class initialiser writing the same offset from different
constants; which character classes take which table is **not read**.

Mode 8's body is the only other one read: it differences two counters at
`+0x38c8` and `+0x38cc`, writes the `<=` result as a byte into a *different*
object at its `+0x8f4`, and on first entry latches the difference into `+0x38e0`,
clears `+0x38dc` and sets `+0xcc5`.

What the loop body at `0x2a9ac4` actually does with each event box is **not
read**, and that is where the behaviour lives. Nothing here is implemented; the
port still runs its placeholder.


## Game over

`ModeGame::Process_GameOver` @ `0x2de964` is a three-step state machine on a
counter at `ModeGame+0xc160`:

1. `GameBgmPlay(3)`, then `SYS_GAMEOVER_MSG` through `GetStringResource` ->
   `CnvFormatString` -> `SetMessageWindow`. The Japanese line is
   `"@H　は　力尽きた…"`, so without the control-code expansion it cannot even
   name the player.
2. once the message window has closed:
   `SetDispFade(EFADETYPE 1, 800 ms, 0, 0, 0)` -- fade to black.
3. once the fade is done: `SetNextMode(5)`.

The port does all of it except step 3's destination: it has no mode system and
no title screen, so it ends the run and says so rather than inventing a screen.
Verified in both languages -- "You fall to the ground..." and
"ヒーロー　は　力尽きた…", the latter with `@H` expanded from the default hero
name.


## The save format, as far as it is read

`_GameSaveAccess(int)` @ `0x30c820` is the whole serializer, and it is a **flat,
ordered byte stream** -- no encoding, no tags, no lengths. Every field is one
inlined `memcpy` between the save buffer and `oG + <offset>`, with a running
offset in the buffer and a clamp against the buffer's remaining length:

```
size   = min(4, remaining)
memcpy(oG + OFF, buffer + pos, size)     // or the reverse, on save
pos   += size
```

It opens with the two names through `SaveAccessStr` -- `oG+0x68` (hero) and
`oG+0xe8` (girl) -- and then walks a contiguous run of 4-byte fields:

| order | address | GameParameter | field |
|---|---|---|---|
| 1 | `oG+0x168` | `+0x108` | (unidentified) |
| 2 | `oG+0x170` | `+0x110` | level |
| 3 | `oG+0x174` | `+0x114` | HP |
| 4 | `oG+0x178` | `+0x118` | MP |
| 5 | `oG+0x17c` | `+0x11c` | max HP |
| 6 | `oG+0x180` | `+0x120` | max MP |
| 7 | `oG+0x184` | `+0x124` | attack |
| 8 | `oG+0x188` | `+0x128` | defence |
| 9 | `oG+0x18c` | `+0x12c` | EXP |
| 10 | `oG+0x190` | `+0x130` | EXP for next level |
| 11 | `oG+0x194` | `+0x134` | money |
| 12-15 | `oG+0x198`..`+0x1a4` | `+0x138`..`+0x144` | the four effective stats |
| 16-19 | `oG+0x1a8`..`+0x1b4` | `+0x148`..`+0x154` | the four base stats |
| 20 | `oG+0x1b8` | `+0x158` | the charge meter |

That run is exactly the `GameParameter` block decoded from `Init` and `Update`,
in address order, which corroborates that layout from a completely different
function.

**Correction to an earlier version of this note.** It said the fields "stop
being uniform 4-byte copies" after field 20 and "jump backwards into `oG+0x10`,
`+0xd0`, `+0xd8`". They do not. That was an artifact of disassembling past the
end of `_GameSaveAccess` and reading a different function's code as if it were
part of the walk. Keying the extractor on the `csel w21, wSIZE, wREMAIN, ne`
that actually selects the copy size -- rather than guessing which register held
it -- recovers the whole thing cleanly:

| what | address | GameParameter | size |
|---|---|---|---|
| hero name | `oG+0x68` | `+0x08` | `SaveAccessStr` |
| girl name | `oG+0xe8` | `+0x88` | `SaveAccessStr` |
| (unidentified) | `oG+0x168` | `+0x108` | 4 |
| level .. `+0x15c` | `oG+0x170`..`+0x1bc` | `+0x110`..`+0x15c` | 20 x 4 |
| (unidentified) | `oG+0x1c0`..`+0x1c6` | `+0x160`..`+0x166` | 4 x 2 |

That is **92 bytes** of `GameParameter` after the two names -- the contiguous
span `oG+0x168`..`+0x1c7`, minus a 4-byte hole at `oG+0x16c` that is not
serialized at all.

The function contains the walk **twice**, once per direction (save and load),
which is why an offset-ordered scan appears to restart: run 0 covers
`0x30c874`..`0x30ce48` and run 1 `0x30cf04`..`0x30dbcc`. Both list the same
fields in the same order.

### How big the function actually is

`_GameSaveAccess` runs `0x30c820`..`0x312cbc` -- **25,760 bytes**, 6,440
instructions, with exactly one `ret` whose `add sp, sp, #0xe0` matches the
prologue. That matters because the next symbol in the table is `0x313074`, far
past the end, and disassembling "to the next symbol" is what previously produced
a wrong claim about the field walk. The header above is the first 7 KB of it.

Two things make a mechanical scan of this function untrustworthy, and both were
hit before being caught:

* **`x23` is not `oG` throughout.** It is loaded once at `0x30c864` and is `oG`
  for the header walk, but at `0x30df30` it is reassigned (`add x23, x9, #0x1c8`)
  and becomes a cursor into the inventory. Every `x23`-relative offset after that
  address means something different.
* **The buffer-growth paths reload `oG` into a second register** (`x12`) and call
  `MemManagerRealloc` with `remaining + 0x1000`, so the walk resumes from a
  different base. There are 101 such calls against 109 copy sites.

### Inventory

Immediately after the header -- `oG+0x1c8` **is** `GameParameter+0x168`, so the
92-byte boundary is exactly where the inventory begins -- the save walks four
bags that tile `GameParameter+0x168`..`+0x368` with no gaps:

| bag | offset | stride | slots | holds | ends |
|---|---|---|---|---|---|
| items | `+0x168` | 12 | 16 | type 1, ids 1..37 | `0x228` |
| weapons | `+0x228` | 8 | 16 | type 2, ids 101..118 | `0x2a8` |
| armour | `+0x2a8` | 8 | 16 | types 4, 5 and 6 -- helms, armour and accessories **share one bag** | `0x328` |
| magic | `+0x328` | 8 | 8 | type 7, ids 501..508 | `0x368` |

Three independent functions agree, which is the reason to believe it:

* `GameParameter::IsHaveItem` @ `0x2c5678` -- the compiler fully unrolled its
  search, so the offsets it touches *enumerate* every slot.
* `GameParameter::AddItem` @ `0x2c625c` -- unrolled the same way, over the same
  offsets.
* `_GameSaveAccess` itself -- walks bag 0 with `add x25, x25, #0xc` against
  `cmp x25, #0xc0`, i.e. 16 records of 12 bytes, confirming stride and count
  without reference to either accessor.

Magic is not searched at all: `IsHaveItem` addresses it straight off the id with
`add x8, x19, w20, sxtw #3` then `sub x8, x8, #0xc80`, which for id 501 lands on
`+0x328` -- precisely where the armour bag ends. That the arithmetic and the
tiling meet at the same address is the strongest single check on the whole
layout.

A slot is `{id, seq}` (plus a third word in bag 0). **Nothing stacks.** Neither
the item path nor the equipment path compares the id being added against the ids
already held -- both scan for the first slot whose id word is 0 and return false
when all 16 are taken. The second word is the acquisition order, written from a
monotonic counter at `GameParameter+0x368` that the write tail post-increments:

```
stp w20, w0, [x22]        slot->id = id;  slot->seq = *counter
str w8,  [x19, #0x368]    *counter += 1
```

`GameParameter::SearchSlotGetCnt` @ `0x2c66f4` confirms the direction of use: it
searches bag 0 for the slot whose *second* word equals its argument, which is how
the item list walks the bag in pickup order.

`AddItem`'s second parameter is a `bool`, not a count -- the symbol is
`AddItemEib`. It gates the write, so `AddItem(id, false)` is a dry run, which is
what `IsAddItem` @ `0x2cd8b0` uses; the global `AddItem` @ `0x2cd8e4` passes
`true` and recomputes `GameParameter` as `oG+0x60`, confirming that offset again.

Bag 0's third word is written as `DataTableGetItem(id)+0x4` forced to 0 when it
equals 1 -- the item `kind`, whose meaning is still open, so the port models the
field but does not fill it.

Still unread: the map flags, the per-room enemy-dead bits (`ClearRoomEnemyDead`
touches `+0x414`..`+0x438`) and the 8 KB block `Init` memsets at `+0x444`.


## The boot chain: ModeInit to ModeGame

`MainProcess::Initialize` @ `0x2c08d8` seeds `srand(time(0))`, sets the frame
rate with `SiDrawServer::SetFrameParSecond(60)`, and `new`s three singletons —
`ApplicationMode` (0xb8 bytes), `MCFSiLib` (0x30) and **`ApplicationGlobal`
(0x2910 = 10,512 bytes)**. That last one is stored at GOT slot `0xd38`, which
is the pointer this project has been calling `oG` all along: it is an
`ApplicationGlobal`, and `GameParameter` lives at its `+0x60`.

`ApplicationMode::ProcessMain` @ `0x2c133c` is the mode **factory** — it
switches on the pending mode word and `new`s the matching class, so the enum
values are the game's own:

| EMODE | class | size |
|---|---|---|
| 2 | `ModeInit` | — |
| 3 | `ModeCESA` | 792 |
| 4 | `ModeMakerLogo` | 800 |
| 5 | `ModeTitle` | — |
| 6 | `ModeGame` | 55,304 |

`SetNextMode` @ `0x2c0d04` is one instruction, `str w1, [x0, #0x5c]`, so the
chain is read from the argument each mode passes:

```
ModeInit      @0x2f6778 -> 3  ModeCESA
ModeCESA      @0x2d1ef0 -> 4  ModeMakerLogo
ModeMakerLogo @0x2f6a2c -> 5  ModeTitle
ModeTitle     @0x3070bc -> 6  ModeGame
ModeGame                -> 5  from Process_GameOver @0x2dea58,
                              Process_SystemMenu @0x2f4514, and Process itself
```

**Game over returns to the title.** The port previously logged mode 5 as "a mode
this port does not have" — it is `ModeTitle`, and that note is now retired.

The two splash modes are frame-counter state machines: `ModeCESA::Process`
compares a step counter against `0x13`, `0x14`, `0x1e` and `0x28`, advancing at
`0x28`; `ModeMakerLogo` does the same and additionally requires its counter at
`+0x318` to reach `0xb1` — 177 frames, about 2.95s at the engine's 60fps.

The port implements this chain (`src/engine/mode.{h,cpp}`, `--boot`) and
`--mode-selftest` drives it for real rather than asserting a list. It is
**opt-in** rather than the default because the splash modes draw nothing: the
logo artwork has not been located in the archive, and defaulting to six seconds
of black in place of gameplay would be a regression dressed as progress.

NOT REVERSED: what `ModeInit`, `ModeCESA` and `ModeMakerLogo` actually draw, and
`ModeTitle`'s menu — the port takes its "start" branch directly and says so.


### The boot art, and a PNG decoder

Each boot mode names its assets, and the names come from the string literals its
own code references:

| mode | assets |
|---|---|
| `ModeInit` | mounts `sk1/sk1.mpk`, `sk1/sk1bgm.mpk`, `sk1/sk1patch.mpk` |
| `ModeCESA` | `cesa.png` |
| `ModeMakerLogo` | `sk1/sqex%s.png`, `sk1/SE%04d.wav` |
| `ModeTitle` | title logos, and the **name-entry** screen (`SYS_NAMEENTRY_*`, `SYS_DEFAULTNAME_HERO` / `_GIRL`) |

So the title is not just a menu — it is where a new game's hero and girl names
are entered, which is the missing half of the new-game path.

Searching the archive (9,886 entries, stated because a zero here must be
distinguishable from not having looked): `sk1/sqex.png` and `sk1/sqex_high.png`
are present, as are `titlelogo_{en,ja}_{color,mono}[_high].png` and
`sk1/title_000.png`. **`cesa` matches 0 of 9,886** — that screen's art is not in
the archive, and it is not in the assets root either, which holds no PNG at all.

`sk1bgm.mpk` and `sk1patch.mpk` are likewise **not present** in this extracted
tree; only `sk1.mpk` is. The port never referenced them, which turns out to be
harmless here, but it is recorded because a patch archive that did ship would
override entries and silently change what the port loads.

Every boot PNG is **8-bit RGBA, non-interlaced**, so `src/mcf/png.cpp` decodes
exactly that and refuses anything else by name. It carries its own inflate,
because the project has no zlib — `WritePng` emits only stored blocks, so
nothing in the tree could read a real deflate stream.

The decoder is cross-checked against an independent implementation rather than
itself: `tools/asset/png_check.py` decodes the same four files through Python's
`zlib` and prints the same rolling hash. All four agree byte for byte, over 2.6
million pixels. `--png-selftest` additionally feeds six inputs that MUST be
refused — empty, bad signature, truncated mid-IDAT, palette, interlaced and
16-bit — because a decoder that never says no would "succeed" on anything.


### Drawing the boot screens

`--boot` now draws the real art: `ModeMakerLogo` shows `sk1/sqex.png` and
`ModeTitle` shows the per-language title logo, both decoded by the port's own
PNG decoder and uploaded as RGBA textures. They are aspect-fit rather than
stretched, because the art is authored at 960x544 and the window is not.

`ModeCESA` stays blank, and that is the honest outcome rather than a bug: its
`cesa.png` is in neither the archive nor the assets root.

`ModeTitle` waits for the player, as `ModeTitle::Process` does — it advances on
a choice, not a timer, so the port does not invent a duration. Headless and
`--warmup` runs auto-advance since nobody is there to press anything. Only the
"start" branch is taken; the menu and the name entry the same class carries are
not reversed.

Verifying this needed a real diagnostic, and the first one lied. `--screenshot`
counts gameplay frames, so it never captures a splash screen at all; `--shot-mode
NAME` was added to capture the named mode and exit. Then the first pixel check
counted "non-black" pixels and reported all three screens as ~100% covered —
including CESA, which draws nothing — because the port clears to (26, 28, 36),
not black, so the background counted as content. Measuring the pixels that
*differ from the clear colour* separates the classes properly:

| mode | frame differing from the clear colour |
|---|---|
| `ModeCESA` | 0% — nothing drawn, as expected |
| `ModeMakerLogo` | 2% |
| `ModeTitle` | 6% |

Those magnitudes are consistent with the sources: `sqex.png` is 5% opaque and
the title logo 11%, letterboxed into a square window.


### ModeTitle's own state machine

`ModeTitle::Process` @ `0x306670` is 6,856 bytes and runs **22 sub-states** off a
step word at `this+0x318`, dispatched through a jump table at `.rodata`
`0xbd004` with branch base `0x306718`. The states advance each other with
`ApplicationMode::SetNextSubMode` @ `0x2c0d0c` (one instruction,
`str w1, [x0, #0x60]`), which is a second, finer machine sitting inside the
mode machine.

What the states do, by the calls in each handler's own block:

| state | does |
|---|---|
| 0 | `ModeTitle::LoadTitleLogo` |
| 1, 14, 15 | fades — `SetDispFade` / `IsDispFade` / `DispFadeFinish` |
| 3 | **`GameBgmPlay(1)`** — the title theme is track 1 |
| 5 | `GameSePlay`, fade clear |
| 7, 12 | display sizing — `ApplicationDispH`, `VirtualDispW`, `ConvXPosAppWithPadding` |
| 10 | `ModeTitle::LoadOpeningFont`, `FontTexSetDeviceMake` |
| 13 | **`GameSaveDataHeaderLoad`**, `GameSaveDataIsFinAsync` — the Continue / New Game branch |

So the title loads the save header to decide what to offer, which is the entry
point to the save format already documented above.

### Name entry

The same class carries the new-game name entry. The strings resolve to:

| key | en | ja |
|---|---|---|
| `SYS_DEFAULTNAME_HERO` | `Sumo` | ヒーロー |
| `SYS_DEFAULTNAME_GIRL` | `Fuji` | ヒロイン |
| `SYS_NAMEENTRY_INFO_2` | "Names may consist of up to 8 letters or numbers." | (lists the permitted kana) |

so the limit is **8 characters**, and the defaults are the series' own Sumo and
Fuji. The port already sources both from `SYS_DEFAULTNAME_*` rather than
hardcoding them — checked rather than assumed, and it was already right.

NOT REVERSED: the menu layout, the name-entry UI, and which sub-state runs it.


### The new-game path

`GameParameter::Init` is called from exactly three places: `ModeGame`'s
constructor and `ModeTitle::Process` twice. The title's is the new game, and it
is short:

```
GameParameter::Init()                                          ; oG+0x60
strcpy(oG+0x68, GetStringResource("SYS_DEFAULTNAME_HERO"), 128)
strcpy(oG+0xe8, GetStringResource("SYS_DEFAULTNAME_GIRL"), 128)
ApplicationMode::SkipAtNextFrame()
```

This **independently confirms the save header**: `oG+0x68` and `oG+0xe8` are the
hero and girl names, exactly the two `SaveAccessStr` fields the save writes
first, and `__strcpy_chk`'s bound pins each buffer at **128 bytes** — a size
previously only inferred from the 0x80 gap between the two offsets.

**The title does not choose the starting room.** `ModeTitle::Process` contains no
map, jump, or script call of any kind — its whole call set is BGM, SE, save-data
and `GameParameter::Init`. So the start room comes from somewhere later, and the
port's `M0000_00_00` remains a PORT CHOICE.

That choice is *not* confirmed by the standalone `"M0000_00_00"` literal at
`0x9bf31` either: its only reference in the binary is
`SceneWeapon::Initialize`, a debug/viewer scene, not the boot path. Worth
recording because it is exactly the sort of coincidence that would otherwise
read as confirmation.

#### An unresolved run near `0xbd678`

There is a run of 14 records at `0xbd678`, stride `0x88`, each
`{u32 = 4}{"sk1/M0000_XX_00"}` for XX = 01..14, and all 14 rooms exist in the
archive. **What reads it is not established** — neither the run's base nor the
individual strings have an `adrp`+`add` reference anywhere, which is a statement
about that scan (it cannot see indexed access through a register base) rather
than proof the data is dead.

It is recorded as an observation, not a finding, because two earlier readings of
this same region were wrong: first a "272-record 16x17 world grid", which came
from starting 0x80 too early and sampling at a stride that drifted a whole
record every 16 entries. The raw bytes killed it. Nothing should be built on
this run until its reader is found.


### `ModeGame`'s constructor: the authoritative game-start order

`ModeGame::ModeGame` @ `0x2d22e8` makes 126 calls, and their order is the
engine's own game-start sequence:

1. `ModeBase`, display sizing (`NativeDispW/H`, `texSizeFix`)
2. the nine servers, each `new`ed: `AppCharacterServer`, `AppObjectServer`,
   `AppMapServer`, `AppMapTextureServer`, `AppEventBoxServer`,
   `AppCollisionServer`, `AppEffectServer`, `AppWeaponServer`,
   `AppCharacterStandServer`
3. effect textures and six `StorageLoadModel` / `StorageLoadTexArray` /
   `AddEffectModel` triples, then `EffectPackStatInit`
4. `new GameScript`; **`fileread("sk1/sk1.lua")`** then
   `GameScript::AddScriptToOwner`
5. **`RunString("SystemInit();")`**
6. `LoadMapObject`
7. **`GameParameter::Init`**
8. `WeaponInit`
9. `CreatePlayer(-1, 0)` → `AppCharacterServer::Add(..., "MainPlayer")`
10. `TexPackHelper::Load` / `DataInit`, `SiSurfaceRender::Create`,
    `new AppCameraGame(player)`
11. **`GameSaveDataLoad(i)`**
12. `CreatePlayer_SetData`, `ModeOverlayOption::OptionUpdate`, six `AppTextView`s
13. character models + motions, nine `WeaponLoad`s, twelve `LoadEffectPackData`s

The port already does steps 4 and 5 — `sk1/sk1.lua` is loaded and `SystemInit`
called — which is now confirmed as exactly what the engine does rather than a
reasonable-looking choice. `SystemInit` itself is 85 lines of pure assignment
(every `scflag*` and `v##_##_##` story flag zeroed) with no calls at all, so it
sets no map.

**The start room comes from `GameSaveDataLoad`**, not from a literal: the
constructor loads the save *after* `GameParameter::Init` has laid down defaults.
That localises the remaining question to which `GameParameter` field holds the
map — still open, and `M0000_00_00` remains a PORT CHOICE.

One offset trap worth recording: `GameParameter+0x168` is the **item bag**
(`IsHaveItem`, `AddItem` and `SearchSlotGetCnt` all read it), while the save's
first post-name field is `oG+0x168`, which is `GameParameter+0x108`. Since
`GameParameter` sits at `oG+0x60` the two are 0x60 apart and easy to confuse —
`GameParameter+0x168` is `oG+0x1c8`, exactly where the bag was already shown to
begin.
