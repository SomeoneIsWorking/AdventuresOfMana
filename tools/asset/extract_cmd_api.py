#!/usr/bin/env python3
"""Extract the complete Lua `cmd` module API from libmcfandroid.so.

Pairs each Lua-visible name with the native C++ implementation it dispatches to,
by walking luaopen_cmd's tolua_function registrations and then following each
generated wrapper to its single `bl <impl>`.

Emits a report to stdout. Exits non-zero if it finds nothing -- a silent empty
result would be indistinguishable from "the scan broke".
"""
import re, struct, sys, subprocess, os

SO  = sys.argv[1] if len(sys.argv) > 1 else "scratch/raw/libmcfandroid.so"
ASM = sys.argv[2] if len(sys.argv) > 2 else "scratch/raw/full.asm"

data = open(SO, 'rb').read()
phoff, = struct.unpack_from('<Q', data, 0x20)
phentsize, phnum = struct.unpack_from('<HH', data, 0x36)
segs = []
for i in range(phnum):
    o = phoff + i * phentsize
    if struct.unpack_from('<I', data, o)[0] != 1:   # PT_LOAD
        continue
    p_off, p_vaddr, _, p_filesz = struct.unpack_from('<QQQQ', data, o + 8)
    segs.append((p_vaddr, p_vaddr + p_filesz, p_off))

def cstr(v):
    for a, b, off in segs:
        if a <= v < b:
            o = off + (v - a)
            e = data.index(b'\0', o)
            s = data[o:e]
            if 0 < len(s) < 80 and all(32 <= c < 127 for c in s):
                return s.decode()
    return None

# ---- index every instruction by address, plus a symbol map -------------------
insn = {}          # addr -> (mnemonic, operands)
sym_at = {}        # addr -> symbol name
for line in open(ASM):
    m = re.match(r'^([0-9a-f]{16}) <(.+)>:', line)
    if m:
        sym_at[int(m.group(1), 16)] = m.group(2)
        continue
    m = re.match(r'^\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(\S+)\s*(.*)$', line)
    if m:
        insn[int(m.group(1), 16)] = (m.group(2), m.group(3).strip())

if not insn:
    sys.exit("FATAL: parsed 0 instructions from %s -- disassembly is empty or malformed" % ASM)

LUAOPEN = next((a for a, n in sym_at.items() if n == '_Z11luaopen_cmdP9lua_State'), None)
if LUAOPEN is None:
    sys.exit("FATAL: luaopen_cmd symbol not found in %s" % ASM)

def imm(s):
    s = s.strip().lstrip('#')
    return int(s, 16) if s.startswith('0x') else int(s, 16 if re.fullmatch(r'[0-9a-f]+', s) else 10)

def scan(start, limit, want_bl=None, collect_regs=False):
    """Walk forward from `start`, emulating adrp/add page-relative address forming."""
    regs, out = {}, []
    a = start
    for _ in range(limit):
        if a not in insn:
            break
        mn, ops = insn[a]
        if mn == 'adrp':
            m = re.match(r'(\w+), (?:0x)?([0-9a-f]+)', ops)
            if m: regs[m.group(1)] = int(m.group(2), 16)
        elif mn == 'add':
            m = re.match(r'(\w+), (\w+), #(?:0x)?([0-9a-f]+)', ops)
            if m and m.group(2) in regs:
                regs[m.group(1)] = regs[m.group(2)] + imm(m.group(3))
        elif mn == 'adr':
            m = re.match(r'(\w+), (?:0x)?([0-9a-f]+)', ops)
            if m: regs[m.group(1)] = int(m.group(2), 16)
        elif mn == 'bl':
            m = re.search(r'<([^>]+)>', ops)
            tgt = m.group(1) if m else ops
            if want_bl and want_bl in tgt:
                out.append((dict(regs), a))
            elif want_bl is None:
                # skip tolua argument-marshalling and stack-guard thunks; the
                # first remaining call is the real implementation
                base = tgt.replace('@plt', '')
                if not (base.startswith('tolua_') or base == '__stack_chk_fail'):
                    return tgt
        elif mn in ('ret', 'b') and want_bl is None:
            break
        a += 4
    return out if want_bl else None

# ---- pass 1: the 200 tolua_function(L, "Name", wrapper) registrations --------
regs_at_call = scan(LUAOPEN, 8000, want_bl='tolua_function')
print(f"tolua_function registrations found: {len(regs_at_call)}", file=sys.stderr)
if not regs_at_call:
    sys.exit("FATAL: 0 registrations found -- the adrp/add emulation failed")

api = []
for regs, call_addr in regs_at_call:
    name = cstr(regs.get('x1', 0)) if 'x1' in regs else None
    wrapper = regs.get('x2')
    impl = scan(wrapper, 400) if wrapper else None      # follow wrapper -> real fn
    # What the wrapper PUSHES is the Lua-visible return type; the mangled name
    # of the implementation does not carry it.
    pushes = scan(wrapper, 400, want_bl='tolua_push')
    kinds = set()
    a = wrapper
    for _ in range(400):
        if a not in insn: break
        mn, ops = insn[a]
        if mn == 'bl':
            mm = re.search(r'<([^>]+)>', ops)
            t = (mm.group(1) if mm else ops).replace('@plt', '')
            if t.startswith('tolua_push'): kinds.add(t[len('tolua_push'):])
            elif t == 'tolua_error': break
        a += 4
    api.append((name, wrapper, impl, kinds))

named = [a for a in api if a[0]]
resolved = [a for a in api if a[2]]
print(f"  names resolved: {len(named)}/{len(api)}", file=sys.stderr)
print(f"  impls  resolved: {len(resolved)}/{len(api)}", file=sys.stderr)

demangle = subprocess.run(['c++filt'], input='\n'.join(
    (a[2] or '').replace('@plt', '') for a in api), capture_output=True, text=True).stdout.split('\n')

print("| # | Lua name | native implementation | returns |")
print("|---|----------|-----------------------|---------|")
for i, (name, wrapper, impl, kinds) in enumerate(api):
    sig = demangle[i].strip() if i < len(demangle) else ''
    ret = "+".join(sorted(kinds)) if kinds else "nothing"
    print(f"| {i+1} | `{name or '??'}` | `{sig or '??'}` | {ret} |")
