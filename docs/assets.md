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

## NOT reversed

The **40-byte node record's internals**. `SetBinary` never reads them — they are
consumed by the collision *query* code, not the loader — so nothing here
constrains their layout, and guessing would be inventing. Reversing them means
following `CollisionBase` / `SiCollisionMesh` query methods, which is a separate
job and only needed once the port does actual collision.
