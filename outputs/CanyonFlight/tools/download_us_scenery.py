"""Download source scenery for the 48 contiguous US states (for later game builds).

Public USGS services, the same as the Arizona/Nevada packs:
  NAIP natural colour  https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPImagery/ImageServer
  3DEP elevation       https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer
Attribution: USGS, USDA, The National Map.

Tiles are plain latitude/longitude (EPSG:4326), independent of the game's
Sedona-centred frame, so any later projection can be built from them:
  elevation/e_<lat>_<lon>.tif   1 x 1 deg, 1201 x 1201 float32, 3 arc-second (~90 m),
                                pixel centres on the whole-degree grid (rows south->north as exported)
  coarse/<j>/c_<j>_<i>.png      0.125 deg cells, 1024 x 1024 (~14 m/px N-S), cell (i,j) spans
                                lon -125+i/8 .. +1/8, lat 24+j/8 .. +1/8
  fine/<j>/f_<j>_<i>.png        0.02 deg cells, 1024 x 1024 (~2.2 m/px N-S), lon -125+i*0.02,
                                lat 24+j*0.02, within 3.5 km of runways at 433 major airports
                                (OurAirports large airports + medium with scheduled service)
Empty imagery (ocean, Canada, Mexico) is not kept; manifest.jsonl records every tile.
Resumable: existing files are skipped. Four requests at a time with back-off.

  python -P outputs/CanyonFlight/tools/download_us_scenery.py F:/CirrusFlightData/us [--plan] [--only elevation|fine|coarse] [--west-first]
         [--source pc] [--workers N]

--source pc (v35.08) takes the NAIP photos from Microsoft's Planetary Computer instead of the
USGS image server (which slowed to ~90 s a request in Nov 2026): the same NAIP quarter-quads as
cloud-optimised GeoTIFFs, read over HTTP at the matching overview and reprojected here to the
same 1024 x 1024 EPSG:4326 PNG. Per state only the two newest NAIP years are used, newest first,
so each pixel shows the latest photo. Needs rasterio. https://planetarycomputer.microsoft.com/dataset/naip
All Planetary Computer traffic goes through the curl executable: on this PC AVG's Web Shield
intercepts Python's (and GDAL's own) HTTPS and cuts Microsoft's responses short, while curl
passes. The GeoTIFFs are read through a small curl-backed file object (512 KB range blocks,
cached) handed to rasterio's opener, so only the needed overview tiles are downloaded.
"""
import sys, os, json, math, time, csv, threading, urllib.request, urllib.parse, concurrent.futures
from pathlib import Path
import numpy as np
from PIL import Image
os.environ.pop('SSLKEYLOGFILE', None)
Image.MAX_IMAGE_PIXELS = None
NAIP = 'https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPImagery/ImageServer/exportImage'
DEP = 'https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer/exportImage'
root = Path(sys.argv[1]); plan = '--plan' in sys.argv
source = sys.argv[sys.argv.index('--source') + 1] if '--source' in sys.argv else 'usgs'
workers = int(sys.argv[sys.argv.index('--workers') + 1]) if '--workers' in sys.argv else (12 if source == 'pc' else 4)
cells_arg = sys.argv[sys.argv.index('--cells') + 1] if '--cells' in sys.argv else None   # test: "j,i;j,i"

only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else None
repo = Path(__file__).resolve().parent.parent.parent.parent
UA = {'User-Agent': '3DMicroFlight scenery builder (non-commercial)'}

# Simplified contiguous-US outline (lon, lat); cells near it are kept with a margin.
CONUS = [(-124.7, 48.4), (-123.0, 49.0), (-95.15, 49.0), (-95.15, 49.4), (-89.5, 48.0), (-84.5, 46.5), (-82.5, 45.3),
         (-82.4, 43.0), (-79.0, 43.3), (-76.5, 44.2), (-74.7, 45.0), (-71.5, 45.0), (-70.0, 46.7), (-69.2, 47.45),
         (-67.8, 47.1), (-67.0, 45.0), (-67.0, 44.5), (-70.0, 43.7), (-70.6, 42.6), (-69.9, 41.7), (-71.9, 41.3),
         (-74.0, 40.5), (-74.0, 39.0), (-75.5, 38.0), (-75.8, 36.0), (-76.5, 34.7), (-78.5, 33.8), (-80.8, 32.0),
         (-81.4, 30.5), (-80.0, 26.8), (-80.4, 25.2), (-81.1, 25.1), (-82.0, 26.5), (-82.8, 28.0), (-84.0, 30.0),
         (-85.4, 29.7), (-88.0, 30.4), (-89.6, 29.0), (-90.5, 29.1), (-93.8, 29.7), (-94.8, 29.3), (-97.2, 27.7),
         (-97.4, 25.9), (-99.1, 26.5), (-100.3, 28.3), (-101.4, 29.8), (-103.0, 29.0), (-104.5, 29.6), (-106.5, 31.8),
         (-108.2, 31.8), (-108.2, 31.3), (-111.1, 31.3), (-114.8, 32.5), (-117.1, 32.5), (-117.3, 33.2), (-118.5, 34.0),
         (-120.6, 34.6), (-120.9, 35.4), (-122.5, 37.5), (-123.8, 39.5), (-124.4, 40.4), (-124.2, 42.0), (-124.6, 43.0),
         (-124.0, 46.2), (-124.7, 48.4)]
def inside(x, y):
    c = False
    for (x1, y1), (x2, y2) in zip(CONUS, CONUS[1:]):
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1: c = not c
    return c
def edge(x, y):
    r = 1e9
    for (x1, y1), (x2, y2) in zip(CONUS, CONUS[1:]):
        dx, dy = x2 - x1, y2 - y1; t = max(0, min(1, ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy)))
        r = min(r, math.hypot(x - x1 - t * dx, y - y1 - t * dy))
    return r
def near(x, y, margin): return inside(x, y) or edge(x, y) <= margin

# ---------------------------------------------------------------- tile lists
elev = [(lat, lon) for lat in range(24, 50) for lon in range(-125, -66) if near(lon + .5, lat + .5, .8)]
coarse = [(j, i) for j in range(int(26 * 8)) for i in range(int(59 * 8))
          if near(-125 + (i + .5) / 8, 24 + (j + .5) / 8, .15)]
airports = {a['ident']: a for a in csv.DictReader(open(repo / 'work/ourairports/airports.csv', encoding='utf-8'))}
major = {k: a for k, a in airports.items() if a['iso_country'] == 'US' and a['iso_region'] not in ('US-AK', 'US-HI')
         and (a['type'] == 'large_airport' or (a['type'] == 'medium_airport' and a['scheduled_service'] == 'yes'))}
fine = set(); R = 3500; M = 111195
for r in csv.DictReader(open(repo / 'work/ourairports/runways.csv', encoding='utf-8')):
    if r['airport_ident'] not in major or r['closed'] != '0' or not r['le_latitude_deg'] or not r['he_latitude_deg']: continue
    a = (float(r['le_longitude_deg']), float(r['le_latitude_deg'])); b = (float(r['he_longitude_deg']), float(r['he_latitude_deg']))
    kx = M * math.cos(math.radians(a[1]))
    lo_lon, hi_lon = min(a[0], b[0]) - R / kx - .02, max(a[0], b[0]) + R / kx + .02
    lo_lat, hi_lat = min(a[1], b[1]) - R / M - .02, max(a[1], b[1]) + R / M + .02
    for j in range(int((lo_lat - 24) / .02), int((hi_lat - 24) / .02) + 1):
        for i in range(int((lo_lon + 125) / .02), int((hi_lon + 125) / .02) + 1):
            cx, cy = -125 + (i + .5) * .02, 24 + (j + .5) * .02
            ax, ay, bx, by, px, py = a[0] * kx, a[1] * M, b[0] * kx, b[1] * M, cx * kx, cy * M
            dx, dy = bx - ax, by - ay; t = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / max(dx * dx + dy * dy, 1)))
            if math.hypot(px - ax - t * dx, py - ay - t * dy) <= R + 1600: fine.add((j, i))
fine = sorted(fine)
print(f'PLAN elevation {len(elev)} tiles (~{len(elev)*5.8/1024:.1f} GB), airports {len(major)} -> fine {len(fine)} tiles '
      f'(~{len(fine)*1.8/1024:.0f} GB), coarse {len(coarse)} cells (~{len(coarse)*1.6/1024:.0f} GB before empty ones are dropped)', flush=True)
if plan: raise SystemExit

# ---------------------------------------------------------------- download
root.mkdir(parents=True, exist_ok=True)
lock = threading.Lock(); manifest = open(root / 'manifest.jsonl', 'a', encoding='utf-8')
def record(entry):
    with lock: manifest.write(json.dumps(entry) + '\n'); manifest.flush()
done = set()
if (root / 'manifest.jsonl').exists():
    for line in open(root / 'manifest.jsonl', encoding='utf-8'):
        try: e = json.loads(line); done.add(e['file'])
        except Exception: pass
def fetch(service, bbox, w, h, fmt, extra):
    params = {'f': 'json', 'bbox': ','.join(map(str, bbox)), 'bboxSR': 4326, 'imageSR': 4326, 'size': f'{w},{h}',
              'format': fmt, 'interpolation': 'RSP_BilinearInterpolation', 'adjustAspectRatio': 'false', **extra}
    url = service + '?' + urllib.parse.urlencode(params)
    for attempt in range(8):
        try:
            response = json.load(urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=180))
            if 'href' not in response: raise RuntimeError(str(response)[:200])
            return urllib.request.urlopen(urllib.request.Request(response['href'], headers=UA), timeout=300).read(), url
        except Exception as e:
            if '400' in str(e) and attempt >= 2: return None, url        # outside the service extent
            time.sleep(min(300, 10 * 2 ** attempt))
    raise RuntimeError('failed: ' + url)
# ---------------------------------------------------------------- Planetary Computer NAIP
import subprocess, io
PC_STAC = 'https://planetarycomputer.microsoft.com/api/stac/v1/search'
PC_TOKEN = 'https://planetarycomputer.microsoft.com/api/sas/v1/token/naip'
CURL = ['curl', '-s', '-f', '--retry', '4', '--retry-delay', '3', '--retry-all-errors', '--connect-timeout', '30', '--max-time', '180']
def curl(url, extra=(), data=None):
    r = subprocess.run(CURL + list(extra) + [url], input=data, capture_output=True)
    if r.returncode: raise RuntimeError(f'curl exit {r.returncode} {url[:120]}')
    return r.stdout
token_lock = threading.Lock(); token = {'value': None, 'until': 0}
def pc_token():
    with token_lock:
        if time.time() > token['until']:
            token['value'] = json.loads(curl(PC_TOKEN))['token']; token['until'] = time.time() + 1800   # ~1 h tokens; renew at 30 min
        return token['value']
def pc_items(bbox):
    body = json.dumps({'collections': ['naip'], 'bbox': bbox, 'limit': 250,
                       'fields': {'include': ['id', 'properties.naip:year', 'properties.naip:state', 'properties.datetime',
                                              'assets.image.href'], 'exclude': ['links', 'geometry']}}).encode()
    return json.loads(curl(PC_STAC, ['-X', 'POST', '-H', 'Content-Type: application/json', '--data-binary', '@-'], body))['features']
class CurlFile(io.RawIOBase):
    """Seekable read-only view of a remote file; 512 KB range blocks fetched with curl and cached."""
    BLOCK = 1 << 19
    def __init__(self, url): self.url, self.pos, self.cache, self.size = url, 0, {}, None; self._block(0)
    def _block(self, k):
        if k not in self.cache:
            raw = curl(self.url, ['-D', '-', '-r', f'{k * self.BLOCK}-{(k + 1) * self.BLOCK - 1}'])
            head, _, body = raw.partition(b'\r\n\r\n')
            if self.size is None:
                for line in head.split(b'\r\n'):
                    if line.lower().startswith(b'content-range:'): self.size = int(line.rsplit(b'/', 1)[1])
                if self.size is None: self.size = len(body)
            self.cache[k] = body
        return self.cache[k]
    def readable(self): return True
    def seekable(self): return True
    def seek(self, off, whence=0):
        self.pos = off if whence == 0 else self.pos + off if whence == 1 else self.size + off; return self.pos
    def tell(self): return self.pos
    def read(self, n=-1):
        if n is None or n < 0: n = self.size - self.pos
        n = max(0, min(n, self.size - self.pos)); out = bytearray()
        while n > 0:
            k, o = divmod(self.pos, self.BLOCK); b = self._block(k)[o:o + n]
            if not b: break
            out += b; self.pos += len(b); n -= len(b)
        return bytes(out)
    def readinto(self, buf):
        d = self.read(len(buf)); buf[:len(d)] = d; return len(d)
pc_urls = {}
def pc_open(fp, mode="rb", **kwds):
    # rasterio treats the dataset name as a path (a URL would be mangled) and first probes the
    # opener with a dummy name, so names map to URLs here and unknown names get an empty file.
    url = pc_urls.get(str(fp).replace(chr(92), '/'))
    return CurlFile(url) if url else io.BytesIO(b'')
def pc_image(bbox, size):
    import rasterio, rasterio.warp, rasterio.windows, rasterio.transform
    from rasterio.enums import Resampling
    items = pc_items(bbox)
    years = {}
    for it in items:
        p = it['properties']; years.setdefault(p.get('naip:state'), set()).add(int(p.get('naip:year', it['properties']['datetime'][:4])))
    keep = {st: sorted(ys)[-2:] for st, ys in years.items()}
    items = [it for it in items if int(it['properties'].get('naip:year', it['properties']['datetime'][:4])) in keep[it['properties'].get('naip:state')]]
    items.sort(key=lambda it: it['properties']['datetime'], reverse=True)      # newest first
    dst = np.zeros((3, size, size), np.uint8); filled = np.zeros((size, size), bool)
    dt = rasterio.transform.from_bounds(*bbox, size, size)
    res_m = (bbox[3] - bbox[1]) * 111195 / size                                    # target metres per pixel
    for it in items:
        if filled.mean() > .999: break                                      # newest photos cover the cell
        href = it['assets']['image']['href'] + '?' + pc_token(); tmp = None
        for attempt in range(3):
            try:
                name = f'pcnaip/{it["id"]}.tif'; pc_urls[name] = href
                with rasterio.open(name, opener=pc_open) as src:
                    b = rasterio.warp.transform_bounds('EPSG:4326', src.crs, *bbox, densify_pts=21)
                    try:
                        win = rasterio.windows.from_bounds(*b, transform=src.transform).intersection(
                            rasterio.windows.Window(0, 0, src.width, src.height))
                    except rasterio.errors.WindowError: break
                    scale = max(1.0, res_m / abs(src.transform.a))                     # GDAL picks the overview at or finer than this
                    out_h, out_w = max(1, int(win.height / scale)), max(1, int(win.width / scale))
                    arr = src.read([1, 2, 3], window=win, out_shape=(3, out_h, out_w), resampling=Resampling.average)
                    wt = rasterio.windows.transform(win, src.transform) * rasterio.Affine.scale(win.width / out_w, win.height / out_h)
                    tmp = np.zeros_like(dst)
                    rasterio.warp.reproject(arr, tmp, src_transform=wt, src_crs=src.crs, dst_transform=dt, dst_crs='EPSG:4326',
                                            src_nodata=0, dst_nodata=0, resampling=Resampling.bilinear)
                break
            except Exception:
                if attempt == 2: raise
                token['until'] = 0; href = it['assets']['image']['href'] + '?' + pc_token(); time.sleep(5 * 2 ** attempt)
        if tmp is None: continue
        new = (tmp.max(0) > 0) & ~filled
        dst[:, new] = tmp[:, new]; filled |= new
    return np.ascontiguousarray(dst.transpose(1, 2, 0)), len(items)
def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True); part = path.with_suffix(path.suffix + '.part')
    part.write_bytes(data); part.replace(path)
def elevation_tile(t):
    lat, lon = t; name = f'elevation/e_{lat}_{lon}.tif'; path = root / name
    if name in done or path.exists(): return
    d = 1 / 1200; bbox = [lon - d / 2, lat - d / 2, lon + 1 + d / 2, lat + 1 + d / 2]
    data, url = fetch(DEP, bbox, 1201, 1201, 'tiff', {'pixelType': 'F32', 'renderingRule': json.dumps({'rasterFunction': 'None'})})
    if data is None: record({'file': name, 'excluded': 'outside service'}); return
    save(path, data); record({'file': name, 'bytes': len(data), 'request': url})
def image_tile(kind, t):
    j, i = t; step = .125 if kind == 'coarse' else .02
    name = f'{kind}/{j}/{kind[0]}_{j}_{i}.png'; path = root / name
    if name in done or path.exists(): return
    lon0, lat0 = -125 + i * step, 24 + j * step
    if source == 'pc':
        a, n = pc_image([lon0, lat0, lon0 + step, lat0 + step], 1024)
        covered = float((a.max(2) > 5).mean())
        if n == 0 or a.std() < 4 or covered < .02:
            record({'file': name, 'excluded': 'no imagery', 'covered': round(covered, 4), 'source': 'pc'}); return
        from io import BytesIO
        buf = BytesIO(); Image.fromarray(a).save(buf, 'PNG'); data = buf.getvalue()
        save(path, data); record({'file': name, 'bytes': len(data), 'covered': round(covered, 4), 'source': 'pc', 'items': n}); return
    data, url = fetch(NAIP, [lon0, lat0, lon0 + step, lat0 + step], 1024, 1024, 'png',
                      {'bandIds': '0,1,2', 'renderingRule': json.dumps({'rasterFunction': 'NaturalColor'})})
    if data is None: record({'file': name, 'excluded': 'outside service'}); return
    from io import BytesIO
    a = np.asarray(Image.open(BytesIO(data)).convert('RGB'))
    covered = float((a.max(2) > 5).mean())
    if a.std() < 4 or covered < .02: record({'file': name, 'excluded': 'no imagery', 'covered': round(covered, 4)}); return
    save(path, data); record({'file': name, 'bytes': len(data), 'covered': round(covered, 4), 'request': url})
jobs = []
if only in (None, 'elevation'): jobs += [(elevation_tile, (t,)) for t in elev]
if only in (None, 'fine'): jobs += [(image_tile, ('fine', t)) for t in fine]
# --west-first: cells west of 102 W (Colorado's east edge) before the rest,
# so the western states (phase 1 of the nationwide plan) finish first.
if '--west-first' in sys.argv: coarse = sorted(coarse, key=lambda t: (-125 + t[1] / 8 >= -102, t))
if cells_arg: coarse = [tuple(int(v) for v in c.split(',')) for c in cells_arg.split(';')]
if only in (None, 'coarse'): jobs += [(image_tile, ('coarse', t)) for t in coarse]
start = time.time(); failed = 0
with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
    futures = [pool.submit(f, *a) for f, a in jobs]
    for n, future in enumerate(concurrent.futures.as_completed(futures), 1):
        try: future.result()
        except Exception as e: failed += 1; record({'error': str(e)[:300]})
        if n % 100 == 0 or n == len(futures):
            print(f'PROGRESS {n}/{len(futures)} failed {failed} {time.time()-start:.0f} s', flush=True)
print('DONE failed', failed, flush=True)
