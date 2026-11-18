"""Arizona + Nevada elevation (v32).

Extends the v31 statewide 100 m grid west and north so it covers Nevada and
a 60 km border (the Sierra Nevada beside Reno and Carson City, Lake Mead).
Arizona heights come from the v31 download cache (work/usgs_az); only the new
8x8-tile chunks are downloaded from USGS 3DEP (work/usgs_nv). Writes:
  sdmc/3ds/CanyonFlight/scenery/elevation32/e_<i>_<j>.bin  streamed 100 m tiles
  data/elevation_nevada500.i16                           embedded 500 m base
  source/terrain_data.c                                  Arizona + Nevada bases + detail grids
  data/scenery_metadata.json                             provenance
Tiles outside Arizona and Nevada + 60 km are not written; the game falls
back to the 500 m bases there (and to the procedural terrain beyond them).

Run from the repository root:  python -P outputs/CanyonFlight/tools/build_elevation_v32.py [--plan]
"""
from pathlib import Path
import urllib.request, urllib.parse, json, math, hashlib, os, struct, zlib, sys, warnings
import numpy as np
from PIL import Image

os.environ.pop('SSLKEYLOGFILE', None)
Image.MAX_IMAGE_PIXELS = None
root = Path(__file__).resolve().parent.parent
az_cache = Path.cwd() / 'work' / 'usgs_az'
cache = Path.cwd() / 'work' / 'usgs_nv'
cache.mkdir(parents=True, exist_ok=True)
base = 'https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer'
LAT = 34.848598480225; LON = -111.78800201416
M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT))

# Must match include/terrain_stream.h (v32).
SPACING = 100; TILE = 256; TX = 41; TY = 48
OLD_E0 = -281600; OLD_TX = 21; OLD_TY = 26; SHIFT = 20          # v31 grid sits 20 tiles east
E0 = OLD_E0 - SHIFT * TILE * SPACING; N0 = -409600
CHUNK = 8
BASE_SPACING = 500; BASE_STEP = BASE_SPACING // SPACING
BUFFER = 60000
NODATA_BELOW = -100.0

NV = [(42.0, -120.0), (42.0, -114.0397), (36.1975, -114.0397), (36.12, -114.74), (35.6, -114.66),
      (35.0017, -114.633), (39.0, -120.0), (42.0, -120.0)]
AZ = [(37.0, -114.05), (37.0, -109.045), (31.332, -109.045), (31.332, -111.075), (32.495, -114.813), (32.718, -114.719),
      (33.03, -114.52), (33.41, -114.72), (33.70, -114.52), (34.10, -114.43), (34.30, -114.14), (34.72, -114.47), (35.10, -114.63),
      (35.60, -114.66), (36.10, -114.74), (36.12, -114.05), (37.0, -114.05)]
def poly(points): return [((lon - LON) * MX, (lat - LAT) * M) for lat, lon in points]
NVP, AZP = poly(NV), poly(AZ)

def inside(p, x, y):
    c = False
    for (x1, y1), (x2, y2) in zip(p, p[1:]):
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1: c = not c
    return c
def edge(p, x, y):
    r = 1e20
    for (x1, y1), (x2, y2) in zip(p, p[1:]):
        dx = x2 - x1; dy = y2 - y1; t = max(0, min(1, ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy)))
        r = min(r, math.hypot(x - x1 - t * dx, y - y1 - t * dy))
    return r
def near(p, x, y, d): return inside(p, x, y) or edge(p, x, y) <= d

def tile_needed(i, j):
    """Old Arizona grid tiles, and tiles within BUFFER of Nevada (any corner or centre)."""
    if SHIFT <= i < SHIFT + OLD_TX and j < OLD_TY: return True
    e = E0 + i * TILE * SPACING; n = N0 + j * TILE * SPACING; s = TILE * SPACING
    return any(near(NVP, x, y, BUFFER) for x, y in ((e, n), (e + s, n), (e, n + s), (e + s, n + s), (e + s / 2, n + s / 2)))

def export(name, e, n, d, w, h):
    bbox = [LON + (e - d / 2) / MX, LAT + (n - d / 2) / M, LON + (e + (w - .5) * d) / MX, LAT + (n + (h - .5) * d) / M]
    params = {'f': 'json', 'bbox': ','.join(map(str, bbox)), 'bboxSR': 4326, 'imageSR': 4326, 'size': f'{w},{h}',
              'format': 'tiff', 'pixelType': 'F32', 'renderingRule': json.dumps({'rasterFunction': 'None'}),
              'interpolation': 'RSP_BilinearInterpolation', 'adjustAspectRatio': 'false'}
    url = base + '/exportImage?' + urllib.parse.urlencode(params)
    path = cache / (name + '.tif')
    if not path.exists():
        for attempt in range(10):
            try:
                response = json.load(urllib.request.urlopen(url, timeout=600))
                if 'href' not in response: raise RuntimeError(response)
                tmp = path.with_suffix('.part'); urllib.request.urlretrieve(response['href'], tmp); tmp.replace(path)
                (cache / (name + '.json')).write_text(json.dumps(response, indent=2)); break
            except Exception as exc:
                print('  retry', name, attempt, exc, flush=True)
                __import__('time').sleep(20 * (attempt + 1))  # 502s come in bursts
        else:
            raise RuntimeError('export failed: ' + name)
    a = np.asarray(Image.open(path), dtype=np.float32)[::-1]
    assert a.shape == (h, w), (name, a.shape)
    return a, url

def neighbours(a):
    p = np.pad(a, 1, mode='edge')
    return [p[dy:dy + a.shape[0], dx:dx + a.shape[1]] for dy in range(3) for dx in range(3) if dy != 1 or dx != 1]

def clean_mask(full):
    valid = np.isfinite(full) & (full > NODATA_BELOW) & (full < 5000) & (np.abs(full) > 1e-3)
    a = np.where(valid, full, np.nan); spike = np.zeros_like(valid)
    for y0 in range(0, a.shape[0], 512):
        lo = max(y0 - 1, 0); hi = min(y0 + 513, a.shape[0]); band = a[lo:hi]
        with np.errstate(all='ignore'), warnings.catch_warnings():
            warnings.simplefilter('ignore'); med = np.nanmedian(np.stack(neighbours(band) + [band]), axis=0)
        s = np.abs(band - med) > 800
        spike[y0:min(y0 + 512, a.shape[0])] = s[y0 - lo:y0 - lo + min(512, a.shape[0] - y0)]
    valid &= ~spike
    for _ in range(2):
        bad = sum((~n).astype(np.int8) for n in neighbours(valid)); valid &= bad < 4
    return valid

def push_pull_fill(a, valid):
    if valid.all(): return a
    levels = [(np.where(valid, a, 0).astype(np.float32), valid.astype(np.float32))]
    while min(levels[-1][0].shape) > 2:
        v, wgt = levels[-1]; hh, ww = v.shape
        v = np.pad(v, ((0, hh % 2), (0, ww % 2))); wgt = np.pad(wgt, ((0, hh % 2), (0, ww % 2)))
        v = v.reshape(v.shape[0] // 2, 2, v.shape[1] // 2, 2).sum((1, 3))
        wgt = wgt.reshape(wgt.shape[0] // 2, 2, wgt.shape[1] // 2, 2).sum((1, 3)); levels.append((v, wgt))
    v, wgt = levels[-1]
    filled = np.where(wgt > 0, v / np.maximum(wgt, 1e-9), np.nan)
    filled = np.where(np.isnan(filled), np.nanmean(filled), filled)
    for v, wgt in reversed(levels[:-1]):
        up = np.repeat(np.repeat(filled, 2, 0), 2, 1)[:v.shape[0], :v.shape[1]]
        here = np.where(wgt > 0, v / np.maximum(wgt, 1e-9), up)
        frac = np.clip(wgt / wgt.max(), 0, 1) if wgt.max() > 0 else wgt
        filled = here * frac + up * (1 - frac) if v is not levels[0][0] else here
    return np.where(valid, a, filled).astype(np.float32)

def main():
    need = {(i, j) for j in range(TY) for i in range(TX) if tile_needed(i, j)}
    chunks = sorted({(i // CHUNK, j // CHUNK) for i, j in need
                     if not (SHIFT <= i < SHIFT + OLD_TX and j < OLD_TY)})
    print('tiles needed', len(need), 'of', TX * TY, '; chunks to download', len(chunks),
          '(~%d MB of float32 TIFF)' % (len(chunks) * (CHUNK * TILE + 1) ** 2 * 4 // 2 ** 20), flush=True)
    if '--plan' in sys.argv: return
    W = TX * TILE + 1; H = TY * TILE + 1
    full = np.full((H, W), np.nan, np.float32)
    az = np.load(az_cache / 'arizona_100m.npy')
    assert az.shape == (OLD_TY * TILE + 1, OLD_TX * TILE + 1), az.shape
    full[:az.shape[0], SHIFT * TILE:SHIFT * TILE + az.shape[1]] = az; del az
    meta_chunks = []
    for cx, cy in chunks:
        tw = min(CHUNK, TX - cx * CHUNK); th = min(CHUNK, TY - cy * CHUNK)
        w = tw * TILE + 1; h = th * TILE + 1
        e = E0 + cx * CHUNK * TILE * SPACING; n = N0 + cy * CHUNK * TILE * SPACING
        name = f'nv100_{cx}_{cy}'
        a, url = export(name, e, n, SPACING, w, h)
        y0 = cy * CHUNK * TILE; x0 = cx * CHUNK * TILE
        region = full[y0:y0 + h, x0:x0 + w]
        region[...] = np.where(np.isnan(region), a, region)     # Arizona cache wins where present
        good = a > NODATA_BELOW
        meta_chunks.append({'name': name, 'east': e, 'north': n, 'width': w, 'height': h,
                            'valid_fraction': round(float(good.mean()), 4), 'export_request': url})
        print(name, a.shape, 'valid %.3f' % good.mean(), flush=True)
    valid = clean_mask(full)
    print('valid fraction %.4f' % valid.mean(), flush=True)
    full = push_pull_fill(np.where(valid, full, 0), valid)
    heights = np.clip(np.rint(full), -100, 4500).astype('<i2')

    out = root / 'sdmc' / '3ds' / 'CanyonFlight' / 'scenery' / 'elevation32'
    out.mkdir(parents=True, exist_ok=True)
    total = 0; manifest = []
    for i, j in sorted(need):
        t = heights[j * TILE:j * TILE + TILE + 1, i * TILE:i * TILE + TILE + 1]
        payload = np.ascontiguousarray(t).tobytes()
        header = struct.pack('<4sHHHHI', b'AZEL', 1, i, j, TILE + 1, zlib.crc32(payload) & 0xffffffff)
        (out / f'e_{i}_{j}.bin').write_bytes(header + payload); total += len(header) + len(payload)
        manifest.append({'i': i, 'j': j, 'measured_fraction': round(float(valid[j * TILE:j * TILE + TILE + 1, i * TILE:i * TILE + TILE + 1].mean()), 4)})
    print('tiles', len(need), 'bytes', total, flush=True)

    # Nevada 500 m base over Nevada + 60 km (Arizona keeps its v31 base).
    xs = [p[0] for p in NVP]; ys = [p[1] for p in NVP]
    bx0 = int(math.floor((min(xs) - BUFFER - E0) / BASE_SPACING)) * BASE_STEP
    bx1 = int(math.ceil((max(xs) + BUFFER - E0) / BASE_SPACING)) * BASE_STEP
    by0 = int(math.floor((min(ys) - BUFFER - N0) / BASE_SPACING)) * BASE_STEP
    by1 = int(math.ceil((max(ys) + BUFFER - N0) / BASE_SPACING)) * BASE_STEP
    bx0 = max(bx0, 0); by0 = max(by0, 0); bx1 = min(bx1, W - 1); by1 = min(by1, H - 1)
    k = np.array([1, 4, 6, 4, 1], np.float32) / 16
    f = full[max(by0 - 2, 0):by1 + 3, max(bx0 - 2, 0):bx1 + 3].astype(np.float32)
    f = np.apply_along_axis(lambda r: np.convolve(np.pad(r, 2, mode='edge'), k, 'valid'), 1, f)
    f = np.apply_along_axis(lambda c: np.convolve(np.pad(c, 2, mode='edge'), k, 'valid'), 0, f)
    oy = by0 - max(by0 - 2, 0); ox = bx0 - max(bx0 - 2, 0)
    base500 = np.clip(np.rint(f[oy:oy + (by1 - by0) + 1:BASE_STEP, ox:ox + (bx1 - bx0) + 1:BASE_STEP]), -100, 4500).astype('<i2')
    bh, bw = base500.shape; raw500 = base500.tobytes()
    (root / 'data' / 'elevation_nevada500.i16').write_bytes(raw500)
    nv_e = E0 + bx0 * SPACING; nv_n = N0 + by0 * SPACING

    meta = json.loads((root / 'data' / 'scenery_metadata.json').read_text())
    grids = {g['name']: g for g in meta['grids']}
    source = ['#include "terrain_data.h"', '/* Generated by tools/build_elevation_v32.py; USGS 3DEP, modified. */']
    decl = []
    for name in ('arizona500', 'sedona', 'flagstaff', 'canyon'):
        g = grids[name]
        a = np.frombuffer((root / 'data' / f'elevation_{name}.i16').read_bytes(), '<i2').reshape(g['height'], g['width'])
        assert hashlib.sha256(a.tobytes()).hexdigest() == g['sha256'], name
        source.append(f"static const int16_t heights_{name}[{g['width'] * g['height']}]={{")
        source.extend(','.join(map(str, row)) + ',' for row in a); source.append('};')
        decl.append(f"{{{g['east']},{g['north']},{g['spacing']},{g['width']},{g['height']},heights_{name}}}")
    source.append(f'static const int16_t heights_nevada500[{bw * bh}]={{')
    source.extend(','.join(map(str, row)) + ',' for row in base500); source.append('};')
    decl.append(f'{{{nv_e},{nv_n},{BASE_SPACING},{bw},{bh},heights_nevada500}}')
    source.append('/* [0] Arizona 500 m base, [1..3] detail grids, [4] Nevada 500 m base (v32). */')
    source.append('const ElevationGrid elevationGrids[ELEVATION_GRID_COUNT]={' + ','.join(decl) + '};')
    (root / 'source' / 'terrain_data.c').write_text('\n'.join(source) + '\n')
    meta['grids'] = [g for g in meta['grids'] if g['name'] != 'nevada500'] + [{
        'name': 'nevada500', 'east': nv_e, 'north': nv_n, 'spacing': BASE_SPACING, 'width': bw, 'height': bh,
        'min_m': int(base500.min()), 'max_m': int(base500.max()), 'sha256': hashlib.sha256(raw500).hexdigest(),
        'derived_from': 'v32 100 m grid over Nevada + 60 km, [1,4,6,4,1] low-pass, decimated x5'}]
    meta['statewide_stream_v32'] = {'spacing': SPACING, 'tile_cells': TILE, 'east': E0, 'north': N0, 'tiles_x': TX,
                                    'tiles_y': TY, 'tiles_written': len(need), 'bytes': total, 'folder': 'scenery/elevation32',
                                    'download_date': __import__('datetime').date.today().isoformat(), 'chunks': meta_chunks}
    (root / 'data' / 'scenery_metadata.json').write_text(json.dumps(meta, indent=2))
    (root / 'data' / 'elevation_stream_manifest_v32.json').write_text(json.dumps(manifest))
    print('nevada500', base500.shape, int(base500.min()), int(base500.max()), 'at', nv_e, nv_n, flush=True)

if __name__ == '__main__':
    main()
