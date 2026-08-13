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
| 0x0C | u32 | **absolute offset of payload section** = `align512(16 + dirsize)` | 113664 | 512 |

Confirmed: `LibMpkHeadSize()` returns 16; `LibMpkOpenRead_NeedsSize()` returns
`count * 256 + 16`, so an **uncompressed directory entry is 256 bytes**.

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

**CONFIRMED: LHA static Huffman + LZ77 (`-lh5-`), 13-bit (8 KB) dictionary.**
Identified from `uncompress_int`'s allocation fingerprint and verified against
the real archive; the other four dictionary sizes the engine supports all fail
to decode it, so the test discriminates.

| Allocation | LHA constant |
|---|---|
| `510` x5 | `NC = UCHAR_MAX + MAXMATCH + 2 - THRESHOLD` = 510 |
| `19` x5 | `NT = CODE_BIT + 3` = 19 |
| `2038` x10 | `left[]`/`right[]` = `u16[2*NC-1]` |
| `8192` / `512` | `c_table[4096]` / `pt_table[256]` as u16 |
| `2048..32768` | five ring buffers, dictionary 11..15 bits |

Implementation: `tools/asset/lha.py`.

## Directory entry -- 256 bytes

| Off | Type | Meaning |
|-----|------|---------|
| 0x00 | char[240] | path, NUL-terminated (e.g. `sk1/M0000_00_00.lua`) |
| 0xF0 | u32 | flags -- always 1 across all 9886 entries |
| 0xF4 | u32 | offset of stream, relative to the payload section at `hdr[0x0C]` |
| 0xF8 | u32 | compressed size |
| 0xFC | u32 | uncompressed size |

Streams are 512-byte aligned and tightly packed (inter-stream gaps 0..511).
Entries are sorted by name, so offsets are not monotonic.

## Validation

The extractor requires the decoder to consume **exactly** `csize` input bytes.
Checking only the output length is worthless: the decoder pre-allocates its
output buffer, so a length check passes by construction. Under that broken test
700 consecutive candidate base offsets all "passed"; under the consumption test
exactly one does (113664).

For targeted inspection, `mpk.py -e <exact/archive/path> -o <directory>` scans
the validated directory but inflates only the named payload. It reports the
number of directory entries scanned and refuses both zero matches and ambiguous
duplicate matches; an absent entry therefore cannot masquerade as an empty
successful extraction. `tools/verify.sh` extracts the opening room script,
byte-compares it with the full-corpus copy, and also exercises the missing-name
failure path.

`mpk.py --check-dir <directory>` compares an extraction against the archive's
directory itself: exact relative path set and every entry's declared
uncompressed size. A raw count is insufficient because one missing asset plus
one unrelated file still totals 9,886. The full verifier requires this check
before running any parser, then temporarily exercises one missing member, one
wrong-sized member, and one extra member; an exit trap restores the shipping
corpus before parser work begins.

## Status

- [x] Header layout
- [x] Directory location, size, entry stride
- [x] Codec: LHA `-lh5-`, dicbit 13
- [x] Directory entry layout
- [x] Full extraction of all 9886 entries
- [x] Exact one-entry extraction with positive and missing-entry gates
- [x] Exact extracted-corpus identity with missing/size/extra negative gates
