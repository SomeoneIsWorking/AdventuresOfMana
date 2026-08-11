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
