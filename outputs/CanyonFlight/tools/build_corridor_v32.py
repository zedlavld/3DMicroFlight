"""Build the v32 photo tile set: Arizona + Nevada, ETC1-compressed.

Tiles keep the v31 grid (2048 m fine / 8192 m coarse keys in the Sedona-centred
equirectangular frame) and the CFSTRM header layout, now 'CFSTRM16' with an
ETC1 payload (4 bits per pixel: fine 699,040 bytes, coarse 174,752 bytes; a
quarter of the v31 RGB565 tiles). Written to scenery/stream32/ so a v31 pack
can stay on the card beside it.

Sources, in order: the v31 download cache (work/corridor/<tile>.png), the v31
RGB565 tile on disk (decoded; Arizona tiles whose PNG was not kept), or a new
USGS/USDA NAIP export (Nevada). Encoding: tex3ds -f etc1 -q medium -m lanczos.
The image handed to tex3ds is in texture-memory row order (the PNG flipped
vertically), which reproduces the v31 packing byte for byte for RGB565.

  python -P outputs/CanyonFlight/tools/build_corridor_v32.py [--plan] [--workers 4]
"""
from pathlib import Path
import argparse, math, json, struct, hashlib, os, time, urllib.request, urllib.parse, concurrent.futures, datetime, subprocess, tempfile
import numpy as np
from PIL import Image
os.environ.pop('SSLKEYLOGFILE', None)
parser = argparse.ArgumentParser(); parser.add_argument('--plan', action='store_true'); parser.add_argument('--workers', type=int, default=4)
parser.add_argument('--offline', action='store_true', help='no downloads: only tiles with a local source')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
cache = Path.cwd() / 'work/corridor'; cache.mkdir(parents=True, exist_ok=True)
old = root / 'sdmc/3ds/CanyonFlight/scenery/stream'
out = root / 'sdmc/3ds/CanyonFlight/scenery/stream32'; out.mkdir(parents=True, exist_ok=True)
scratch = Path(tempfile.gettempdir()) / 'cirrus_etc1'; scratch.mkdir(exist_ok=True)
TEX3DS = r'C:\devkitPro\tools\bin\tex3ds.exe'
base = 'https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPImagery/ImageServer'
lat0 = 34.848598480225; lon0 = -111.78800201416; m = 6371000 * math.pi / 180; mx = m * math.cos(math.radians(lat0))
FINE_BYTES, COARSE_BYTES = 699040, 174752
NV_IDS = {'KLAS', 'KVGT', 'KHND', 'K61B', 'K67L', 'KRNO', 'KCXP', 'KEKO', 'KELY', 'KTPH', 'KWMC'}
NV = [(42.0, -120.0), (42.0, -114.0397), (36.1975, -114.0397), (36.12, -114.74), (35.6, -114.66),
      (35.0017, -114.633), (39.0, -120.0), (42.0, -120.0)]
NVP = [((lon - lon0) * mx, (lat - lat0) * m) for lat, lon in NV]
STATE_BUFFER = 12000; AIRPORT_FINE = 3500

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

# Arizona: exactly the v31 catalog (every key there has a built tile).
raw = (root / 'data/corridor_index.bin').read_bytes()
assert raw[:8] == b'CFINDEX5'
count = struct.unpack_from('<I', raw, 12)[0]
az_keys = {struct.unpack_from('<3i', raw, 16 + 12 * k) for k in range(count)}
if (root / 'data/corridor_index_v31.bin').exists():   # rerun: the v31 catalog was saved on the first run
    raw = (root / 'data/corridor_index_v31.bin').read_bytes(); count = struct.unpack_from('<I', raw, 12)[0]
    az_keys = {struct.unpack_from('<3i', raw, 16 + 12 * k) for k in range(count)}
else:
    (root / 'data/corridor_index_v31.bin').write_bytes(raw)
nv_keys = set()
xs = [p[0] for p in NVP]; ys = [p[1] for p in NVP]; span = 8192; margin = STATE_BUFFER + span / math.sqrt(2)
for x in range(math.floor((min(xs) - margin) / span), math.ceil((max(xs) + margin) / span)):
    for y in range(math.floor((min(ys) - margin) / span), math.ceil((max(ys) + margin) / span)):
        cx = (x + .5) * span; cy = (y + .5) * span
        if inside(NVP, cx, cy) or edge(NVP, cx, cy) <= margin: nv_keys.add((1, x, y))
runways = json.loads((root / 'data/airports_v31.json').read_text())
for r in runways:
    if r['id'] not in NV_IDS: continue
    h = math.radians(r['heading']); half = r['length'] / 2; span = 2048
    ends = [(r['east'] - math.sin(h) * half, r['north'] - math.cos(h) * half), (r['east'] + math.sin(h) * half, r['north'] + math.cos(h) * half)]
    margin = AIRPORT_FINE + span / math.sqrt(2)
    for x in range(math.floor((min(e[0] for e in ends) - margin) / span), math.ceil((max(e[0] for e in ends) + margin) / span)):
        for y in range(math.floor((min(e[1] for e in ends) - margin) / span), math.ceil((max(e[1] for e in ends) + margin) / span)):
            cx = (x + .5) * span; cy = (y + .5) * span; a, b = ends; dx = b[0] - a[0]; dy = b[1] - a[1]
            t = max(0, min(1, ((cx - a[0]) * dx + (cy - a[1]) * dy) / (dx * dx + dy * dy)))
            if math.hypot(cx - a[0] - t * dx, cy - a[1] - t * dy) <= margin: nv_keys.add((0, x, y))
nv_keys -= az_keys
keys = sorted(az_keys | nv_keys)
name = lambda k: f'{"f" if k[0] == 0 else "c"}_{k[1]}_{k[2]}'
local = sum(1 for k in keys if (cache / (name(k) + '.png')).exists() or (old / (name(k) + '.cft')).exists())
print('PLAN Arizona', len(az_keys), 'Nevada', len(nv_keys), '(fine %d, coarse %d);' % (sum(k[0] == 0 for k in nv_keys), sum(k[0] == 1 for k in nv_keys)),
      'local sources', local, '; to download', len(keys) - local,
      '; pack bytes ~%.2f GB' % ((sum(k[0] == 0 for k in keys) * (FINE_BYTES + 32) + sum(k[0] == 1 for k in keys) * (COARSE_BYTES + 32)) / 1e9), flush=True)
if args.plan: raise SystemExit

def decode_v31(path, size):
    """Level 0 of a v31 RGB565 Morton tile, in texture-memory row order."""
    v = np.frombuffer(path.read_bytes()[32:32 + size * size * 2], dtype='<u2')
    y, x = np.indices((size, size)); z = np.zeros((size, size), dtype=np.int64)
    for b in range(3): z |= ((x >> b) & 1) << (2 * b); z |= ((y >> b) & 1) << (2 * b + 1)
    px = v[(((y // 8) * (size // 8) + x // 8) * 64 + z).ravel()].reshape(size, size)
    r = ((px >> 11) & 31) * 255 // 31; g = ((px >> 5) & 63) * 255 // 63; b = (px & 31) * 255 // 31
    return Image.fromarray(np.dstack([r, g, b]).astype(np.uint8))

def encode(key, memory_image):
    tier, x, y = key; size = 1024 if tier == 0 else 512; level = 7 if tier == 0 else 6
    tmp = scratch / (name(key) + '.png'); bin_ = scratch / (name(key) + '.bin')
    memory_image.save(tmp)
    subprocess.run([TEX3DS, '-f', 'etc1', '-q', 'medium', '-m', 'lanczos', '-r', '-z', 'none', '-o', str(bin_), str(tmp)], check=True, capture_output=True)
    data = bin_.read_bytes(); tmp.unlink(); bin_.unlink()
    payload = data[4:]; assert len(payload) == (FINE_BYTES if tier == 0 else COARSE_BYTES), (key, len(payload))
    assert data[0] == 0 and int.from_bytes(data[1:4], 'little') == len(payload)
    raw = b'CFSTRM16' + struct.pack('<2i4I', x, y, size, level, len(payload), 0) + payload
    dest = out / (name(key) + '.cft'); part = dest.with_suffix('.tmp'); part.write_bytes(raw); part.replace(dest)
    return {'tier': tier, 'x': x, 'y': y, 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}

def covered(a): return a.std() > 10 and (a.max(2) > 5).mean() > .99

def build(key, png):
    tier = key[0]; size = 1024 if tier == 0 else 512
    dest = out / (name(key) + '.cft')
    if dest.exists() and dest.stat().st_size == (FINE_BYTES if tier == 0 else COARSE_BYTES) + 32 and dest.read_bytes()[:8] == b'CFSTRM16':
        raw = dest.read_bytes(); return {'tier': tier, 'x': key[1], 'y': key[2], 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest(), 'source': 'kept'}
    if png and png.exists():
        image = Image.open(png).convert('RGB'); assert image.size == (size, size)
        a = np.asarray(image)
        if not covered(a): return {'tier': tier, 'x': key[1], 'y': key[2], 'excluded': 'no imagery coverage'}
        r = encode(key, image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)); r['source'] = 'png'; return r
    v31 = old / (name(key) + '.cft')
    if v31.exists():
        r = encode(key, decode_v31(v31, size)); r['source'] = 'v31 tile'; return r
    return {'tier': tier, 'x': key[1], 'y': key[2], 'excluded': 'no source'}

def build_group(group):
    """v31 batching: up to 2x2 tiles per export on one exact shared pixel lattice."""
    (tier, gx, gy), items = group; size = 1024 if tier == 0 else 512; span = 2048 if tier == 0 else 8192
    missing = [k for k in items if not (cache / (name(k) + '.png')).exists() and not (old / (name(k) + '.cft')).exists()
               and not (out / (name(k) + '.cft')).exists()]
    if missing and not args.offline:
        step = span / (size - 2); dimension = 2 * (size - 2) + 2; e = gx * 2 * span; n = gy * 2 * span
        bbox = [lon0 + (e - step) / mx, lat0 + (n - step) / m, lon0 + (e + 2 * span + step) / mx, lat0 + (n + 2 * span + step) / m]
        params = {'f': 'json', 'bbox': ','.join(map(str, bbox)), 'bboxSR': 4326, 'imageSR': 4326, 'size': f'{dimension},{dimension}',
                  'format': 'png', 'bandIds': '0,1,2', 'renderingRule': json.dumps({'rasterFunction': 'NaturalColor'}),
                  'interpolation': 'RSP_BilinearInterpolation', 'adjustAspectRatio': 'false'}
        request = base + '/exportImage?' + urllib.parse.urlencode(params); batch = cache / f'batch2_{tier}_{gx}_{gy}.png'
        for attempt in range(5):
            try:
                if not batch.exists():
                    response = json.load(urllib.request.urlopen(request, timeout=180))
                    if 'href' not in response: raise RuntimeError(str(response)[:300])
                    data = urllib.request.urlopen(response['href'], timeout=180).read(); part = batch.with_suffix('.part')
                    part.write_bytes(data); part.replace(batch)
                image = Image.open(batch).convert('RGB'); assert image.size == (dimension, dimension)
                for _, x, y in missing:
                    col = x - gx * 2; row = y - gy * 2; px = col * (size - 2); py = (1 - row) * (size - 2)
                    image.crop((px, py, px + size, py + size)).save(cache / (name((tier, x, y)) + '.png'))
                break
            except Exception as error:
                if tier == 1 and '400' in str(error) and not batch.exists():
                    return [{'tier': tier, 'x': k[1], 'y': k[2], 'excluded': 'outside imagery service extent'} for k in items]
                try:
                    if batch.exists(): batch.unlink()
                except OSError: time.sleep(1)
                if attempt == 4: raise
                time.sleep(3 * (attempt + 1))
    return [build(k, cache / (name(k) + '.png')) for k in items]

groups = {}
for key in keys: groups.setdefault((key[0], key[1] // 2, key[2] // 2), []).append(key)
results = []; failed = []; started = time.time()
with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, min(args.workers, 6))) as pool:
    futures = {pool.submit(build_group, g): g[0] for g in groups.items()}
    for n, future in enumerate(concurrent.futures.as_completed(futures), 1):
        try: results.extend(future.result())
        except Exception as e: failed.append({'batch': futures[future], 'error': str(e)}); print('FAILED', failed[-1], flush=True)
        if n % 50 == 0 or n == len(futures):
            print('PROGRESS groups %d/%d tiles %d failed %d %.0f s' % (n, len(futures), len(results), len(failed), time.time() - started), flush=True)
excluded = [t for t in results if 'excluded' in t]; results = [t for t in results if 'excluded' not in t]
built = {(t['tier'], t['x'], t['y']) for t in results}
keys = [k for k in keys if k in built]
(root / 'data/corridor_index.bin').write_bytes(b'CFINDEX5' + struct.pack('<2I', 1, len(keys)) + b''.join(struct.pack('<3i', *k) for k in keys))
for f in out.glob('*.cft'):
    tier = 0 if f.name.startswith('f_') else 1; x, y = map(int, f.stem.split('_')[1:])
    if (tier, x, y) not in built: f.unlink()
meta = {'version': 16, 'tile_format': 'CFSTRM16 + x,y,dimension,maxLevel,payloadBytes,reserved (32-byte header), ETC1 3DS-tiled mip payload (tex3ds -f etc1 -q medium -m lanczos)',
        'folder': 'scenery/stream32', 'states': ['Arizona (v31 catalog)', 'Nevada (v32)'], 'nevada_airports': sorted(NV_IDS),
        'nevada_state_buffer_m': STATE_BUFFER, 'airport_fine_radius_m': AIRPORT_FINE, 'source': base,
        'attribution': 'USGS, USDA, The National Map: Orthoimagery', 'build_date': str(datetime.date.today()),
        'fine_count': sum(k[0] == 0 for k in keys), 'coarse_count': sum(k[0] == 1 for k in keys),
        'total_bytes': sum(t['bytes'] for t in results), 'excluded': sorted(excluded, key=lambda t: (t['tier'], t['x'], t['y'])),
        'failures': failed, 'tiles': sorted(results, key=lambda t: (t['tier'], t['x'], t['y']))}
(root / 'data/corridor_metadata_v32.json').write_text(json.dumps(meta, indent=1))
(out / 'manifest.json').write_text(json.dumps({'version': 16, 'fine': meta['fine_count'], 'coarse': meta['coarse_count'], 'files': len(results),
                                              'complete': not failed, 'source': base, 'attribution': meta['attribution']}, indent=2))
print('DONE', len(results), 'tiles', meta['total_bytes'], 'bytes; excluded', len(excluded), '; failed', len(failed), flush=True)
if failed: raise SystemExit(1)
