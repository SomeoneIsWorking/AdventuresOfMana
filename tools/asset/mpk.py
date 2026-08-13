#!/usr/bin/env python3
"""Extract Square Enix / MCF `.mpk` archives (magic `mcfa`).

Format reversed statically from libmcfandroid.so; see docs/mpk-format.md.
Payload streams are LHA static-Huffman (-lh5-, 13-bit dictionary).

Every entry is validated: the decoder must consume EXACTLY the stored compressed
size. A stream that decodes to the right length while consuming the wrong number
of input bytes is garbage that happens not to throw -- that mistake cost a debug
cycle here, so the check is mandatory rather than optional.
"""
import argparse, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lha

MAGIC = b'mcfa'
DICBIT = 13
ENTRY_SIZE = 256
NAME_MAX = 240


def read_header(f):
    h = f.read(16)
    if h[:4] != MAGIC:
        sys.exit("not an MPK archive (magic %r, expected %r)" % (h[:4], MAGIC))
    v, dir_csize, data_off = struct.unpack_from('<III', h, 4)
    version, count = v >> 24, v & 0xFFFFFF
    if version != 1:
        sys.exit("unsupported MPK version %d" % version)
    return count, dir_csize, data_off


def read_directory(f, count, dir_csize):
    f.seek(16)
    raw = lha.decompress(f.read(dir_csize), count * ENTRY_SIZE, DICBIT)
    out = []
    for i in range(count):
        e = raw[i * ENTRY_SIZE:(i + 1) * ENTRY_SIZE]
        name = e[:e.index(b'\0')].decode('ascii', 'replace')
        flag, off, csize, usize = struct.unpack_from('<IIII', e, NAME_MAX)
        out.append((name, flag, off, csize, usize))
    return out


def extract_one(f, data_off, off, csize, usize):
    f.seek(data_off + off)
    dec = lha.Decoder(f.read(csize), DICBIT)
    data = dec.decode(usize)
    if dec.br.p != csize:
        raise ValueError("consumed %d of %d compressed bytes" % (dec.br.p, csize))
    return data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('archive')
    ap.add_argument('-o', '--outdir')
    ap.add_argument('-l', '--list', action='store_true')
    ap.add_argument('-e', '--entry', help='extract exactly this archive path')
    a = ap.parse_args()

    f = open(a.archive, 'rb')
    count, dir_csize, data_off = read_header(f)
    ents = read_directory(f, count, dir_csize)
    print("%s: %d entries, payload at %d" % (a.archive, count, data_off), file=sys.stderr)

    if a.list:
        for name, flag, off, csize, usize in ents:
            print("%10d %10d  %s" % (csize, usize, name))
        return

    if not a.outdir:
        sys.exit("need -o OUTDIR (or --list)")

    if a.entry:
        matches = [e for e in ents if e[0] == a.entry]
        if not matches:
            sys.exit("scanned %d entries, matched 0 for %r" % (len(ents), a.entry))
        if len(matches) != 1:
            sys.exit("scanned %d entries, matched %d for %r; refusing ambiguous extraction" %
                     (len(ents), len(matches), a.entry))
        name, flag, off, csize, usize = matches[0]
        data = extract_one(f, data_off, off, csize, usize)
        p = os.path.join(a.outdir, name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'wb') as g:
            g.write(data)
        print("scanned %d entries, extracted 1: %s (%d bytes)" %
              (len(ents), name, len(data)), file=sys.stderr)
        return

    ok = failed = 0
    errors = []
    for i, (name, flag, off, csize, usize) in enumerate(ents):
        try:
            data = extract_one(f, data_off, off, csize, usize)
        except Exception as ex:
            failed += 1
            errors.append((name, "%s: %s" % (type(ex).__name__, ex)))
            continue
        p = os.path.join(a.outdir, name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'wb') as g:
            g.write(data)
        ok += 1
        if ok % 500 == 0:
            print("  %d/%d" % (i + 1, count), file=sys.stderr)

    print("\nextracted %d, FAILED %d, of %d total" % (ok, failed, count), file=sys.stderr)
    for name, err in errors[:20]:
        print("  FAIL %s -- %s" % (name, err), file=sys.stderr)
    if failed:
        sys.exit(1)


if __name__ == '__main__':
    main()
