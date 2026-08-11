#!/usr/bin/env python3
"""`sk1/str_en.bin` and `sk1/str_ja.bin` -- the game's string table.

This file exists because the project previously recorded, wrongly, that
"dialogue text is missing from the extracted data entirely" and treated it as
blocking all UI work. The text was in the archive the whole time. The earlier
search looked for CJK byte runs and for text inside the per-room assets; it did
not look at `str_*.bin`, whose names do not say "text".

Layout (both files share it, and their id lists are byte-identical):

    +0x00  u32   version, 1
    +0x04  u32   count (1906)
    +0x08  u32   44
    +0x0C  u32   offset of the text block
    +0x10  u32[count]   a permutation of 0..count-1 -- the ids in sorted order,
                        i.e. the lookup index GetIDString binary-searches
    +0x10+count*4   count records of 48 bytes, each a NUL-padded ASCII id
    text offset     count NUL-terminated UTF-8 strings, in record order

The id at index i pairs with the text at index i. That is not assumed: several
decoded Japanese strings match, character for character, the commented-out
dialogue the original developers left beside the matching call in `sk1.lua`
(e.g. `SYS_PARTYMSG_1_1` vs the comment above `PartyMsg_01`). `--selftest`
re-runs those cross-checks.
"""

import os
import struct
import sys

HEADER = 16
ID_STRIDE = 48

# id -> the exact Japanese text the developers left as a comment in sk1.lua,
# beside the call that shows it. An independent source for the SAME string, so
# agreement proves the id/text pairing rather than restating the parse.
CROSS_CHECKS = {
    "SYS_PARTYMSG_1_1": "お怪我は　大丈夫ですか？",
    "SYS_PARTYMSG_2_1": "洞くつには　マトックや\nモーニングスターで　こわせる壁が\nあるという",
}


class FormatError(Exception):
    pass


def parse(data):
    """-> (ids, texts). Raises rather than returning a partial table."""
    if len(data) < HEADER:
        raise FormatError(f"too short for a header: {len(data)} bytes")
    version, count, _, text_off = struct.unpack_from("<4I", data, 0)
    if version != 1:
        raise FormatError(f"version {version}, expected 1")
    ids_off = HEADER + count * 4
    if text_off <= ids_off or text_off > len(data):
        raise FormatError(f"text offset {text_off:#x} outside the file")
    want = count * ID_STRIDE
    if text_off - ids_off != want:
        raise FormatError(f"{count} ids at stride {ID_STRIDE} need {want} bytes "
                          f"but the id block is {text_off - ids_off}")
    ids = []
    for i in range(count):
        raw = data[ids_off + i * ID_STRIDE:ids_off + (i + 1) * ID_STRIDE]
        ids.append(raw.split(b"\0")[0].decode("ascii", "replace"))
    blob = data[text_off:]
    parts = blob.split(b"\0")
    if parts and parts[-1] == b"":
        parts = parts[:-1]
    if len(parts) != count:
        raise FormatError(f"text block holds {len(parts)} strings, not {count}")
    return ids, [p.decode("utf-8", "replace") for p in parts]


def main(argv):
    root = "scratch/dump/sk1"
    paths = {lang: os.path.join(root, f"str_{lang}.bin") for lang in ("en", "ja")}
    missing = [p for p in paths.values() if not os.path.exists(p)]
    if missing:
        print(f"FAIL: {', '.join(missing)} not found -- decoded NOTHING",
              file=sys.stderr)
        return 2

    tables = {}
    for lang, p in paths.items():
        ids, texts = parse(open(p, "rb").read())
        tables[lang] = (ids, texts)
        nonempty = sum(1 for t in texts if t.strip())
        print(f"str_{lang}.bin: {len(ids)} ids, {len(texts)} strings, "
              f"{nonempty} non-empty")

    ok = True
    if tables["en"][0] != tables["ja"][0]:
        print("  FAIL: the en and ja id lists differ")
        ok = False
    else:
        print(f"  en and ja id lists are identical ({len(tables['en'][0])} ids)")

    # The pairing check. These strings come from sk1.lua's comments, not from
    # this file, so a mismatch means the id/text pairing is wrong.
    ids, ja = tables["ja"][0], tables["ja"][1]
    for key, expect in CROSS_CHECKS.items():
        if key not in ids:
            print(f"  FAIL: cross-check id {key} is not in the table")
            ok = False
            continue
        got = ja[ids.index(key)].strip("\n")
        if got != expect:
            print(f"  FAIL: {key} decoded as {got!r}, but sk1.lua's own comment "
                  f"says {expect!r}")
            ok = False
        else:
            print(f"  cross-check OK: {key} matches sk1.lua's comment verbatim")

    # A negative the format check must be able to give: truncate and require a
    # refusal rather than a partial table.
    trunc = open(paths["en"], "rb").read()[:1000]
    try:
        parse(trunc)
        print("  SELFTEST FAILED: a truncated table parsed successfully")
        ok = False
    except FormatError:
        print("  selftest: a truncated table is refused, so the check can fail")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
