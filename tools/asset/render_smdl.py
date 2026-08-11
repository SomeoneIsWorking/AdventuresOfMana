#!/usr/bin/env python3
"""Software-rasterize an .smdl to PNG -- a VISUAL check that the parsed geometry
is real, not merely self-consistent. Orthographic, depth-shaded, no textures."""
import struct, zlib, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smdl

def png(path, w, h, rgb):
    raw = b''.join(b'\x00' + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    def ck(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t+d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n'
        + ck(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
        + ck(b'IDAT', zlib.compress(raw, 6)) + ck(b'IEND', b''))

def render(path, out, S=420, axis=('x', 'y')):
    b = open(path, 'rb').read()
    m = smdl.parse(b)
    P = smdl.positions(b, m)
    I = smdl.indices(b, m)
    ai = {'x': 0, 'y': 1, 'z': 2}
    hx, hy = ai[axis[0]], ai[axis[1]]
    hz = 3 - hx - hy
    xs = [p[hx] for p in P]; ys = [p[hy] for p in P]; zs = [p[hz] for p in P]
    mnx, mxx, mny, mxy = min(xs), max(xs), min(ys), max(ys)
    mnz, mxz = min(zs), max(zs)
    sc = (S * 0.9) / max(mxx-mnx, mxy-mny, 1e-6)
    cx, cy = (mnx+mxx)/2, (mny+mxy)/2
    fb = bytearray(S*S*3); zb = [1e30]*(S*S)
    def proj(p):
        return (int(S/2 + (p[hx]-cx)*sc), int(S/2 - (p[hy]-cy)*sc), p[hz])
    for t in range(0, len(I)-2, 3):
        tri = [proj(P[I[t+k]]) for k in range(3)]
        d = (sum(v[2] for v in tri)/3 - mnz) / max(mxz-mnz, 1e-6)
        sh = int(40 + 200*(1-d))
        col = (sh, int(sh*0.93), int(sh*0.82))
        x0 = max(0, min(v[0] for v in tri)); x1 = min(S-1, max(v[0] for v in tri))
        y0 = max(0, min(v[1] for v in tri)); y1 = min(S-1, max(v[1] for v in tri))
        (ax, ay, _), (bx, by, _), (ccx, ccy, _) = tri
        den = (by-ccy)*(ax-ccx) + (ccx-bx)*(ay-ccy)
        if den == 0: continue
        for y in range(y0, y1+1):
            for x in range(x0, x1+1):
                w0 = ((by-ccy)*(x-ccx) + (ccx-bx)*(y-ccy)) / den
                w1 = ((ccy-ay)*(x-ccx) + (ax-ccx)*(y-ccy)) / den
                w2 = 1-w0-w1
                if w0 < 0 or w1 < 0 or w2 < 0: continue
                z = w0*tri[0][2] + w1*tri[1][2] + w2*tri[2][2]
                i = y*S+x
                if z < zb[i]:
                    zb[i] = z
                    fb[i*3:i*3+3] = bytes(col)
    png(out, S, S, bytes(fb))
    return m, len(I)//3

if __name__ == '__main__':
    src = sys.argv[1]; dst = sys.argv[2]
    ax = tuple(sys.argv[3]) if len(sys.argv) > 3 else ('x', 'y')
    m, tris = render(src, dst, axis=ax)
    print("%s: %d verts, %d tris, %d bones, stride %d -> %s"
          % (src.split('/')[-1], m['vertex']['count'], tris, m['bones'],
             m['vertex']['stride'], dst))
