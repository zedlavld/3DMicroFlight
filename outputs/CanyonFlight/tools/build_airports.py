"""Generate source/airports.c and data/airports_v31.json from OurAirports (v31).

One record per open runway. The first three records (KSEZ, KFLG, KGCN) keep
their validated v25 values so saves and regression tests are unchanged.

Geometry is converted into the game's local equirectangular frame (centred on
Sedona). East-west game metres are true metres times cos(lat0)/cos(lat), so a
true heading and length are transformed with that factor. This keeps runways
aligned with NAIP imagery exported in the same frame.

Usage (repository root):
    python -P outputs/CanyonFlight/tools/build_airports.py work/ourairports
"""
import csv, json, math, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
src = Path(sys.argv[1] if len(sys.argv) > 1 else 'work/ourairports')
LAT0 = 34.848598480225; LON0 = -111.78800201416
M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT0))
FT = .3048

# (ident, menu name, primary runway le_ident)
AIRPORTS = [('KSEZ', 'Sedona', '03'), ('KFLG', 'Flagstaff', '03'), ('KGCN', 'Grand Canyon', '03'),
            ('KPHX', 'Phoenix', '08'), ('KDVT', 'Deer Valley', '07R'), ('KTUS', 'Tucson', '12'),
            ('KPRC', 'Prescott', '03R'), ('KIGM', 'Kingman', '03'), ('KHII', 'Lake Havasu', '14'),
            ('KNYL', 'Yuma', '03L'), ('KPGA', 'Page', '15'), ('KSOW', 'Show Low', '07'), ('KDUG', 'Douglas', '17'),
            # v32: Nevada, appended so Arizona record indices (saves) are unchanged.
            ('KLAS', 'Las Vegas', '08L'), ('KVGT', 'North Las Vegas', '12R'), ('KHND', 'Henderson', '17R'),
            ('K61B', 'Boulder City', '09'), ('K67L', 'Mesquite', '01'), ('KRNO', 'Reno', '17R'),
            ('KCXP', 'Carson City', '09'), ('KEKO', 'Elko', '06'), ('KELY', 'Ely', '18'),
            ('KTPH', 'Tonopah', '15'), ('KWMC', 'Winnemucca', '14')]
LEGACY = {  # v25 records: id, east, north, elevation, heading, length, width, low, high
    'KSEZ': (-45.25601, 0.21236, 1471.2696, 45.8, 1564.2336, 30.48, 1443.5328, 1471.2696),
    'KFLG': (10631.13802, 32229.87559, 2136.9528, 41.0, 2682.24, 45.72, 2136.9528, 2132.076),
    'KGCN': (-32759.85966, 122731.63782, 2014.4232, 40.6, 2742.8952, 45.72, 1991.2584, 2014.4232)}


def project(lat, lon):
    return ((lon - LON0) * MX, (lat - LAT0) * M)


def number(ident):
    digits = ''.join(c for c in ident if c.isdigit())
    return digits.zfill(2)[-2:] if digits else '00'


def main():
    airports = {r['ident']: r for r in csv.DictReader(open(src / 'airports.csv', encoding='utf-8'))}
    runways = {}
    for r in csv.DictReader(open(src / 'runways.csv', encoding='utf-8')):
        runways.setdefault(r['airport_ident'], []).append(r)
    records = []
    for ident, name, primary in AIRPORTS:
        a = airports[ident]
        rows = [r for r in runways[ident] if r['closed'] == '0' and r['le_latitude_deg'] and r['he_latitude_deg']]
        rows.sort(key=lambda r: (r['le_ident'] != primary, -float(r['length_ft'])))
        assert rows and rows[0]['le_ident'] == primary, (ident, primary)
        for k, r in enumerate(rows):
            lat = (float(r['le_latitude_deg']) + float(r['he_latitude_deg'])) / 2
            lon = (float(r['le_longitude_deg']) + float(r['he_longitude_deg'])) / 2
            e, n = project(lat, lon)
            s = math.cos(math.radians(LAT0)) / math.cos(math.radians(lat))
            true = float(r['le_heading_degT']) if r['le_heading_degT'] else math.degrees(math.atan2(
                (float(r['he_longitude_deg']) - float(r['le_longitude_deg'])) * math.cos(math.radians(lat)),
                float(r['he_latitude_deg']) - float(r['le_latitude_deg']))) % 360
            hx, hy = math.sin(math.radians(true)) * s, math.cos(math.radians(true))
            heading = math.degrees(math.atan2(hx, hy)) % 360
            length = float(r['length_ft']) * FT * math.hypot(hx, hy)
            fallback = float(a['elevation_ft'] or 0) * FT
            low = float(r['le_elevation_ft']) * FT if r['le_elevation_ft'] else fallback
            high = float(r['he_elevation_ft']) * FT if r['he_elevation_ft'] else fallback
            rec = {'id': ident, 'name': name, 'state': a['iso_region'][3:5],
                   'cls': {'large_airport': 0, 'medium_airport': 1}.get(a['type'], 2), 'le': r['le_ident'], 'he': r['he_ident'], 'primary': int(k == 0),
                   'east': e, 'north': n, 'elevation': max(low, high), 'heading': heading, 'true_heading': true,
                   'length': length, 'width': float(r['width_ft']) * FT, 'low': low, 'high': high,
                   'source': {'runway_id': r['id'], 'airport_ref': r['airport_ref'], 'surface': r['surface']}}
            if k == 0 and ident in LEGACY:
                L = LEGACY[ident]
                rec.update(east=L[0], north=L[1], elevation=L[2], heading=L[3], length=L[4], width=L[5], low=L[6], high=L[7])
            elif k > 0 and ident in LEGACY:
                continue  # legacy airports keep their single runway
            records.append(rec)
    # A secondary runway that crosses its primary is shifted vertically so both
    # profiles agree at the intersection (endpoint elevations are rounded and
    # disagree by metres at some fields). Primary profiles are never changed.
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
    for p in (r for r in records if r['primary']):
        (ax, ay), (bx, by) = ends(p)
        for s in (r for r in records if r['id'] == p['id'] and not r['primary']):
            (cx, cy), (dx, dy) = ends(s)
            den = (bx - ax) * (dy - cy) - (by - ay) * (dx - cx)
            if abs(den) < 1e-6:
                continue
            t = ((cx - ax) * (dy - cy) - (cy - ay) * (dx - cx)) / den
            u = ((cx - ax) * (by - ay) - (cy - ay) * (bx - ax)) / den
            if 0 <= t <= 1 and 0 <= u <= 1:
                x, y = ax + t * (bx - ax), ay + t * (by - ay)
                delta = profile(p, along_of(p, x, y)) - profile(s, along_of(s, x, y))
                s['low'] += delta; s['high'] += delta; s['elevation'] = max(s['low'], s['high'])
                s['crossing_shift_m'] = round(delta, 3)
                print('crossing', s['id'], s['le'], 'shifted %+.2f m to meet' % delta, p['le'])
    # Apron/building side: the apron box (along -700..300 m, 20..300 m off the
    # centreline) on the side where fewer other-runway samples fall; left
    # (-1, the v25 layout) on a tie.
    for p in (r for r in records if r['primary']):
        h = math.radians(p['heading']); hits = {-1: 0, 1: 0}
        for o in records:
            if o is p or o['id'] != p['id']:
                continue
            (ax, ay), (bx, by) = ends(o)
            for k in range(201):
                for w in (-o['width'] / 2, 0, o['width'] / 2):
                    t = k / 200; oh = math.radians(o['heading'])
                    x = ax + t * (bx - ax) + w * math.cos(oh); y = ay + t * (by - ay) - w * math.sin(oh)
                    dx, dy = x - p['east'], y - p['north']
                    along = dx * math.sin(h) + dy * math.cos(h); across = dx * math.cos(h) - dy * math.sin(h)
                    if -700 < along < 300 and 20 < abs(across) < 300:
                        hits[1 if across > 0 else -1] += 1
        p['apron_side'] = 1 if hits[-1] > hits[1] else -1
        if hits[p['apron_side']]:
            p['apron_side'] = 0  # both sides blocked: no generic apron/buildings
        if hits[-1] or hits[1]:
            print('apron', p['id'], 'left hits', hits[-1], 'right hits', hits[1], '-> side', p['apron_side'])
    for r in records:
        r.setdefault('apron_side', -1)
    starts = [i for i, r in enumerate(records) if r['primary']]
    lines = ['#include "flight.h"', '/* Generated by tools/build_airports.py from OurAirports (public domain). */',
             f'const Airport builtinAirports[AIRPORT_COUNT]={{']
    for r in records:
        lines.append('{"%s",%.5ff,%.5ff,%.4ff,%.4ff,%.4ff,%.4ff,%.4ff,%.4ff,"%s","%s","%s",%d,%d,"%s",%d},' % (
            r['id'], r['east'], r['north'], r['elevation'], r['heading'], r['length'], r['width'], r['low'], r['high'],
            r['name'], number(r['le']), number(r['he']), r['primary'], r['apron_side'], r['state'], r['cls']))
    lines.append('};')
    lines.append('const int airportStarts[AIRPORT_START_COUNT]={' + ','.join(map(str, starts)) + '};')
    assert len(records) == 49 and len(starts) == 24, ('update AIRPORT_COUNT / AIRPORT_START_COUNT in flight.h', len(records), len(starts))
    # v1.1p20: airports[] points here until source/airport_index.c appends the card's runways.
    lines.append('const Airport *airports=builtinAirports;')
    lines.append('int airportCount=AIRPORT_COUNT;')
    lines.append('float airportReach(const Airport *ap){')
    lines.append(' float along=fmaxf(ap->length/2+30,650)+100,across=fmaxf(ap->width/2+65,250)+100;')
    lines.append(' return sqrtf(along*along+across*across);')
    lines.append('}')
    (root / 'source' / 'airports.c').write_text('\n'.join(lines) + '\n')
    (root / 'data' / 'airports_v31.json').write_text(json.dumps(records, indent=1))  # name kept: v31 and v32 tools read it
    print('records', len(records), 'starts', len(starts))
    for r in records:
        print('%-5s %-4s %-4s prim=%d side=%+d e=%9.0f n=%9.0f hdg=%6.2f (true %6.2f) len=%6.0f elev=%6.1f' % (
            r['id'], r['le'], r['he'], r['primary'], r['apron_side'], r['east'], r['north'], r['heading'],
            r['true_heading'], r['length'], r['elevation']))


if __name__ == '__main__':
    main()
