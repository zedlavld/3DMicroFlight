"""v1.1p19: every lower-48 airport for the moving map -> scenery/airports_us.bin.

Source: OurAirports airports.csv and runways.csv (public domain), the same data as
tools/build_airports.py. Large, medium and small airports (no heliports, seaplane bases,
balloonports or closed fields); open runways. Airports already in source/airports.c are left
out (the game draws those from its own table).

Runway ends come from the CSV where given; otherwise from the runway heading and length, or
from the runway number (x 10 degrees, magnetic, so a few degrees off) when even the heading
is missing. Runways with neither (e.g. "N"/"S" strips, helipads) are not drawn; the airport
still shows as a dot.

Class (when the map shows it): 0 large, 1 medium, 2 small with a paved runway of 3000 ft or
more, 3 other small.

File (little endian): "CFAPT001", int32 cell (m), int32 east0, north0 (grid origin, m),
int32 gw, gh, int32 airports, int32 runways; uint32 first[gw*gh+1] (airport index per cell,
row-major from the south-west); airports sorted by cell, 20 bytes each: int32 east, north,
uint16 first runway, uint8 runways, uint8 class, char id[8]; runways 10 bytes each: int16
east1, north1, east2, north2 (m from the airport), uint8 paved, uint8 width (m).

  python -P outputs/CanyonFlight/tools/build_us_airports_map.py [ourairports dir] [out.bin]
"""
import os, csv, math, re, struct, sys
from pathlib import Path
root = Path(__file__).resolve().parent.parent
src = Path(sys.argv[1] if len(sys.argv) > 1 else os.environ.get('OURAIRPORTS_DIR', 'work/ourairports'))
out = Path(sys.argv[2] if len(sys.argv) > 2 else 'F:/CirrusFlightData/airports_us/airports_us.bin')
LAT0 = 34.848598480225; LON0 = -111.78800201416
M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT0)); FT = .3048
CELL = 32000
def project(lat, lon): return ((lon - LON0) * MX, (lat - LAT0) * M)
PAVED = ('ASP', 'CON', 'PEM', 'BIT', 'TAR', 'PAVED', 'ASPH', 'CONC')
def paved(s): return any(k in s.upper() for k in PAVED)
def num(s):
    try: return float(s)
    except ValueError: return None

compiled = set(re.findall(r'\{"([A-Z0-9]+)",', (root / 'source/airports.c').read_text()))
air = {}
for r in csv.DictReader(open(src / 'airports.csv', encoding='utf-8')):
    if r['iso_country'] != 'US' or r['type'] not in ('large_airport', 'medium_airport', 'small_airport'): continue
    lat, lon = float(r['latitude_deg']), float(r['longitude_deg'])
    if not (24 < lat < 49.6 and -125 < lon < -66.5) or r['ident'] in compiled: continue
    ident = r['ident'] if len(r['ident']) <= 7 else (r['local_code'] or r['gps_code'] or r['ident'])[:7]
    air[r['ident']] = dict(id=ident, type=r['type'], lat=lat, lon=lon, runways=[], longPaved=False)
for r in csv.DictReader(open(src / 'runways.csv', encoding='utf-8')):
    a = air.get(r['airport_ident'])
    if not a or r['closed'] == '1': continue
    length = (num(r['length_ft']) or 0) * FT; width = (num(r['width_ft']) or 0) * FT; p = paved(r['surface'])
    if p and length >= 3000 * FT: a['longPaved'] = True
    ae, an = project(a['lat'], a['lon'])
    la, lo, ha, ho = (num(r[k]) for k in ('le_latitude_deg', 'le_longitude_deg', 'he_latitude_deg', 'he_longitude_deg'))
    if None not in (la, lo, ha, ho):
        e1, n1 = project(la, lo); e2, n2 = project(ha, ho)
    else:
        h = num(r['le_heading_degT'])
        if h is None:
            m = re.match(r'(\d{1,2})', r['le_ident'] or '')
            h = int(m.group(1)) * 10 if m and 1 <= int(m.group(1)) <= 36 else None
        if h is None or length < 50: continue
        h = math.radians(h); cx, cy = ae, an
        e1, n1 = cx - math.sin(h) * length / 2, cy - math.cos(h) * length / 2
        e2, n2 = cx + math.sin(h) * length / 2, cy + math.cos(h) * length / 2
    rel = [max(-32000, min(32000, round(v))) for v in (e1 - ae, n1 - an, e2 - ae, n2 - an)]
    a['runways'].append((*rel, 1 if p else 0, max(0, min(255, round(width)))))
cls = {'large_airport': 0, 'medium_airport': 1}
rows = []
for a in air.values():
    e, n = project(a['lat'], a['lon'])
    c = cls.get(a['type'], 2 if a['longPaved'] else 3)
    rows.append((round(e), round(n), c, a['id'], a['runways']))
E0 = min(r[0] for r in rows) // CELL * CELL; N0 = min(r[1] for r in rows) // CELL * CELL
GW = (max(r[0] for r in rows) - E0) // CELL + 1; GH = (max(r[1] for r in rows) - N0) // CELL + 1
def cell(r): return ((r[1] - N0) // CELL) * GW + (r[0] - E0) // CELL
rows.sort(key=lambda r: (cell(r), r[2]))
first = [0] * (GW * GH + 1); k = 0
for c in range(GW * GH):
    first[c] = k
    while k < len(rows) and cell(rows[k]) == c: k += 1
first[-1] = len(rows)
abytes = bytearray(); rbytes = bytearray(); nr = 0
for e, n, c, ident, rws in rows:
    rws = rws[:255]
    abytes += struct.pack('<iiHBB8s', e, n, nr, len(rws), c, ident.encode('ascii', 'replace')[:8])
    for rw in rws: rbytes += struct.pack('<hhhhBB', *rw)
    nr += len(rws)
assert nr < 65536, nr
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(b'CFAPT001' + struct.pack('<iiiiiii', CELL, E0, N0, GW, GH, len(rows), nr)
                + struct.pack(f'<{len(first)}I', *first) + bytes(abytes) + bytes(rbytes))
counts = [sum(1 for r in rows if r[2] == c) for c in range(4)]
print(f'{out}: {len(rows)} airports (large {counts[0]}, medium {counts[1]}, small paved 3000 ft+ {counts[2]}, other small {counts[3]}), '
      f'{nr} runways, grid {GW} x {GH} of {CELL//1000} km, {out.stat().st_size/1024:.0f} KB; {len(compiled)} compiled airports left out')
