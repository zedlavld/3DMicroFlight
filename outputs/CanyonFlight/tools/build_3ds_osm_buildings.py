"""3DS city buildings straight from OpenStreetMap, for metros the Thor port has no tiles for.

On the 3DS only buildings 20 m and taller are drawn, and in cities without lidar those heights
all come from OpenStreetMap tags (height, or building:levels at 3.3 m a floor plus 2 m). So for
each metro below this asks the Overpass API once for the tagged tall buildings (the same query
as the Thor port's tools/build_thor_objects.py osm_tall), caches the answer, and writes 3DS
tiles in the format of tools/build_3ds_buildings.py: walls from the outline (outlines of more
than SIMPLE corners or concave ones become their minimum-area rectangle), a flat roof, colours
from the Thor palettes, lighting baked. Tiles that already exist (the 32 Thor cities) are left
alone. The index (index.bin) is rewritten from every tile in the folder.

Data: OpenStreetMap (c) OpenStreetMap contributors, ODbL 1.0.

    python -P tools/build_3ds_osm_buildings.py [--out F:/CirrusFlightData/buildings3ds_v2] [--only name,name]
"""
from pathlib import Path
import argparse, json, math, random, struct, sys, time, urllib.parse, urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_3ds_buildings as B

CACHE = Path('F:/CirrusFlightData/osm_heights_3ds')
OVERPASS = ['https://overpass-api.de/api/interpreter', 'https://overpass.kumi.systems/api/interpreter',
            'https://maps.mail.ru/osm/tools/overpass/api/interpreter']
MIN_H = 20.0
ROOFS = [(118, 118, 116), (96, 96, 98), (140, 138, 132), (158, 156, 148), (84, 82, 80), (126, 112, 100), (170, 170, 166), (108, 100, 92)]
FACADES = [(120, 130, 140), (150, 150, 146), (100, 108, 120), (168, 162, 150), (128, 118, 108), (86, 96, 108), (182, 178, 170)]

# name: (lat, lon of downtown, half size km). US metros of about half a million people and up
# that the Thor CITIES table leaves out, big neighbours outside its boxes, and state capitals
# with towers. Alaska and Hawaii are outside the scenery.
METROS = {
    'fort_worth': (32.7555, -97.3308, 6), 'st_paul': (44.9537, -93.0900, 5), 'long_beach': (33.7701, -118.1937, 6),
    'oakland': (37.8044, -122.2712, 5), 'san_jose': (37.3382, -121.8863, 7), 'bellevue': (47.6101, -122.2015, 4),
    'tacoma': (47.2529, -122.4443, 5), 'fort_lauderdale': (26.1224, -80.1373, 6), 'west_palm_beach': (26.7153, -80.0534, 5),
    'tampa': (27.9506, -82.4572, 7), 'st_petersburg': (27.7676, -82.6403, 5), 'jacksonville': (30.3322, -81.6557, 7),
    'riverside': (33.9806, -117.3755, 5), 'irvine': (33.6846, -117.8265, 6), 'sacramento': (38.5816, -121.4944, 6),
    'cincinnati': (39.1031, -84.5120, 6), 'cleveland': (41.4993, -81.6944, 6), 'columbus': (39.9612, -82.9988, 6),
    'indianapolis': (39.7684, -86.1581, 6), 'milwaukee': (43.0389, -87.9065, 6), 'raleigh': (35.7796, -78.6382, 5),
    'durham': (35.9940, -78.8986, 4), 'oklahoma_city': (35.4676, -97.5164, 6), 'tulsa': (36.1540, -95.9928, 5),
    'memphis': (35.1495, -90.0490, 6), 'louisville': (38.2527, -85.7585, 6), 'richmond': (37.5407, -77.4360, 5),
    'virginia_beach': (36.8529, -75.9780, 6), 'norfolk': (36.8508, -76.2859, 5), 'providence': (41.8240, -71.4128, 5),
    'hartford': (41.7658, -72.6734, 5), 'new_haven': (41.3083, -72.9279, 4), 'stamford': (41.0534, -73.5387, 4),
    'buffalo': (42.8864, -78.8784, 5), 'rochester': (43.1566, -77.6088, 5), 'albany': (42.6526, -73.7562, 5),
    'syracuse': (43.0481, -76.1474, 4), 'birmingham': (33.5186, -86.8104, 5), 'grand_rapids': (42.9634, -85.6681, 5),
    'omaha': (41.2565, -95.9345, 5), 'des_moines': (41.5868, -93.6250, 5), 'wichita': (37.6872, -97.3301, 5),
    'albuquerque': (35.0844, -106.6504, 6), 'el_paso': (31.7619, -106.4850, 6), 'fresno': (36.7378, -119.7871, 5),
    'bakersfield': (35.3733, -119.0187, 5), 'boise': (43.6150, -116.2023, 5), 'colorado_springs': (38.8339, -104.8214, 5),
    'little_rock': (34.7465, -92.2896, 5), 'baton_rouge': (30.4515, -91.1871, 5), 'jackson': (32.2988, -90.1848, 4),
    'knoxville': (35.9606, -83.9207, 5), 'chattanooga': (35.0456, -85.3097, 5), 'greenville': (34.8526, -82.3940, 4),
    'columbia': (34.0007, -81.0348, 5), 'charleston': (32.7765, -79.9311, 5), 'greensboro': (36.0726, -79.7920, 4),
    'winston_salem': (36.0999, -80.2442, 4), 'dayton': (39.7589, -84.1916, 5), 'toledo': (41.6528, -83.5379, 5),
    'akron': (41.0814, -81.5190, 4), 'pittsburgh_east': (40.4406, -79.8800, 3), 'harrisburg': (40.2732, -76.8867, 4),
    'allentown': (40.6023, -75.4714, 4), 'spokane': (47.6588, -117.4260, 5), 'tucson_north': (32.3000, -110.9700, 3),
    'tempe_scottsdale': (33.4500, -111.9300, 5), 'mesa': (33.4152, -111.8315, 4), 'ogden': (41.2230, -111.9738, 4),
    'provo': (40.2338, -111.6585, 4), 'reno': (39.5296, -119.8138, 5), 'madison': (43.0731, -89.4012, 5),
    'lincoln': (40.8136, -96.7026, 4), 'tallahassee': (30.4383, -84.2807, 4), 'springfield_il': (39.7817, -89.6501, 4),
    'montgomery': (32.3792, -86.3077, 4), 'bismarck': (46.8083, -100.7837, 3), 'lansing': (42.7325, -84.5555, 4),
    'springfield_ma': (42.1015, -72.5898, 4), 'worcester': (42.2626, -71.8023, 4), 'portland_me': (43.6591, -70.2568, 3),
    'corpus_christi': (27.8006, -97.3964, 5), 'mcallen': (26.2034, -98.2300, 5), 'lexington': (38.0406, -84.5037, 4),
    'shreveport': (32.5252, -93.7502, 4), 'mobile': (30.6954, -88.0399, 4), 'savannah': (32.0809, -81.0912, 4),
    'orlando_south': (28.3852, -81.5639, 6), 'daytona_beach': (29.2108, -81.0228, 5), 'atlantic_city': (39.3643, -74.4229, 4),
    'myrtle_beach': (33.6891, -78.8867, 6), 'galveston': (29.3013, -94.7977, 5), 'miami_beach_north': (25.9500, -80.1200, 5),
}

LAT0, LON0 = B.LAT0, B.LON0
M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT0)); SPAN = B.SPAN


def local(lon, lat): return (lon - LON0) * MX, (lat - LAT0) * M


def parse_height(t):
    import re
    m = re.match(r'\s*([0-9]+(?:\.[0-9]+)?)\s*(m|ft|\'|feet)?\s*$', t.get('height', ''))
    if m: return float(m.group(1)) * (.3048 if m.group(2) in ('ft', "'", 'feet') else 1)
    m = re.match(r'\s*([0-9]+)', t.get('building:levels', ''))
    if m: return int(m.group(1)) * 3.3 + 2
    return None


def fetch(name, s, w, n, e, tries=6):
    tall = '["height"~"^([2-9][0-9]|[1-9][0-9][0-9]+)"]'; levels = '["building:levels"~"^([6-9]|[1-9][0-9]+)$"]'
    q = (f'[out:json][timeout:240];(way["building"]{tall}({s},{w},{n},{e});way["building"]{levels}({s},{w},{n},{e});'
         f'relation["building"]{tall}({s},{w},{n},{e});relation["building"]{levels}({s},{w},{n},{e}););out geom;')
    for attempt in range(tries):
        try:
            req = urllib.request.Request(OVERPASS[attempt % len(OVERPASS)], data=urllib.parse.urlencode({'data': q}).encode(),
                                         headers={'User-Agent': '3DMicroFlight-scenery-tools/1.0'})
            return json.loads(urllib.request.urlopen(req, timeout=300).read())['elements']
        except Exception as ex:
            print(f'  overpass {name} retry ({ex!r})', flush=True); time.sleep(20 + 20 * attempt)
    return None


def osm_tall(name):
    path = CACHE / f'{name}.json'
    if path.exists():
        return json.loads(path.read_text())
    la, lo, h = METROS[name]; dla = h * 1000 / M; dlo = h * 1000 / (M * math.cos(math.radians(la)))
    els = fetch(name, la - dla, lo - dlo, la + dla, lo + dlo)
    if els is None:
        print(f'  overpass {name} FAILED; skipped this run', flush=True); return None
    found = {}
    for e in els:
        hgt = parse_height(e.get('tags', {}))
        if not hgt or hgt < MIN_H: continue
        rings = [e['geometry']] if e.get('geometry') else [m['geometry'] for m in e.get('members', []) if m.get('role') == 'outer' and m.get('geometry')]
        for k, pts in enumerate(rings):
            if len(pts) >= 4: found[f"{e['id']}_{k}"] = {'h': round(hgt, 1), 'pts': [[p['lon'], p['lat']] for p in pts]}
    CACHE.mkdir(parents=True, exist_ok=True); path.write_text(json.dumps(found))
    time.sleep(5)   # one query at a time, with a pause: Overpass is a shared public service
    return found


def ear_clip(ring):
    """Triangles (index triples, anticlockwise) of a simple anticlockwise polygon; None if it fails."""
    idx = list(range(len(ring))); out = []
    def cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    guard = 0
    while len(idx) > 3 and guard < 400:
        guard += 1
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = ring[i0], ring[i1], ring[i2]
            if cross(a, b, c) <= 0: continue
            if any(cross(a, b, ring[j]) >= 0 and cross(b, c, ring[j]) >= 0 and cross(c, a, ring[j]) >= 0 for j in idx if j not in (i0, i1, i2)): continue
            out.append((i0, i1, i2)); idx.pop(k); break
        else:
            return None
    if len(idx) == 3: out.append(tuple(idx))
    return out


def building(key, h, pts, x, y):
    """(cx, cy, verts, area, height) in tile dm, as build_3ds_buildings.read_tile returns."""
    w0, s0 = x * SPAN, y * SPAN
    ring = [local(p[0], p[1]) for p in pts]
    if ring[0] == ring[-1]: ring = ring[:-1]
    ring = [(round((e - w0) * 10), round((n - s0) * 10)) for e, n in ring]
    ring = [p for i, p in enumerate(ring) if p != ring[i - 1]]
    if len(ring) < 3: return None
    signed = sum(ring[k][0] * ring[(k + 1) % len(ring)][1] - ring[(k + 1) % len(ring)][0] * ring[k][1] for k in range(len(ring)))
    if signed < 0: ring = ring[::-1]
    area = abs(signed) / 200.0
    rng = random.Random(key); roof, wall = rng.choice(ROOFS), rng.choice(FACADES); top = round(h * 10)
    tris = ear_clip(ring) if len(ring) <= B.SIMPLE else None
    if tris is None:
        rect = B.min_rect(ring)
        if not rect: return None
        verts = B.box_tier(rect, top, roof, wall)
    else:
        verts = []
        for k in range(len(ring)):
            verts += B.wall_tris(*ring[k], *ring[(k + 1) % len(ring)], top, wall)
        rc = B.shade(roof, 0, 0, 1)
        for t in tris:
            verts += [((ring[i][0], ring[i][1], top), rc) for i in t]
    cx = round(sum(p[0] for p in ring) / len(ring)); cy = round(sum(p[1] for p in ring) / len(ring))
    return cx, cy, verts, area, h


def write_tile(out, x, y, blds, cap):
    blds = [bd for bd in blds if bd[2]]
    blds.sort(key=lambda bd: -bd[4] * math.sqrt(max(bd[3], 1.0)))
    chosen, tris = [], 0
    for bd in blds:
        t = len(bd[2]) // 3
        if tris + t > cap: continue
        chosen.append(bd); tris += t
    if not chosen: return 0, 0
    chosen.sort(key=lambda bd: (bd[4] < B.FAR_MIN, -bd[4] * math.sqrt(max(bd[3], 1.0))))
    body = bytearray()
    for cx, cy, verts, _, _ in chosen:
        body += struct.pack('<hhH', cx, cy, len(verts))
        for (vx, vy, vh), (r, g, bb) in verts:
            body += struct.pack('<hhhBBBB', vx, vy, vh, r, g, bb, 0)
    nv = sum(len(bd[2]) for bd in chosen)
    sub = out / f'{x // 16}_{y // 16}'; sub.mkdir(exist_ok=True)
    (sub / f'b_{x}_{y}.bin').write_bytes(b'CFB3D001' + struct.pack('<iiII', x, y, len(chosen), nv) + body)
    return len(chosen), tris


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='F:/CirrusFlightData/buildings3ds_v2')
    ap.add_argument('--only', help='comma-separated metro names')
    ap.add_argument('--cap', type=int, default=2000)
    a = ap.parse_args(); out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    existing = {tuple(map(int, p.stem.split('_')[1:])) for p in out.glob('*/b_*.bin')}
    names = a.only.split(',') if a.only else list(METROS)
    tiles = {}; failed = []
    for name in names:
        found = osm_tall(name)
        if found is None: failed.append(name); continue
        n = 0
        for key, f in found.items():
            pts = f['pts']; e, no = local(*pts[0])
            ce = sum(local(*p)[0] for p in pts) / len(pts); cn = sum(local(*p)[1] for p in pts) / len(pts)
            x, y = math.floor(ce / SPAN), math.floor(cn / SPAN)
            if (x, y) in existing or B.under_landmark(ce, cn): continue
            bd = building(key, f['h'], pts, x, y)
            if bd: tiles.setdefault((x, y), {})[key] = bd; n += 1
        print(f'{name}: {len(found)} tall buildings in OSM, {n} in new tiles', flush=True)
    made = kept = tris_all = 0
    for (x, y), blds in sorted(tiles.items()):
        k, t = write_tile(out, x, y, list(blds.values()), a.cap)
        if k: made += 1; kept += k; tris_all += t
    allt = sorted(tuple(map(int, p.stem.split('_')[1:])) for p in out.glob('*/b_*.bin'))
    (out / 'index.bin').write_bytes(b'CFBIDX01' + struct.pack('<I', len(allt)) + b''.join(struct.pack('<ii', x, y) for x, y in allt))
    print(f'{made} new tiles, {kept} buildings, {tris_all} triangles; index {len(allt)} tiles -> {out}')
    if failed: print('FAILED (rerun later):', ','.join(failed))


if __name__ == '__main__':
    main()
