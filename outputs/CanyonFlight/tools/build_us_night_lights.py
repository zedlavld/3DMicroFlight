"""v1.1p14: night lights for the lower 48 outside Arizona and Nevada -> scenery/night_lights_us.bin.

Input: F:/CirrusFlightData/nightlights_us/samples.bin from tools/extract_osm_road_samples.py
(points every 32 m along OSM streets, (c) OpenStreetMap contributors, ODbL).

Built-up land is where streets are dense: road length per 500 m square, smoothed over 3 x 3
squares, gives a score from 0 (under ~5 km of street per km2) to 1 (~18 km/km2 and more).
Lights sit on the streets, as the Arizona/Nevada table's do on photo-built-up land:
  area lights  about AREA (30) per km2 at score 1 (x score^1.3): sodium, warm and LED white
  streetlights every ~96 m along secondary, primary, trunk and motorway roads where the
               score is over 0.3; brighter sodium orange
Levels as tools/build_night_lights.py: 0 (1 in 48, to 42 km), 1 (1 in 10, to 10 km), 2 (to 3.5 km).
Cells already in data/night_lights.bin (Arizona, Nevada) are left out.

File: "CFLITE03", int32 cell (4096), int32 cells, int32 points; cells as CFLITE02 (int16 cx, cy,
uint32 first, uint16 n0, n1, n2 cumulative by level, sorted by (cy, cx)); points int16 dx, dy,
uint8 colour | level << 2 | road << 4, uint8 brightness, level 0 first within a cell.

  python -P outputs/CanyonFlight/tools/build_us_night_lights.py [samples.bin] [out.bin]
"""
import sys, struct, math
from pathlib import Path
import numpy as np
root = Path(__file__).resolve().parent.parent
src = Path(sys.argv[1] if len(sys.argv) > 1 else 'F:/CirrusFlightData/nightlights_us/samples.bin')
out = Path(sys.argv[2] if len(sys.argv) > 2 else 'F:/CirrusFlightData/nightlights_us/night_lights_us.bin')
AREA = float(sys.argv[3]) if len(sys.argv) > 3 else 30.0
CELL = 4096; G = 500
E0, N0 = -1_300_000, -1_300_000; GW, GH = 5_600_000 // G, 3_100_000 // G
rec = np.memmap(src, dtype=[('e', '<i4'), ('n', '<i4'), ('c', 'u1')], mode='r')
print(f'{len(rec)/1e6:.1f}M street samples')
CH = 20_000_000
count = np.zeros(GW * GH, np.uint32)
for a in range(0, len(rec), CH):
    r = rec[a:a + CH]; gx = (r['e'] - E0) // G; gy = (r['n'] - N0) // G
    ok = (gx >= 0) & (gx < GW) & (gy >= 0) & (gy < GH)
    count += np.bincount((gy[ok] * GW + gx[ok]).astype(np.int64), minlength=GW * GH).astype(np.uint32)
grid = count.reshape(GH, GW).astype(np.float32); del count
sm = grid.copy()  # 3 x 3 box
for dy in (-1, 0, 1):
    for dx in (-1, 0, 1):
        if dx or dy: sm += np.roll(np.roll(grid, dy, 0), dx, 1)
sm /= 9
km = sm * 32 / 1000 / (G * G / 1e6)  # street km per km2
score = np.clip((km - 5) / 13, 0, 1)
print(f'built-up squares: {np.count_nonzero(score > 0)/4:.0f} km2 at score > 0, {np.count_nonzero(score > .5)/4:.0f} km2 over 0.5')
# area-light probability per sample: AREA * score^1.3 lights per km2 over the square's samples
per_sq = AREA * score ** 1.3 * (G * G / 1e6)
p_area = np.where(grid > 0, per_sq / np.maximum(grid, 1), 0).astype(np.float32)
# Arizona / Nevada cells already lit
emb = (root / 'data/night_lights.bin').read_bytes(); ncell = struct.unpack_from('<i', emb, 12)[0]
skip = {struct.unpack_from('<hh', emb, 20 + 14 * i) for i in range(ncell)}
print(len(skip), 'embedded cells skipped')
rng = np.random.default_rng(1101)
parts = []
for a in range(0, len(rec), CH):
    r = rec[a:a + CH]; e = r['e'].astype(np.int64); n = r['n'].astype(np.int64); c = r['c']
    gx = (e - E0) // G; gy = (n - N0) // G
    ok = (gx >= 0) & (gx < GW) & (gy >= 0) & (gy < GH); e, n, c, gx, gy = e[ok], n[ok], c[ok], gx[ok], gy[ok]
    s = score[gy, gx]; u = rng.random(len(e))
    area = u < p_area[gy, gx]
    street = (c >= 2) & (s > .3) & (rng.random(len(e)) < 1 / 3) & ~area
    for sel, road in ((area, 0), (street, 1)):
        k = np.count_nonzero(sel); jit = rng.normal(0, 7, (2, k)) if road == 0 else rng.normal(0, 2, (2, k))
        ee = e[sel] + jit[0]; nn = n[sel] + jit[1]
        if road == 0:
            cc = rng.random(k); colour = np.where(cc < .55, 0, np.where(cc < .85, 1, 2))
            lv = rng.random(k); level = np.where(lv < 1 / 48, 0, np.where(lv < 1 / 10, 1, 2))
            br = (150 + 105 * rng.random(k)).astype(np.int64)
        else:
            colour = np.full(k, 3); level = np.where(rng.random(k) < .25, 1, 2); br = np.full(k, 230)
        parts.append(np.stack([ee.astype(np.int64), nn.astype(np.int64), colour, level, np.full(k, road), br], 1))
L = np.concatenate(parts); del parts
cx = np.floor_divide(L[:, 0], CELL); cy = np.floor_divide(L[:, 1], CELL)
keep = ~np.isin(cy * 100000 + cx, np.array([y * 100000 + x for x, y in skip], np.int64))
L, cx, cy = L[keep], cx[keep], cy[keep]
order = np.lexsort((L[:, 3], cx, cy)); L, cx, cy = L[order], cx[order], cy[order]
print(f'{len(L)/1e6:.2f}M lights ({np.count_nonzero(L[:,4]==1)/1e6:.2f}M streetlights)')
key = cy * 100000 + cx
starts = np.flatnonzero(np.r_[True, key[1:] != key[:-1]]); ends = np.r_[starts[1:], len(L)]
table = bytearray()
for a, z in zip(starts, ends):
    lv = L[a:z, 3]; counts = [int(np.count_nonzero(lv <= k)) for k in range(3)]
    if counts[2] > 65535: raise SystemExit(f'cell {cx[a]},{cy[a]} has {counts[2]} lights')
    table += struct.pack('<hhIHHH', int(cx[a]), int(cy[a]), int(a), *counts)
dx = np.clip(L[:, 0] - cx * CELL, 0, 4095).astype('<i2'); dy = np.clip(L[:, 1] - cy * CELL, 0, 4095).astype('<i2')
pts = np.zeros(len(L), dtype=[('dx', '<i2'), ('dy', '<i2'), ('a', 'u1'), ('b', 'u1')])
pts['dx'] = dx; pts['dy'] = dy; pts['a'] = (L[:, 2] | L[:, 3] << 2 | L[:, 4] << 4).astype('u1'); pts['b'] = L[:, 5].astype('u1')
out.write_bytes(b'CFLITE03' + struct.pack('<iii', CELL, len(starts), len(L)) + bytes(table) + pts.tobytes())
print(f'{out}: {len(starts)} cells, {len(L)} lights, {out.stat().st_size/1e6:.1f} MB, densest cell {max(int(z-a) for a, z in zip(starts, ends))}')
# lights a viewer would consider (all three levels, by distance) over a few cities
LAT0 = 34.848598480225; LON0 = -111.78800201416; M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT0))
for name, la, lo in (('New York', 40.758, -73.985), ('Chicago', 41.88, -87.63), ('Los Angeles', 34.05, -118.25), ('Atlanta', 33.75, -84.39), ('Tampa', 27.95, -82.46)):
    e, n = (lo - LON0) * MX, (la - LAT0) * M
    d = np.hypot(L[:, 0] - e, L[:, 1] - n); lv = L[:, 3]
    seen = np.count_nonzero((d < 42000) & (lv == 0)) + np.count_nonzero((d < 10000) & (lv == 1)) + np.count_nonzero((d < 3500) & (lv == 2))
    print(f'  {name:12} ~{seen} lights in range')
# the same count for Phoenix from the embedded table, for scale
ep = np.frombuffer(emb, dtype=[('dx', '<i2'), ('dy', '<i2'), ('a', 'u1'), ('b', 'u1')], count=struct.unpack_from('<i', emb, 16)[0], offset=20 + 14 * ncell)
pe, pn = (-112.07 - LON0) * MX, (33.45 - LAT0) * M; seen = 0
for i in range(ncell):
    x, y, first, n0, n1, n2 = struct.unpack_from('<hhIHHH', emb, 20 + 14 * i)
    pts_ = ep[first:first + n2]; ex = x * CELL + pts_['dx'].astype(np.int64); ey = y * CELL + pts_['dy'].astype(np.int64)
    d = np.hypot(ex - pe, ey - pn); lv = (pts_['a'] >> 2) & 3
    seen += np.count_nonzero((d < 42000) & (lv == 0)) + np.count_nonzero((d < 10000) & (lv == 1)) + np.count_nonzero((d < 3500) & (lv == 2))
print(f'  {"Phoenix":12} ~{seen} lights in range (embedded table, ~6,700 considered in game)')
