"""3DS city buildings (prototype): the Thor port's building tiles, thinned for the 3DS.

Reads the Thor object tiles (o_x_y.bin, 2048 m tiles, from the CirrusFlight Thor branch's
tools/build_thor_objects.py: Microsoft US Building Footprints and OpenStreetMap, ODbL 1.0;
USGS lidar heights, public domain) and writes one small file per tile for the 3DS:

  b_x_y.bin  'CFB3D001', i32 x, i32 y, u32 buildings, u32 vertices
    per building: i16 cx, cy (dm from the tile's south-west corner; the game samples the
                  ground there), u16 vertex count
    per vertex:   i16 x, y (dm, tile-local), i16 h (dm above the sampled ground; walls
                  start 3 m below it so slopes don't show a gap), u8 r, g, b (lighting
                  baked from a fixed sun), u8 0                                (10 B)
  Vertices are plain triangle lists (three per triangle), walls then roof.

Each tile keeps its most prominent buildings (height x sqrt(footprint area)) until the
triangle cap is reached, so downtown towers and casinos come first and houses last.

    python -P tools/build_3ds_buildings.py [--src F:/CirrusFlightData/objects_thor]
        [--out F:/CirrusFlightData/buildings3ds] [--box X0 Y0 X1 Y1] [--cap 3000]
"""
from pathlib import Path
import argparse, math, struct, sys

SPAN = 2048.0
SUN = (-0.45, -0.55, 0.70)          # light from the south-west, fairly high
BELOW = -30                          # wall bottom, dm below the sampled ground

def shade(rgb, nx, ny, nz):
    l = math.sqrt(sum(c * c for c in SUN)); d = (nx * SUN[0] + ny * SUN[1] + nz * SUN[2]) / l
    k = 0.58 + 0.42 * max(0.0, d)
    return tuple(min(255, int(c * k)) for c in rgb)

def wall_tris(ax, ay, bx, by, top, rgb):
    """One wall quad (dm), outward normal from the edge direction (outer ring anticlockwise)."""
    dx, dy = bx - ax, by - ay; l = math.hypot(dx, dy)
    if l < 0.5:
        return []
    c = shade(rgb, dy / l, -dx / l, 0.0)
    a0, b0, b1, a1 = (ax, ay, BELOW), (bx, by, BELOW), (bx, by, top), (ax, ay, top)
    return [(a0, c), (b0, c), (b1, c), (a0, c), (b1, c), (a1, c)]

def box_building(e, n, yaw, length, width, height, roof, wall):
    """Oriented box from the Thor CFOBJ001 record; returns (cx, cy, verts, area, height)."""
    ca, sa = math.cos(yaw), math.sin(yaw); hl, hw = length / 2, width / 2
    corners = [(e + ca * u - sa * v, n + sa * u + ca * v) for u, v in ((-hl, -hw), (hl, -hw), (hl, hw), (-hl, hw))]
    pts = [(round(x * 10), round(y * 10)) for x, y in corners]; top = round(height * 10)
    verts = []
    for i in range(4):
        verts += wall_tris(*pts[i], *pts[(i + 1) % 4], top, wall)
    rc = shade(roof, 0, 0, 1)
    for t in ((0, 1, 2), (0, 2, 3)):
        verts += [((pts[k][0], pts[k][1], top), rc) for k in t]
    return round(e * 10), round(n * 10), verts, length * width, height

def read_tile(path):
    b = path.read_bytes()
    magic = b[:8]
    if magic not in (b'CFOBJ001', b'CFOBJ002'):
        return []
    x, y, nt, nb = struct.unpack_from('<iiII', b, 8); q = 24 + nt * 20
    out = []
    for i in range(nb):
        e, n, z, yaw = struct.unpack_from('<4f', b, q); L, W, H = struct.unpack_from('<3H', b, q + 16)
        roof, wall = tuple(b[q + 22:q + 25]), tuple(b[q + 25:q + 28]); q += 28
        out.append(box_building(e, n, yaw, L / 10, W / 10, H / 10, roof, wall))
    if magic == b'CFOBJ002' and q + 4 <= len(b):
        count, = struct.unpack_from('<I', b, q); q += 4
        for _ in range(count):
            e, n, z = struct.unpack_from('<3f', b, q); roof, wall = tuple(b[q + 12:q + 15]), tuple(b[q + 15:q + 18])
            parts = b[q + 18]; q += 20
            verts, area, tallest = [], 0.0, 0.0
            for _ in range(parts):
                top_dm, rings, tris = struct.unpack_from('<3H', b, q); q += 6
                lens = struct.unpack_from(f'<{rings}H', b, q); q += rings * 2; total = sum(lens)
                pts = [struct.unpack_from('<2h', b, q + k * 4) for k in range(total)]; q += total * 4
                idx = struct.unpack_from(f'<{tris * 3}H', b, q); q += tris * 6
                top = top_dm; tallest = max(tallest, top / 10)
                base = 0
                for r, ln in enumerate(lens):
                    ring = pts[base:base + ln]
                    for k in range(ln):
                        verts += wall_tris(*ring[k], *ring[(k + 1) % ln], top, wall)
                    if r == 0 and ln >= 3:   # footprint area of the outer ring (m^2)
                        area = max(area, abs(sum(ring[k][0] * ring[(k + 1) % ln][1] - ring[(k + 1) % ln][0] * ring[k][1] for k in range(ln))) / 200.0)
                    base += ln
                rc = shade(roof, 0, 0, 1)
                verts += [((pts[i][0], pts[i][1], top), rc) for i in idx if i < total]
            out.append((round(e * 10), round(n * 10), verts, area, tallest))
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default='F:/CirrusFlightData/objects_thor')
    ap.add_argument('--out', default='F:/CirrusFlightData/buildings3ds')
    ap.add_argument('--box', type=int, nargs=4, default=[-152, 68, -146, 75], help='tile x0 y0 x1 y1 (default: Las Vegas)')
    ap.add_argument('--cap', type=int, default=3000, help='triangles per tile')
    a = ap.parse_args()
    src, out = Path(a.src), Path(a.out); out.mkdir(parents=True, exist_ok=True)
    x0, y0, x1, y1 = a.box; tiles = kept = tris_all = 0
    for x in range(x0, x1 + 1):
        for y in range(y0, y1 + 1):
            p = src / f'o_{x}_{y}.bin'
            if not p.exists():
                continue
            blds = [bd for bd in read_tile(p) if bd[2]]
            blds.sort(key=lambda bd: -bd[4] * math.sqrt(max(bd[3], 1.0)))
            chosen, tris = [], 0
            for bd in blds:
                t = len(bd[2]) // 3
                if tris + t > a.cap:
                    continue          # a smaller building later may still fit
                chosen.append(bd); tris += t
            if not chosen:
                continue
            body = bytearray()
            for cx, cy, verts, _, _ in chosen:
                body += struct.pack('<hhH', cx, cy, len(verts))
                for (vx, vy, vh), (r, g, bb) in verts:
                    body += struct.pack('<hhhBBBB', vx, vy, vh, r, g, bb, 0)
            nv = sum(len(bd[2]) for bd in chosen)
            (out / f'b_{x}_{y}.bin').write_bytes(b'CFB3D001' + struct.pack('<iiII', x, y, len(chosen), nv) + body)
            tiles += 1; kept += len(chosen); tris_all += tris
            print(f'b_{x}_{y}: {len(chosen)}/{len(blds)} buildings, {tris} triangles, {len(body) // 1024} KB', flush=True)
    print(f'{tiles} tiles, {kept} buildings, {tris_all} triangles -> {out}')

if __name__ == '__main__':
    main()
