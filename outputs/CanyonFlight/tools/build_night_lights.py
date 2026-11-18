"""v34.02: night lights for towns, cities and roads -> data/night_lights.bin.

Sources (public domain, US Census Bureau TIGER/Line 2023), downloaded once to
F:/CirrusFlightData/census:
  tl_2023_04_place, tl_2023_32_place            incorporated places and CDPs (AZ, NV)
  tl_2023_04_prisecroads, tl_2023_32_prisecroads primary and secondary roads
plus the game's own coarse photo tiles (stream32 c_*.cft, 8192 m, ETC1): the
two ETC1 base colours of each 4x4 block give a 64 m colour raster, and a
"built-up" score from small-scale texture and greyness (streets and roofs are
busy and grey; desert, rock and mountains are smooth or coloured).

Lights:
  area lights  inside place boundaries, about one per 150 m where the photos
               look built up (probability 0.03 + 0.97 * score^1.3), plus dense
               built-up land just outside boundaries; sodium, warm and LED white
  streetlights every 90 m along primary/secondary roads where they cross
               built-up parts of towns; brighter sodium orange
Each light gets a level: 0 (1 in 48, drawn to 42 km), 1 (1 in 10, to 10 km),
2 (the rest, to 3.5 km), so distant cities read as a glow of sparse bright
points and nearby streets fill in.

File (little endian): "CFLITE01", int32 cell (4096 m), int32 cells, int32
points; per cell int16 cx, int16 cy, uint32 first, uint16 n0, n1, n2
(cumulative counts by level, cells sorted by (cy, cx)); per point int16 dx,
dy (metres from the cell's south-west corner), uint8 colour | level << 2 |
road << 4, uint8 brightness.

  python -P outputs/CanyonFlight/tools/build_night_lights.py [census dir] [stream32 dir]
"""
import sys, struct, math, random, json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
root = Path(__file__).resolve().parent.parent
census = Path(sys.argv[1] if len(sys.argv) > 1 else 'F:/CirrusFlightData/census')
stream = Path(sys.argv[2] if len(sys.argv) > 2 else root / 'sdmc/3ds/CanyonFlight/scenery/stream32')
LAT0 = 34.848598480225; LON0 = -111.78800201416
M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT0))
def project(lon, lat): return ((lon - LON0) * MX, (lat - LAT0) * M)
TILE = 8192; BLOCK = 64; N = TILE // BLOCK; CELL = 4096

# ------------------------------------------------------------------ shapefiles
def read_dbf(path):
    b = path.read_bytes(); n, hl, rl = struct.unpack_from('<IHH', b, 4); fields = []; o = 32
    while b[o] != 0x0D:
        name = b[o:o + 11].split(b'\0')[0].decode(); fields.append((name, b[o + 16])); o += 32
    rows = []
    for i in range(n):
        r = hl + i * rl + 1; row = {}
        for name, ln in fields: row[name] = b[r:r + ln].decode('latin-1').strip(); r += ln
        rows.append(row)
    return rows
def read_shp(path):
    b = path.read_bytes(); o = 100; shapes = []
    while o < len(b):
        _, words = struct.unpack_from('>ii', b, o); c = o + 8; kind = struct.unpack_from('<i', b, c)[0]
        if kind in (3, 5):
            nparts, npts = struct.unpack_from('<ii', b, c + 36); parts = list(struct.unpack_from('<%di' % nparts, b, c + 44))
            pts = np.frombuffer(b, '<f8', npts * 2, c + 44 + 4 * nparts).reshape(-1, 2)
            shapes.append([pts[a:z] for a, z in zip(parts, parts[1:] + [npts])])
        else: shapes.append([])
        o = c + words * 2
    return shapes
def load(name):
    d = census / name; return read_shp(d / (name + '.shp')), read_dbf(d / (name + '.dbf'))
places, roads = [], []
for st in ('04', '32'):
    shp, dbf = load(f'tl_2023_{st}_place')
    for rings, row in zip(shp, dbf):
        rings = [np.column_stack(project(r[:, 0], r[:, 1])) for r in rings]
        places.append((row['NAME'], rings))
    shp, dbf = load(f'tl_2023_{st}_prisecroads')
    for lines, row in zip(shp, dbf):
        for l in lines: roads.append((row.get('MTFCC', ''), np.column_stack(project(l[:, 0], l[:, 1]))))
print('places', len(places), 'road lines', len(roads), flush=True)

# ------------------------------------------------------------------ photo built-up score
sys.path.insert(0, str(Path(__file__).resolve().parent))
def etc1_base(path):
    b = np.frombuffer(path.read_bytes()[32:32 + 512 * 512 // 2], dtype='<u8'); hi = (b >> np.uint64(32)).astype(np.int64)
    diff = (hi >> 1) & 1
    def bits(v, s, n): return (v >> s) & ((1 << n) - 1)
    def ex5(v): return (v << 3) | (v >> 2)
    def sx(v): return np.where(v >= 4, v - 8, v)
    ch = []
    for s5, s4a, s4b, sd in ((27, 28, 24, 24), (19, 20, 16, 16), (11, 12, 8, 8)):
        a5 = bits(hi, s5, 5); c1 = np.where(diff, ex5(a5), bits(hi, s4a, 4) * 17)
        c2 = np.where(diff, ex5(np.clip(a5 + sx(bits(hi, sd, 3)), 0, 31)), bits(hi, s4b, 4) * 17)
        ch.append((c1 + c2) / 2)
    rgb = np.stack(ch, -1); n = 128; out = np.zeros((n, n, 3)); k = 0
    blocks = rgb.reshape(n // 2, n // 2, 4, 3)
    out[0::2, 0::2] = blocks[:, :, 0]; out[0::2, 1::2] = blocks[:, :, 1]; out[1::2, 0::2] = blocks[:, :, 2]; out[1::2, 1::2] = blocks[:, :, 3]
    return out  # row 0 = south
def box(a, r):
    k = 2 * r + 1; p = np.pad(a, r, mode='edge'); c = np.pad(p.cumsum(0).cumsum(1), ((1, 0), (1, 0)))
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)
def built_up(rgb):
    l = rgb.mean(2); mx = rgb.max(2); mn = rgb.min(2); sat = (mx - mn) / np.maximum(mx, 1)
    tex = box(np.abs(l - box(l, 1)), 2); ridge = np.abs(box(l, 2) - box(l, 6))
    grey = np.clip((0.32 - box(sat, 1)) / 0.16, 0, 1)
    s = np.clip((tex - 4.5) / 6, 0, 1) * (0.35 + 0.65 * grey) * np.clip(1.4 - ridge / 18, 0, 1) * np.clip((l - 60) / 40, 0, 1)
    return box(s, 1)

# ------------------------------------------------------------------ per tile
tiles = sorted(p for p in stream.glob('c_*.cft'))
print('coarse tiles', len(tiles), flush=True)
def bbox(rings):
    a = np.vstack(rings); return a[:, 0].min(), a[:, 1].min(), a[:, 0].max(), a[:, 1].max()
place_boxes = [(bbox(r), r) for _, r in places if r]
road_boxes = [(bbox([l]), kind, l) for kind, l in roads]
rng = random.Random(34)
lights = []   # (east, north, colour, level, road, brightness)
def level():
    r = rng.random(); return 0 if r < 1 / 48 else 1 if r < 1 / 10 else 2
for n_t, path in enumerate(tiles):
    _, x, y = path.stem.split('_'); x = int(x); y = int(y); e0 = x * TILE; n0 = y * TILE
    near_places = [r for (a, b, c, d), r in place_boxes if c >= e0 and a <= e0 + TILE and d >= n0 and b <= n0 + TILE]
    near_roads = [(k, l) for (a, b, c, d), k, l in road_boxes if c >= e0 - 100 and a <= e0 + TILE + 100 and d >= n0 - 100 and b <= n0 + TILE + 100]
    if not near_places and not near_roads: continue
    try: score = built_up(etc1_base(path))
    except Exception as ex: print('skip', path.name, ex); continue
    mask = Image.new('L', (N, N), 0); dr = ImageDraw.Draw(mask)
    for rings in near_places:
        for ring in rings:
            pts = [((px - e0) / BLOCK, (py - n0) / BLOCK) for px, py in ring]
            if len(pts) > 2: dr.polygon(pts, fill=255)   # holes are rare in places; drawn filled
    inside = np.asarray(mask) > 0   # row = north index (row 0 south, matching score)
    urban = (inside & (box(score, 3) > .18)) | (score > .55)
    # Area lights: ~0.18 candidates per 64 m block (one per ~150 m).
    nr = np.random.default_rng((x * 73856093 ^ y * 19349663) & 0xffffffff)
    prob = np.where(inside, 0.03 + 0.97 * score ** 1.3, np.where(score > .55, (score - .55) * 1.5, 0))
    js, is_ = np.nonzero(nr.random((N, N)) < 0.18 * prob)
    for j, i in zip(js, is_):
        c = rng.random(); colour = 0 if c < .55 else 1 if c < .85 else 2
        lights.append((e0 + (i + rng.random()) * BLOCK, n0 + (j + rng.random()) * BLOCK, colour, level(), 0, int(150 + 105 * rng.random())))
    # Streetlights along primary/secondary roads in towns and built-up land.
    for kind, line in near_roads:
        for (ax, ay), (bx, by) in zip(line[:-1], line[1:]):
            seg = math.hypot(bx - ax, by - ay); k = 0
            while k * 90 < seg:
                t = k * 90 / seg; px = ax + (bx - ax) * t; py = ay + (by - ay) * t; k += 1
                if not (e0 <= px < e0 + TILE and n0 <= py < n0 + TILE): continue
                i = int((px - e0) / BLOCK); j = int((py - n0) / BLOCK)
                if not urban[j, i]: continue
                lights.append((px, py, 3, 1 if rng.random() < .25 else 2, 1, 230))
    if n_t % 500 == 0: print('tile', n_t, 'lights', len(lights), flush=True)
print('lights', len(lights), flush=True)

# ------------------------------------------------------------------ write
cells = {}
for e, n, colour, lvl, road, br in lights:
    cx, cy = math.floor(e / CELL), math.floor(n / CELL)
    cells.setdefault((cx, cy), []).append((lvl, int(e - cx * CELL), int(n - cy * CELL), colour, road, br))
keys = sorted(cells, key=lambda k: (k[1], k[0]))
table = bytearray(); data = bytearray(); first = 0
for cx, cy in keys:
    pts = sorted(cells[(cx, cy)]); counts = [sum(1 for p in pts if p[0] <= l) for l in range(3)]
    table += struct.pack('<hhIHHH', cx, cy, first, *counts)
    for lvl, dx, dy, colour, road, br in pts: data += struct.pack('<hhBB', min(dx, 4095), min(dy, 4095), colour | lvl << 2 | road << 4, br)
    first += len(pts)
# ------------------------------------------------------------------ traffic roads
# Primary (S1100) and secondary (S1200) roads for moving car lights: simplified
# to 25 m, cut into pieces of at most 12 points, filed by the cell of their
# first point (points as int16 metres from that cell's corner).
def simplify(pts, tol):
    if len(pts) < 3: return pts
    a, b = pts[0], pts[-1]; d = b - a; L = math.hypot(*d) or 1
    dist = np.abs(d[0] * (pts[:, 1] - a[1]) - d[1] * (pts[:, 0] - a[0])) / L; i = int(dist.argmax())
    if dist[i] <= tol: return np.vstack([a, b])
    return np.vstack([simplify(pts[:i + 1], tol)[:-1], simplify(pts[i:], tol)])
xs = [k[0] * CELL for k in keys]; ys = [k[1] * CELL for k in keys]
lo_x, hi_x, lo_y, hi_y = min(xs) - 20000, max(xs) + 20000, min(ys) - 20000, max(ys) + 20000
pieces = {}
for kind, line in roads:
    if kind not in ('S1100', 'S1200'): continue
    if line[:, 0].max() < lo_x or line[:, 0].min() > hi_x or line[:, 1].max() < lo_y or line[:, 1].min() > hi_y: continue
    s = simplify(line, 25)
    for k in range(0, len(s) - 1, 11):
        seg = s[k:k + 12]
        if len(seg) < 2: continue
        cx, cy = math.floor(seg[0][0] / CELL), math.floor(seg[0][1] / CELL)
        rel = [(int(round(px - cx * CELL)), int(round(py - cy * CELL))) for px, py in seg]
        if max(abs(v) for p_ in rel for v in p_) > 32000: continue
        pieces.setdefault((cx, cy), []).append((1 if kind == 'S1100' else 0, rel))
pkeys = sorted(pieces, key=lambda k: (k[1], k[0]))
ptable = bytearray(); pdata = bytearray(); np_ = 0; npieces = 0
for cx, cy in pkeys:
    for primary, rel in pieces[(cx, cy)]:
        ptable += struct.pack('<hhIHH', cx, cy, np_, len(rel), primary)
        for px, py in rel: pdata += struct.pack('<hh', px, py)
        np_ += len(rel); npieces += 1
print('traffic pieces', npieces, 'points', np_, flush=True)
out = root / 'data/night_lights.bin'
out.write_bytes(b'CFLITE02' + struct.pack('<iii', CELL, len(keys), first) + bytes(table) + bytes(data)
                + struct.pack('<ii', npieces, np_) + bytes(ptable) + bytes(pdata))
print('wrote', out, len(keys), 'cells', first, 'lights', out.stat().st_size, 'bytes')
json.dump({'cells': len(keys), 'lights': first, 'sources': ['TIGER/Line 2023 place AZ NV', 'TIGER/Line 2023 prisecroads AZ NV', 'stream32 coarse photo tiles']},
          open(root / 'data/night_lights_meta.json', 'w'), indent=1)
