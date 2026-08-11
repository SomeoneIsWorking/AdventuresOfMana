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

### OPEN: 303 objects sit outside their own room cell

Under the verified 300×240 world cell, **2981 of 3284** `.odt` objects land
inside the cell their filename names. The other 303 do not, and the split is by
map, not random: maps M0000–M0011 (overworld) are perfect, 16 dungeon maps are
not. Median overshoot is 105 units and the worst is 1755, so this is **not**
benign edge overhang like the wall meshes.

Ruled out by measurement, not by argument:
- **Room-local coordinates** — would put every object in `[0,300]×[0,240]`.
  Actual counts are near zero for the dungeon maps (e.g. M0014: 0 of 69).
- **A different per-map cell size** — no candidate from a 9×9 sweep of plausible
  widths and heights fits all objects for any of the failing maps. The overworld
  by contrast is fit **uniquely** by (300, 240) across 2331 objects.
- **A different per-room cell size from `.gdt`** — the two grid sizes are mixed
  inside single maps and correspond to 300×240 plus optional margin, so they do
  not supply a per-map origin.

Most likely a per-map origin or room-index remap that has not been reversed.
Nothing in the port depends on this yet; `roomdata.py` reports the count with
its denominator so the number cannot quietly drift.

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

**KNOWN DEFECT:** object shadow planes currently draw as opaque black quads.
The 80-byte material record's blend/alpha field is not reversed, so nothing is
blended rather than something being blended on a guess.
