"""v1.1p14: sample points along US streets from a Geofabrik extract (ODbL) for night lights.

Writes F:/CirrusFlightData/nightlights_us/samples.bin: records of int32 east, int32 north
(metres, the game's Sedona-centred frame) and uint8 road class, one every 32 m along
every highway of the classes below, lower 48 only.

Node locations go into a dense file index on disk (the US extract has ~1.4 billion nodes),
so memory use stays small.

  python -P outputs/CanyonFlight/tools/extract_osm_road_samples.py [pbf] [out dir]
"""
import sys, math, time, os
from pathlib import Path
import numpy as np
import osmium, osmium.index, osmium.filter

pbf = sys.argv[1] if len(sys.argv) > 1 else 'F:/CirrusFlightData/geofabrik/us-261124.osm.pbf'
out = Path(sys.argv[2] if len(sys.argv) > 2 else 'F:/CirrusFlightData/nightlights_us'); out.mkdir(parents=True, exist_ok=True)
LIMIT = int(os.environ.get('SAMPLE_LIMIT', '0'))  # stop after this many ways (a quick check)
LAT0 = 34.848598480225; LON0 = -111.78800201416
M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT0))
# class: 0 minor street, 1 tertiary, 2 secondary, 3 primary/trunk, 4 motorway
CLASS = {'residential': 0, 'living_street': 0, 'unclassified': 0, 'tertiary': 1, 'tertiary_link': 1,
         'secondary': 2, 'secondary_link': 2, 'primary': 3, 'primary_link': 3, 'trunk': 3, 'trunk_link': 3,
         'motorway': 4, 'motorway_link': 4}
STEP = 32.0

idx = osmium.index.create_map('dense_file_array,' + str(out / 'nodes.idx'))
fp = (osmium.FileProcessor(pbf).with_locations(idx)
      .with_filter(osmium.filter.EntityFilter(osmium.osm.WAY))
      .with_filter(osmium.filter.KeyFilter('highway')))
f = open(out / 'samples.bin.part', 'wb')
buf_e, buf_n, buf_c = [], [], []
ways = kept = samples = 0; t0 = time.time(); carry = 0.0
def flush():
    if not buf_e: return
    rec = np.zeros(len(buf_e), dtype=[('e', '<i4'), ('n', '<i4'), ('c', 'u1')])
    rec['e'] = buf_e; rec['n'] = buf_n; rec['c'] = buf_c; f.write(rec.tobytes())
    buf_e.clear(); buf_n.clear(); buf_c.clear()
for w in fp:
    ways += 1
    if LIMIT and ways > LIMIT: break
    c = CLASS.get(w.tags.get('highway'))
    if c is None: continue
    try:
        pts = [(n.lon, n.lat) for n in w.nodes]
    except osmium.InvalidLocationError:
        continue
    if not pts or not (24 < pts[0][1] < 49.6 and -125 < pts[0][0] < -66.5): continue
    kept += 1
    xy = [((lo - LON0) * MX, (la - LAT0) * M) for lo, la in pts]
    s = STEP / 2
    for (ax, ay), (bx, by) in zip(xy, xy[1:]):
        L = math.hypot(bx - ax, by - ay)
        while s < L:
            t = s / L; buf_e.append(int(ax + (bx - ax) * t)); buf_n.append(int(ay + (by - ay) * t)); buf_c.append(c)
            s += STEP
        s -= L
    if len(buf_e) > 2_000_000: samples += len(buf_e); flush()
    if ways % 2_000_000 == 0:
        print(f'{ways/1e6:.0f}M highway ways read, {kept/1e6:.1f}M kept, {(samples+len(buf_e))/1e6:.1f}M samples, {time.time()-t0:.0f} s', flush=True)
samples += len(buf_e); flush(); f.close()
os.replace(out / 'samples.bin.part', out / 'samples.bin')
print(f'done: {ways} ways, {kept} kept, {samples} samples ({samples*9/1e9:.2f} GB), {time.time()-t0:.0f} s', flush=True)
