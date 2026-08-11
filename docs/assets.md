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
