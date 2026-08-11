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
