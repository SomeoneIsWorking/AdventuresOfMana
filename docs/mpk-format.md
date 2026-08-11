# MPK archive format (`sk1.mpk`)

Reversed statically from `LibMpkOpenRead_NeedsSize`, `LibMpkHeadSize`, `LibMpkOpen`.
No code execution involved.

## File header — 16 bytes

| Off | Type | Meaning | `sk1.mpk` | `sk1patch.mpk` |
|-----|------|---------|-----------|----------------|
| 0x00 | char[4] | magic `"mcfa"` | ✓ | ✓ |
| 0x04 | u24 (low) | entry count | **9886** | 1 |
| 0x07 | u8 (high) | version, must be 1 | 1 | 1 |
| 0x08 | u32 | compressed directory size | 113461 | 35 |
| 0x0C | u32 | same, rounded up to 512 | 113664 | 512 |

Confirmed: `LibMpkHeadSize()` returns 16; `LibMpkOpenRead_NeedsSize()` returns
`count * 256 + 16`, so an **uncompressed directory entry is 256 bytes**.
`0x0C` is `0x08` aligned up to 512 (113461 -> 222*512 = 113664; 35 -> 512),
i.e. payload data begins at `16 + align512(dirsize)` = **113680** for `sk1.mpk`.

## Directory

Compressed blob at offset 16, inflated by `decompress_stream` into
`count * 256` bytes. **The codec is NOT zlib** — verified: zlib, raw-deflate and
gzip all reject the blob. The engine links zlib (for libpng) but does not use it
here.

Real implementation chain:

    decompress_stream (264 B, wrapper)
      -> uncompress_int (1248 B)  -- allocates 32 tables (malloc x32)
      -> uncompress     (3764 B)  -- THE ACTUAL DECODER, not yet reversed
      -> uncompress_end (88 B)

Working hypothesis: table-driven Huffman + LZ (LZH/LZHUF family, common in
Japanese engines of this lineage). 32 allocated tables is the tell. Unconfirmed.

## Status

- [x] Header layout
- [x] Directory location, size, entry stride (256 B)
- [ ] `uncompress` codec  <- **blocking everything downstream**
- [ ] Directory entry layout (needs the codec first)
