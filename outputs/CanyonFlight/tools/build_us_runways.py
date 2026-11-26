"""v1.1p20: every flyable lower-48 runway -> scenery/airports_fly.bin.

Source: OurAirports airports.csv and runways.csv (public domain), as tools/build_airports.py and
tools/build_us_airports_map.py. The game flies these like its 49 compiled runway records
(source/airports.c, left out here): flattened terrain, pavement and markings, edge and approach
lights, start airports and autopilot destinations (source/airport_index.c loads the file).

Airports: large and medium, and small ones with a paved runway of 3000 ft or more (map classes
0-2 in tools/build_us_airports_map.py). Runways: open and paved, 300 m or longer. Geometry
goes into the game's Sedona-centred equirectangular frame exactly as build_airports.py does it
(east-west metres stretched by cos(lat0)/cos(lat), headings and lengths transformed with it).
Ends without coordinates use the airport point, the true heading or the runway number (x 10,
magnetic) and the length. Elevations: the end elevations, else the field elevation. A
secondary runway that crosses its airport's primary is shifted vertically to meet it, and the
primary's apron goes on the side with fewer other runways (both as build_airports.py).
The primary runway is the longest; the start direction is its low-numbered end.

File (little endian): "CFRWY001", int32 records, int32 record size (76), int32 0; then the
records, each airport's runways together, primary first; airports sorted by state, class,
name. Record: float east, north, elevation, heading (deg, game frame), length, width (m),
low, high (le / he end elevation, m); char id[8], name[24], le[3], he[3] (painted two-digit
numbers), state[2]; uint8 primary; int8 apron side (-1 left, +1 right, 0 none); uint8 class
(0 large, 1 medium, 2 small); uint8 0.

  python -P outputs/CanyonFlight/tools/build_us_runways.py [ourairports dir] [out.bin]
"""
import os, csv, math, re, struct, sys
from pathlib import Path
root = Path(__file__).resolve().parent.parent
src = Path(sys.argv[1] if len(sys.argv) > 1 else os.environ.get('OURAIRPORTS_DIR', 'work/ourairports'))
out = Path(sys.argv[2] if len(sys.argv) > 2 else 'F:/CirrusFlightData/airports_us/airports_fly.bin')
LAT0 = 34.848598480225; LON0 = -111.78800201416
M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT0)); FT = .3048
PAVED = ('ASP', 'CON', 'PEM', 'BIT', 'TAR', 'PAVED', 'ASPH', 'CONC')
RECORD = struct.Struct('<8f8s24s3s3s2sBbBB')
assert RECORD.size == 76


def project(lat, lon): return ((lon - LON0) * MX, (lat - LAT0) * M)
def paved(s): return any(k in s.upper() for k in PAVED)
def num(s):
    try: return float(s)
    except (TypeError, ValueError): return None
def number(ident):
    digits = ''.join(c for c in ident if c.isdigit())
    return digits.zfill(2)[-2:] if digits else '00'


SHORT = [(r'\bInternational\b', 'Intl'), (r'\bRegional\b', 'Rgnl'), (r'\bMunicipal\b', 'Muni'),
         (r'\bCounty\b', 'Co'), (r'\bMemorial\b', 'Mem'), (r'\bAir Force Base\b', 'AFB'),
         (r'\bNaval Air Station\b', 'NAS'), (r'\bMarine Corps Air Station\b', 'MCAS'),
         (r'\bField\b', 'Fld'), (r'\bAirport\b', ''), (r'\bAirpark\b', ''), (r'\bAirfield\b', ''),
         (r'\bExecutive\b', 'Exec'), (r'\bMountain\b', 'Mtn'), (r'\bSaint\b', 'St'),
         (r'\bJoint Reserve Base\b', 'JRB'), (r'\bArmy Air Field\b', 'AAF')]


def short_name(name):
    for a, b in SHORT: name = re.sub(a, b, name)
    name = re.sub(r'\s+', ' ', name.replace('/', ' / ')).strip(' -/')
    name = re.sub(r'\s+', ' ', name)
    return name.encode('ascii', 'replace').decode()[:23]


def main():
    compiled = set(re.findall(r'\{"([A-Z0-9]+)",', (root / 'source/airports.c').read_text()))
    air = {}
    for r in csv.DictReader(open(src / 'airports.csv', encoding='utf-8')):
        if r['iso_country'] != 'US' or r['type'] not in ('large_airport', 'medium_airport', 'small_airport'): continue
        lat, lon = float(r['latitude_deg']), float(r['longitude_deg'])
        if not (24 < lat < 49.6 and -125 < lon < -66.5) or r['ident'] in compiled: continue
        ident = r['ident'] if len(r['ident']) <= 7 else (r['local_code'] or r['gps_code'] or r['ident'])[:7]
        air[r['ident']] = dict(id=ident, type=r['type'], lat=lat, lon=lon, name=short_name(r['name']),
                               state=r['iso_region'][3:5], elevation=(num(r['elevation_ft']) or 0) * FT, rows=[])
    for r in csv.DictReader(open(src / 'runways.csv', encoding='utf-8')):
        a = air.get(r['airport_ident'])
        if a and r['closed'] != '1': a['rows'].append(r)
    airports = []
    seen = set(compiled)
    for a in air.values():
        rows = [r for r in a['rows'] if paved(r['surface']) and (num(r['length_ft']) or 0) * FT >= 300]
        longest = max(((num(r['length_ft']) or 0) for r in rows), default=0)
        cls = {'large_airport': 0, 'medium_airport': 1}.get(a['type'], 2)
        if cls == 2 and longest < 3000: continue
        recs = []
        for r in sorted(rows, key=lambda r: -(num(r['length_ft']) or 0)):
            rec = runway(a, r)
            if rec: recs.append(rec)
        if not recs or a['id'] in seen: continue
        seen.add(a['id'])
        recs[0]['primary'] = 1
        crossings(recs); apron(recs)
        airports.append((a['state'], cls, a['name'], a['id'], recs))
    airports.sort()
    blob = bytearray(); n = 0
    for state, cls, name, ident, recs in airports:
        for r in recs:
            blob += RECORD.pack(r['east'], r['north'], max(r['low'], r['high']), r['heading'], r['length'], r['width'],
                                r['low'], r['high'], ident.encode('ascii', 'replace'), name.encode('ascii', 'replace'),
                                number(r['le']).encode(), number(r['he']).encode(), state.encode(), r['primary'],
                                r['apron_side'], cls, 0)
            n += 1
    assert n < 65000 - 49, n
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(b'CFRWY001' + struct.pack('<iii', n, RECORD.size, 0) + bytes(blob))
    counts = [sum(1 for a in airports if a[1] == c) for c in range(3)]
    print(f'{out}: {len(airports)} airports (large {counts[0]}, medium {counts[1]}, small paved 3000 ft+ {counts[2]}), '
          f'{n} runways, {out.stat().st_size / 1024:.0f} KB; {len(compiled)} compiled airports left out')


def runway(a, r):
    length_ft = num(r['length_ft']) or 0; width = (num(r['width_ft']) or 0) * FT
    if width < 10: width = 23 if length_ft < 5000 else 30
    width = min(width, 90)
    la, lo, ha, ho = (num(r[k]) for k in ('le_latitude_deg', 'le_longitude_deg', 'he_latitude_deg', 'he_longitude_deg'))
    true = num(r['le_heading_degT'])
    if None not in (la, lo, ha, ho) and (la, lo) != (ha, ho):
        lat, lon = (la + ha) / 2, (lo + ho) / 2
        if true is None:
            true = math.degrees(math.atan2((ho - lo) * math.cos(math.radians(lat)), ha - la)) % 360
    else:
        lat, lon = a['lat'], a['lon']
        if true is None:
            m = re.match(r'(\d{1,2})', r['le_ident'] or '')
            if not m or not 1 <= int(m.group(1)) <= 36: return None
            true = int(m.group(1)) * 10.0
    e, n = project(lat, lon)
    s = math.cos(math.radians(LAT0)) / math.cos(math.radians(lat))
    hx, hy = math.sin(math.radians(true)) * s, math.cos(math.radians(true))
    heading = math.degrees(math.atan2(hx, hy)) % 360
    length = length_ft * FT * math.hypot(hx, hy)
    low = num(r['le_elevation_ft']); high = num(r['he_elevation_ft'])
    low = low * FT if low is not None else a['elevation']; high = high * FT if high is not None else a['elevation']
    if abs(low - high) > max(30, length * .03): low = high = a['elevation']   # bad end data: level runway
    return dict(east=e, north=n, heading=heading, length=length, width=width, low=low, high=high,
                le=r['le_ident'] or '', he=r['he_ident'] or '', primary=0, apron_side=-1)


def ends(r):
    h = math.radians(r['heading']); half = r['length'] / 2
    return ((r['east'] - math.sin(h) * half, r['north'] - math.cos(h) * half),
            (r['east'] + math.sin(h) * half, r['north'] + math.cos(h) * half))


def profile(r, along):
    t = min(max(along / r['length'] + .5, 0), 1)
    return r['low'] + (r['high'] - r['low']) * t


def along_of(r, x, y):
    h = math.radians(r['heading'])
    return (x - r['east']) * math.sin(h) + (y - r['north']) * math.cos(h)


def crossings(recs):
    """Secondary runways crossing the primary are shifted to meet its profile (build_airports.py)."""
    p = recs[0]; (ax, ay), (bx, by) = ends(p)
    for s in recs[1:]:
        (cx, cy), (dx, dy) = ends(s)
        den = (bx - ax) * (dy - cy) - (by - ay) * (dx - cx)
        if abs(den) < 1e-6: continue
        t = ((cx - ax) * (dy - cy) - (cy - ay) * (dx - cx)) / den
        u = ((cx - ax) * (by - ay) - (cy - ay) * (bx - ax)) / den
        if 0 <= t <= 1 and 0 <= u <= 1:
            x, y = ax + t * (bx - ax), ay + t * (by - ay)
            delta = profile(p, along_of(p, x, y)) - profile(s, along_of(s, x, y))
            s['low'] += delta; s['high'] += delta


def apron(recs):
    """Apron side of the primary: the side with fewer other-runway samples in the apron box."""
    p = recs[0]; h = math.radians(p['heading']); hits = {-1: 0, 1: 0}
    for o in recs[1:]:
        (ax, ay), (bx, by) = ends(o); oh = math.radians(o['heading'])
        for k in range(101):
            for w in (-o['width'] / 2, 0, o['width'] / 2):
                t = k / 100
                x = ax + t * (bx - ax) + w * math.cos(oh); y = ay + t * (by - ay) - w * math.sin(oh)
                dx, dy = x - p['east'], y - p['north']
                along = dx * math.sin(h) + dy * math.cos(h); across = dx * math.cos(h) - dy * math.sin(h)
                if -700 < along < 300 and 20 < abs(across) < 300: hits[1 if across > 0 else -1] += 1
    p['apron_side'] = 1 if hits[-1] > hits[1] else -1
    if hits[p['apron_side']]: p['apron_side'] = 0


if __name__ == '__main__':
    main()
