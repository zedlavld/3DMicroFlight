"""v35 nationwide phase 1: the western states, built from the US download.

Converts F:/CirrusFlightData/us (tools/download_us_scenery.py: 1 deg 3DEP
elevation, 0.125 deg coarse and 0.02 deg fine NAIP cells, all EPSG:4326)
into the game's formats in the Sedona-centred equirectangular frame:

  elevation   scenery/elevation35/e_<i>_<j>.bin   AZEL v1, 257 x 257 int16, row 0 = south
              The v32 grid is extended 17 tiles west and to 84 x 78 tiles
              (lon -125.25 .. -101.7, lat 31.1 .. 49.1). The v32 Arizona and
              Nevada tiles (USGS 100 m exports) are kept and re-indexed; new
              tiles are sampled bilinearly from the 3 arc-second source, and
              where they meet a kept tile the shared edge is copied and the
              difference blended out over 16 samples, so there is no seam.
  photos      scenery/stream32/{f|c}<x>>4>_<y>>4>/{f|c}_<x>_<y>.cft  CFSTRM16 ETC1, same keys and
              pixel lattice as tools/build_corridor_v32.py (one-pixel overlap,
              PNG flipped to texture row order, tex3ds -f etc1 -q medium -m lanczos)
              Only new keys are written (to OUT/stream32); Arizona and Nevada
              keep their v32 tiles. Coarse tiles need >= 25% imagery (the rest
              is filled with the tile's mean colour); fine tiles need >= 98%.
  reindex     v35.10: elevation35 (v35.09 grid) -> elevation36 (grid moved 30 tiles south, 209 x 110)
  usbase      v35.10: data/elevation_us4000.bin, 4 km heights over the whole lower-48 grid
  base        data/elevation_west2000.bin: 2 km heights over the whole grid,
              embedded in the game for the far terrain (outside the AZ/NV bases)
  catalog     data/corridor_index.bin (v32 keys + every built West key) and
              OUT/stream32/manifest.json for the card.

Resumable: built tiles are skipped. A coarse key waits until every source
cell under it is downloaded or recorded as empty in manifest.jsonl, so this
can be rerun while the coarse download is still going.

  python -P outputs/CanyonFlight/tools/build_west_v35.py elevation|base|photos|catalog|all
         [--region lon0,lat0,lon1,lat1] [--workers 6] [--plan]
"""
from pathlib import Path
import argparse, math, json, struct, zlib, os, sys, time, subprocess, tempfile, hashlib, concurrent.futures, datetime
import numpy as np
from PIL import Image
Image.MAX_IMAGE_PIXELS = None
# v35.10: eight mostly-sea 3DEP squares (Palm Beach coast, Gulf coast, off Cape May) come from the
# service as a 1,606-byte header with no pixels, on every re-fetch; tif() reports them and treats
# them as absent, so the coast there is filled from the neighbouring squares.

LAT0 = 34.848598480225; LON0 = -111.78800201416
M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT0))
# Elevation grid; must match include/terrain_stream.h (v35).
SPACING = 100; TILE = 256; TM = TILE * SPACING
OLD_E0 = -793600; SHIFT = 17; E0 = OLD_E0 - SHIFT * TM
# v35.10: the grid covers the whole lower 48 (lon -125.25 .. -66.6, lat 24.26 .. 49.58). The south
# edge moved 30 tiles down, so tile j is the v35.09 j + 30 (folder elevation36, step 'reindex').
WEST_N0 = -409600; SHIFTJ = 30; N0 = WEST_N0 - SHIFTJ * TM; TX = 209; TY = 110
EAST = (-101.7, 24.4, -66.8, 49.5)
WEST = (-125.3, 31.0, -101.7, 49.2)            # lon0, lat0, lon1, lat1
BLEND = 16
FINE_BYTES, COARSE_BYTES = 699040, 174752

SRC = Path('F:/CirrusFlightData/us')
OUT = Path('F:/CirrusFlightData/build35')
OLD = Path(__file__).resolve().parent.parent / 'sdmc/3ds/CanyonFlight/scenery'
REPO = Path(__file__).resolve().parent.parent
TEX3DS = r'C:\devkitPro\tools\bin\tex3ds.exe'
V32_INDEX = REPO / 'validation' / 'corridor_index_v32.bin'   # the Arizona + Nevada catalog, kept for reruns
BASE_SPACING = 2000; BASE_W = 84 * TM // BASE_SPACING + 1; BASE_H = 78 * TM // BASE_SPACING + 1   # v35.09 West grid
US_SPACING = 4000; US_W = TX * TM // US_SPACING + 1; US_H = TY * TM // US_SPACING + 1            # v35.10 whole grid

def lonlat(e, n): return LON0 + e / MX, LAT0 + n / M
def east(lon): return (lon - LON0) * MX
def north(lat): return (lat - LAT0) * M
def in_region(lon, lat, r): return r[0] <= lon <= r[2] and r[1] <= lat <= r[3]

# ------------------------------------------------------------------ elevation
_tifs = {}
def tif(lat, lon):
    """1 x 1 deg 3DEP tile as float32, rows south->north, NaN where no data (None if absent)."""
    k = (lat, lon)
    if k in _tifs: return _tifs[k]
    p = SRC / 'elevation' / f'e_{lat}_{lon}.tif'; a = None
    if p.exists():
        try:
            a = np.asarray(Image.open(p), dtype=np.float32)[::-1].copy()
            a[~np.isfinite(a) | (a < -100) | (a > 5000)] = np.nan
        except OSError as e:                     # truncated download: treat as absent, report it
            print('BAD SOURCE', p.name, e, flush=True); a = None
    if len(_tifs) > 48: _tifs.pop(next(iter(_tifs)))
    _tifs[k] = a; return a

def sample_heights(e, n):
    lon, lat = lonlat(e, n)
    out = np.full(e.shape, np.nan, np.float32)
    la = np.floor(lat).astype(np.int64); lo = np.floor(lon).astype(np.int64)
    for code in np.unique(la * 1000 + (lo + 500)):
        a = int(code // 1000); b = int(code % 1000) - 500
        t = tif(a, b)
        if t is None: continue
        m = (la == a) & (lo == b)
        fy = (lat[m] - a) * 1200; fx = (lon[m] - b) * 1200
        y0 = np.clip(np.floor(fy).astype(np.int64), 0, 1199); x0 = np.clip(np.floor(fx).astype(np.int64), 0, 1199)
        wy = (fy - y0).astype(np.float32); wx = (fx - x0).astype(np.float32)
        out[m] = (t[y0, x0] * (1 - wx) + t[y0, x0 + 1] * wx) * (1 - wy) + (t[y0 + 1, x0] * (1 - wx) + t[y0 + 1, x0 + 1] * wx) * wy
    return out

def fill(a):
    """Push-pull fill of NaNs (as in build_elevation_v32.py)."""
    valid = np.isfinite(a)
    if valid.all(): return a
    levels = [(np.where(valid, a, 0).astype(np.float32), valid.astype(np.float32))]
    while min(levels[-1][0].shape) > 2:
        v, w = levels[-1]; h, wd = v.shape
        v = np.pad(v, ((0, h % 2), (0, wd % 2))); w = np.pad(w, ((0, h % 2), (0, wd % 2)))
        levels.append((v.reshape(v.shape[0] // 2, 2, v.shape[1] // 2, 2).sum((1, 3)), w.reshape(w.shape[0] // 2, 2, w.shape[1] // 2, 2).sum((1, 3))))
    v, w = levels[-1]; f = np.where(w > 0, v / np.maximum(w, 1e-9), np.nan); f = np.where(np.isnan(f), np.nanmean(f) if np.isfinite(f).any() else 0, f)
    for v, w in reversed(levels[:-1]):
        up = np.repeat(np.repeat(f, 2, 0), 2, 1)[:v.shape[0], :v.shape[1]]
        f = np.where(w > 0, v / np.maximum(w, 1e-9), up)
    return np.where(valid, a, f).astype(np.float32)

def old_tile(i, j):
    """v32 tile at new index (i, j) as int16 257x257, or None."""
    oi = i - SHIFT; oj = j - SHIFTJ
    if oi < 0 or oj < 0: return None
    p = OLD / 'elevation32' / f'e_{oi}_{oj}.bin'
    if not p.exists(): return None
    raw = p.read_bytes(); return np.frombuffer(raw[16:], '<i2').reshape(257, 257)

def write_tile(dest, i, j, heights):
    payload = np.ascontiguousarray(heights.astype('<i2')).tobytes()
    header = struct.pack('<4sHHHHI', b'AZEL', 1, i, j, 257, zlib.crc32(payload) & 0xffffffff)
    part = dest.with_suffix('.tmp'); part.write_bytes(header + payload); part.replace(dest)

def elevation(args):
    out = OUT / 'elevation36'; out.mkdir(parents=True, exist_ok=True)
    region = args.region or WEST
    kept = new = skipped = 0; started = time.time()
    order = [(i, j) for j in range(TY) for i in range(TX)]
    for n, (i, j) in enumerate(order):
        e0 = E0 + i * TM; n0 = N0 + j * TM
        lon, lat = lonlat(e0 + TM / 2, n0 + TM / 2)
        if not in_region(lon, lat, region): continue
        dest = out / f'e_{i}_{j}.bin'
        if dest.exists() and not args.force: continue
        o = old_tile(i, j)
        if o is not None:
            write_tile(dest, i, j, o); kept += 1; continue
        if args.plan: new += 1; continue
        k = np.arange(-BLEND, TILE + 1 + BLEND)
        E, N = np.meshgrid(e0 + k * SPACING, n0 + k * SPACING)      # rows south->north
        h = sample_heights(E, N)
        core = h[BLEND:BLEND + 257, BLEND:BLEND + 257]
        if np.isfinite(core).mean() < .02: skipped += 1; continue  # ocean / outside the US data
        h = fill(h)[BLEND:BLEND + 257, BLEND:BLEND + 257].copy()
        # meet kept v32 neighbours exactly at the shared edge, blend over BLEND samples
        ramp = (1 - np.arange(BLEND) / BLEND).astype(np.float32)
        w = old_tile(i - 1, j)
        if w is not None: d = w[:, 256].astype(np.float32) - h[:, 0]; h[:, :BLEND] += d[:, None] * ramp[None, :]
        w = old_tile(i + 1, j)
        if w is not None: d = w[:, 0].astype(np.float32) - h[:, 256]; h[:, -BLEND:] += d[:, None] * ramp[::-1][None, :]
        w = old_tile(i, j - 1)
        if w is not None: d = w[256, :].astype(np.float32) - h[0, :]; h[:BLEND, :] += d[None, :] * ramp[:, None]
        w = old_tile(i, j + 1)
        if w is not None: d = w[0, :].astype(np.float32) - h[256, :]; h[-BLEND:, :] += d[None, :] * ramp[::-1][:, None]
        write_tile(dest, i, j, np.clip(np.rint(h), -100, 4500)); new += 1
        if new % 50 == 0: print(f'ELEV new {new} kept {kept} skipped {skipped} {time.time() - started:.0f} s', flush=True)
    print(f'ELEV DONE new {new} kept {kept} skipped(ocean/no data) {skipped} {time.time() - started:.0f} s', flush=True)

def reindex(args):
    """v35.10: copy elevation35/e_i_j (v35.09 grid) to elevation36/e_i_<j+30> with the new j in the header."""
    src = OUT / 'elevation35'; out = OUT / 'elevation36'; out.mkdir(parents=True, exist_ok=True); n = 0
    for f in src.glob('e_*.bin'):
        i, j = map(int, f.stem.split('_')[1:]); dest = out / f'e_{i}_{j + SHIFTJ}.bin'
        if dest.exists(): continue
        raw = f.read_bytes(); assert raw[:4] == b'AZEL' and struct.unpack_from('<HH', raw, 6) == (i, j), f
        write_tile(dest, i, j + SHIFTJ, np.frombuffer(raw[16:], '<i2').reshape(257, 257)); n += 1
    print(f'REINDEX {n} tiles into {out}', flush=True)

def usbase(args):
    """v35.10: 4 km height base over the whole grid (data/elevation_us4000.bin, int16, row 0 = south),
    for the far terrain outside the 2 km West base. 1 km samples averaged 4 x 4; no source tile -> 0."""
    sub = 4; step = US_SPACING / sub; rows = []
    xs = E0 + (np.arange(US_W * sub) - (sub - 1) / 2) * step
    lon_x = LON0 + xs / MX
    for r in range(US_H):
        ys = N0 + r * US_SPACING + (np.arange(sub) - (sub - 1) / 2) * step
        E, N = np.meshgrid(xs, ys); h = sample_heights(E, N)
        lat_y = LAT0 + ys / M
        none = np.array([[tif(int(math.floor(b)), int(math.floor(c))) is None for c in lon_x] for b in lat_y])
        h[none] = 0
        rows.append(h.reshape(sub, US_W, sub))
        if r % 100 == 0: print(f'USBASE row {r}/{US_H}', flush=True)
    a = np.stack(rows)
    with np.errstate(all='ignore'):
        cnt = np.isfinite(a).sum((1, 3)); tot = np.nansum(a, (1, 3))
    grid = np.where(cnt > 0, tot / np.maximum(cnt, 1), np.nan).astype(np.float32)
    print('USBASE measured fraction %.3f' % np.isfinite(grid).mean(), flush=True)
    grid = np.clip(np.rint(fill(grid)), -100, 4500).astype('<i2')
    (REPO / 'data' / 'elevation_us4000.bin').write_bytes(grid.tobytes())
    print(f'USBASE {US_W} x {US_H} at {E0},{N0} spacing {US_SPACING}: {grid.nbytes} bytes, {grid.min()}..{grid.max()} m', flush=True)

def base(args):
    """2 km height base over the whole v35 grid (data/elevation_west2000.bin, int16, row 0 = south):
    the far terrain rings outside the Arizona and Nevada 500 m bases. 500 m samples averaged 4 x 4.
    Where no source tile exists at all (open Pacific, Mexico) the height is 0; gaps inside the
    data are filled."""
    sub = 4; step = BASE_SPACING / sub; rows = []
    xs = E0 + (np.arange(BASE_W * sub) - (sub - 1) / 2) * step
    for r in range(BASE_H):
        ys = WEST_N0 + r * BASE_SPACING + (np.arange(sub) - (sub - 1) / 2) * step
        E, N = np.meshgrid(xs, ys); h = sample_heights(E, N)
        lon, lat = lonlat(E, N)                        # no source tile at all: open ocean / Mexico -> sea level
        none = np.array([[tif(int(math.floor(b)), int(math.floor(c))) is None for b, c in zip(rb, rc)] for rb, rc in zip(lat, lon)])
        h[none] = 0
        rows.append(h.reshape(sub, BASE_W, sub))
        if r % 100 == 0: print(f'BASE row {r}/{BASE_H}', flush=True)
    a = np.stack(rows)                                      # H x sub x W x sub
    with np.errstate(all='ignore'):
        cnt = np.isfinite(a).sum((1, 3)); tot = np.nansum(a, (1, 3))
    grid = np.where(cnt > 0, tot / np.maximum(cnt, 1), np.nan).astype(np.float32)
    print('BASE measured fraction %.3f' % np.isfinite(grid).mean(), flush=True)
    grid = np.clip(np.rint(fill(grid)), -100, 4500).astype('<i2')
    (REPO / 'data' / 'elevation_west2000.bin').write_bytes(grid.tobytes())
    print(f'BASE {BASE_W} x {BASE_H} at {E0},{WEST_N0} spacing {BASE_SPACING}: {grid.nbytes} bytes, {grid.min()}..{grid.max()} m', flush=True)

# ------------------------------------------------------------------ photos
def name(k): return f'{"f" if k[0] == 0 else "c"}_{k[1]}_{k[2]}'
def rel(k):
    """v35.07 card layout: 16 x 16 subfolders, e.g. c-7_3/c_-100_50.cft (one flat FAT32 folder
    ran out of directory entries the 3DS can reach). Python's >> floors like the game's division."""
    return f'{"f" if k[0] == 0 else "c"}{k[1] >> 4}_{k[2] >> 4}/{name(k)}.cft'
def old_keys():
    raw = (V32_INDEX).read_bytes() if V32_INDEX.exists() else (REPO / 'data' / 'corridor_index.bin').read_bytes()
    assert raw[:8] == b'CFINDEX5'; count = struct.unpack_from('<I', raw, 12)[0]
    return {struct.unpack_from('<3i', raw, 16 + 12 * k) for k in range(count)}

def tile_lattice(tier, x, y):
    span = 2048 if tier == 0 else 8192; size = 1024 if tier == 0 else 512; step = span / (size - 2)
    c = (np.arange(size) + .5) * step
    return np.meshgrid(x * span - step + c, (y + 1) * span + step - c)   # PNG order: row 0 = north

def cells_under(kind, tier, x, y):
    E, N = tile_lattice(tier, x, y); step = .125 if kind == 'coarse' else .02
    lon, lat = lonlat(np.array([E.min(), E.max()]), np.array([N.min(), N.max()]))
    i0, i1 = int(math.floor((lon[0] + 125) / step)), int(math.floor((lon[1] + 125) / step))
    j0, j1 = int(math.floor((lat[0] - 24) / step)), int(math.floor((lat[1] - 24) / step))
    return [(j, i) for j in range(j0, j1 + 1) for i in range(i0, i1 + 1)]

_pngs = {}
def cell_image(kind, j, i):
    k = (kind, j, i)
    if k in _pngs: return _pngs[k]
    p = SRC / kind / str(j) / f'{kind[0]}_{j}_{i}.png'
    a = np.asarray(Image.open(p).convert('RGB')) if p.exists() else None
    if len(_pngs) > 24: _pngs.pop(next(iter(_pngs)))
    _pngs[k] = a; return a

def valid_pixels(a):
    """Pixels with real imagery. Black is outside the photo; v35.10: the Planetary Computer mosaic also
    fills gaps (sea off the Gulf coast, Mexico) with flat near-white (253,253,253). That white has no
    texture at all, so a near-white pixel whose 3x3 neighbourhood is perfectly flat counts as missing;
    real white ground (White Sands, salt flats, roofs) always varies a little and is kept."""
    v = a.max(2) > 5
    w = a.min(2) >= 250
    if w.any():
        g = a.astype(np.int16).sum(2); p = np.pad(g, 1, mode='edge'); h, wd = g.shape
        n = [p[dy:dy + h, dx:dx + wd] for dy in range(3) for dx in range(3)]
        flat = np.maximum.reduce(n) == np.minimum.reduce(n)
        v &= ~(w & flat)
    return v

def render(kind, tier, x, y):
    """Source mosaic sampled on the tile's lattice: (rgb uint8 HxWx3, valid bool HxW)."""
    E, N = tile_lattice(tier, x, y); step = .125 if kind == 'coarse' else .02
    lon, lat = lonlat(E, N)
    fi = (lon + 125) / step; fj = (lat - 24) / step
    i0, i1 = int(np.floor(fi.min())), int(np.floor(fi.max())); j0, j1 = int(np.floor(fj.min())), int(np.floor(fj.max()))
    H = (j1 - j0 + 1) * 1024; W = (i1 - i0 + 1) * 1024
    mos = np.zeros((H, W, 3), np.uint8); have = np.zeros((H, W), bool)
    for j in range(j0, j1 + 1):
        for i in range(i0, i1 + 1):
            a = cell_image(kind, j, i)
            if a is None: continue
            r = (j1 - j) * 1024; c = (i - i0) * 1024
            mos[r:r + 1024, c:c + 1024] = a; have[r:r + 1024, c:c + 1024] = valid_pixels(a)
    px = (fi - i0) * 1024 - .5; py = (j1 + 1 - fj) * 1024 - .5
    x0 = np.clip(np.floor(px).astype(np.int64), 0, W - 2); y0 = np.clip(np.floor(py).astype(np.int64), 0, H - 2)
    wx = np.clip(px - x0, 0, 1)[..., None].astype(np.float32); wy = np.clip(py - y0, 0, 1)[..., None].astype(np.float32)
    f = mos.astype(np.float32)
    rgb = (f[y0, x0] * (1 - wx) + f[y0, x0 + 1] * wx) * (1 - wy) + (f[y0 + 1, x0] * (1 - wx) + f[y0 + 1, x0 + 1] * wx) * wy
    valid = have[y0, x0] & have[y0, x0 + 1] & have[y0 + 1, x0] & have[y0 + 1, x0 + 1]
    return np.clip(np.rint(rgb), 0, 255).astype(np.uint8), valid

def encode(key, image, out):
    tier, x, y = key; size = 1024 if tier == 0 else 512; level = 7 if tier == 0 else 6
    scratch = Path(tempfile.gettempdir()) / 'cf35'; scratch.mkdir(exist_ok=True)
    tmp = scratch / (name(key) + f'_{os.getpid()}.png'); bin_ = tmp.with_suffix('.bin')
    image.transpose(Image.Transpose.FLIP_TOP_BOTTOM).save(tmp)
    subprocess.run([TEX3DS, '-f', 'etc1', '-q', 'medium', '-m', 'lanczos', '-r', '-z', 'none', '-o', str(bin_), str(tmp)], check=True, capture_output=True)
    data = bin_.read_bytes(); tmp.unlink(); bin_.unlink()
    payload = data[4:]; assert len(payload) == (FINE_BYTES if tier == 0 else COARSE_BYTES), (key, len(payload))
    raw = b'CFSTRM16' + struct.pack('<2i4I', x, y, size, level, len(payload), 0) + payload
    dest = out / rel(key); dest.parent.mkdir(exist_ok=True); part = dest.with_suffix('.tmp'); part.write_bytes(raw); part.replace(dest)

def build_keys(job):
    keys, out = job; results = []
    for key in keys:
        tier = key[0]; kind = 'fine' if tier == 0 else 'coarse'
        try:
            rgb, valid = render(kind, *key)
            frac = float(valid.mean())
            if frac < (.98 if tier == 0 else .25): results.append((key, 'sparse', frac)); continue
            if frac < 1:
                mean = rgb[valid].mean(0) if valid.any() else np.array([90, 90, 80])
                rgb = np.where(valid[..., None], rgb, np.rint(mean).astype(np.uint8))
            encode(key, Image.fromarray(rgb), out); results.append((key, 'built', frac))
        except Exception as e:
            results.append((key, 'error ' + str(e)[:200], 0))
    return results

def photo_keys(region, old):
    """Candidate West keys: coarse over the region; fine wherever fine cells were downloaded."""
    coarse = set(); span = 8192
    for x in range(math.floor(east(region[0]) / span), math.ceil(east(region[2]) / span)):
        for y in range(math.floor(north(region[1]) / span), math.ceil(north(region[3]) / span)):
            lon, lat = lonlat((x + .5) * span, (y + .5) * span)
            if in_region(lon, lat, region): coarse.add((1, x, y))
    fine = set(); span = 2048
    for d in (SRC / 'fine').iterdir():
        j = int(d.name)
        for f in d.iterdir():
            i = int(f.stem.split('_')[2]); lon = -125 + i * .02; lat = 24 + j * .02
            if not in_region(lon + .01, lat + .01, region): continue
            for x in range(math.floor(east(lon) / span) - 1, math.floor(east(lon + .02) / span) + 1):
                for y in range(math.floor(north(lat) / span) - 1, math.floor(north(lat + .02) / span) + 1):
                    fine.add((0, x, y))
    return sorted(coarse - old), sorted(fine - old)

def requested_coarse():
    """The coarse cells download_us_scenery.py asks for (the US outline plus a 0.15 degree margin).
    Cells outside it are never downloaded, so a coastal or border tile must not wait for them.
    The outline is read from the downloader so the two scripts cannot disagree."""
    src = (Path(__file__).parent / 'download_us_scenery.py').read_text(encoding='utf-8')
    ns = {'math': math}
    exec(src[src.index('CONUS'):src.index('# ---------------------------------------------------------------- tile lists')], ns)
    return {(j, i) for j in range(26 * 8) for i in range(59 * 8) if ns['near'](-125 + (i + .5) / 8, 24 + (j + .5) / 8, .15)}

def photos(args):
    out = OUT / 'stream32'; out.mkdir(parents=True, exist_ok=True)
    region = args.region or WEST; old = old_keys()
    coarse, fine = photo_keys(region, old)
    final = set()                                   # coarse cells that will not change any more
    for line in open(SRC / 'manifest.jsonl', encoding='utf-8'):
        try: e = json.loads(line)
        except Exception: continue
        if e.get('file', '').startswith('coarse/'): final.add(e['file'])
    requested = requested_coarse()
    def ready(key):
        for j, i in cells_under('coarse', *key):
            f = f'coarse/{j}/c_{j}_{i}.png'
            if (j, i) not in requested: continue    # outside the US: never downloaded, so never coming
            if f not in final and not (SRC / f).exists(): return False
        return True
    todo = [k for k in fine + coarse if not (out / rel(k)).exists()]
    waiting = [k for k in todo if k[0] == 1 and not ready(k)]; wait = set(waiting)
    todo = [k for k in todo if k not in wait]
    def has_source(key):                            # any source image under the tile at all?
        kind = 'fine' if key[0] == 0 else 'coarse'
        return any((SRC / kind / str(j) / f'{kind[0]}_{j}_{i}.png').exists() for j, i in cells_under(kind, *key))
    empty = [k for k in todo if not has_source(k)]; skip = set(empty)   # open sea, Canada, Mexico
    todo = [k for k in todo if k not in skip]
    print(f'PHOTOS candidates fine {len(fine)} coarse {len(coarse)}; to build {len(todo)}; coarse waiting for the download {len(waiting)}; '
          f'skipped with no source imagery {len(empty)}', flush=True)
    if args.plan: return
    todo.sort(key=lambda k: (k[0], k[2] // 4, k[1]))   # rows of nearby tiles share source cells
    jobs = [(todo[s:s + 24], out) for s in range(0, len(todo), 24)]
    built = sparse = errors = 0; started = time.time(); log = open(OUT / 'photos.log', 'a', encoding='utf-8')
    with concurrent.futures.ProcessPoolExecutor(max_workers=args.workers) as pool:
        for n, res in enumerate(pool.map(build_keys, jobs), 1):
            for key, status, frac in res:
                if status == 'built': built += 1
                elif status == 'sparse': sparse += 1
                else: errors += 1
                log.write(json.dumps({'key': key, 'status': status, 'coverage': round(frac, 4)}) + '\n')
            if n % 10 == 0 or n == len(jobs):
                log.flush(); print(f'PHOTOS {n * 24}/{len(todo)} built {built} sparse {sparse} errors {errors} {time.time() - started:.0f} s', flush=True)
    print(f'PHOTOS DONE built {built} sparse {sparse} errors {errors}; coarse still waiting {len(waiting)}', flush=True)

WATER_RGB = (70, 94, 108)   # v35.13: open water where there is no photo (NAIP lake/sea tone)

def _colormap_cell(p):
    """Mean photo colour of one 0.125 degree source cell in 16 x 16 blocks: (lon, lat, rgb sums, counts)."""
    j, i = int(p.parent.name), int(p.stem.split('_')[2])
    a = np.asarray(Image.open(p).convert('RGB')); v = valid_pixels(a)
    f = (a * v[..., None]).astype(np.float32).reshape(16, 64, 16, 64, 3).sum((1, 3))
    c = v.reshape(16, 64, 16, 64).sum((1, 3)).astype(np.float32)
    b = (np.arange(16) + .5) / 16 * .125
    lon = -125 + i * .125 + b[None, :].repeat(16, 0); lat = 24 + (j + 1) * .125 - b[:, None].repeat(16, 1)   # row 0 = north
    return lon.ravel(), lat.ravel(), f.reshape(-1, 3), c.ravel()

def colormap(args):
    """v35.13: ground colour for the whole grid on the 4 km US base lattice (sdcard/ground_us4000.bin -> card scenery/ground_us4000.bin,
    RGB uint8, row 0 = south). Mean photo colour where there are photos; open water (hydro-flattened
    elevation that is exactly level over the cell, or no elevation at all) in WATER_RGB; other gaps
    (Canada, Mexico) filled from the nearest coloured cells. Replaces the Arizona palette beyond the photos."""
    W, H = US_W, US_H
    def cell(e, n):
        c = np.floor((e - E0 + US_SPACING / 2) / US_SPACING).astype(np.int64); r = np.floor((n - N0 + US_SPACING / 2) / US_SPACING).astype(np.int64)
        ok = (c >= 0) & (c < W) & (r >= 0) & (r < H); return r * W + c, ok
    rgb = np.zeros((H * W, 3), np.float64); cnt = np.zeros(H * W, np.float64)
    files = sorted((SRC / 'coarse').rglob('c_*.png')); started = time.time()
    with concurrent.futures.ProcessPoolExecutor(max_workers=args.workers) as pool:
        for k, (lon, lat, f, c) in enumerate(pool.map(_colormap_cell, files, chunksize=32), 1):
            idx, ok = cell(east(lon), north(lat))
            np.add.at(cnt, idx[ok], c[ok]); np.add.at(rgb, idx[ok], f[ok])
            if k % 5000 == 0: print(f'COLORMAP photos {k}/{len(files)} {time.time() - started:.0f} s', flush=True)
    lo = np.full(H * W, 32767, np.int32); hi = np.full(H * W, -32768, np.int32); ecnt = np.zeros(H * W, np.int64)
    k100 = np.arange(257) * (TM / 256)
    for t in sorted((OUT / 'elevation36').glob('e_*.bin')):
        i, j = map(int, t.stem.split('_')[1:]); h = np.frombuffer(t.read_bytes()[16:], '<i2').reshape(257, 257).astype(np.int32)
        E, N = np.meshgrid(E0 + i * TM + k100, N0 + j * TM + k100)
        idx, ok = cell(E.ravel(), N.ravel()); idx = idx[ok]; hv = h.ravel()[ok]
        np.minimum.at(lo, idx, hv); np.maximum.at(hi, idx, hv); np.add.at(ecnt, idx, 1)
    print(f'COLORMAP elevation done {time.time() - started:.0f} s', flush=True)
    photo = cnt > 2000                                   # > ~1 % of the cell has real imagery
    water = ~photo & ((ecnt == 0) | ((ecnt > 100) & (lo == hi)))
    col = np.full((H * W, 3), np.nan, np.float32)
    col[photo] = (rgb[photo] / cnt[photo, None]).astype(np.float32); col[water] = WATER_RGB
    grid = col.reshape(H, W, 3)
    for ch in range(3): grid[..., ch] = fill(grid[..., ch])
    out = np.clip(np.rint(grid), 0, 255).astype(np.uint8)
    (REPO / 'sdcard' / 'ground_us4000.bin').write_bytes(out.tobytes())   # v35.14: not embedded (the code passed 32 MB and would not start)
    print(f'COLORMAP {W} x {H}: photo {photo.mean():.3f}, water {water.mean():.3f}, filled {(~photo & ~water).mean():.3f}; {out.nbytes} bytes', flush=True)
    Image.fromarray(out[::-1]).save(OUT / 'ground_us4000.png')

def catalog(args):
    out = OUT / 'stream32'; old = old_keys()
    if not V32_INDEX.exists(): V32_INDEX.write_bytes((REPO / 'data' / 'corridor_index.bin').read_bytes())
    new = set()
    for f in out.glob('*/*.cft'):
        if f.stat().st_size != (FINE_BYTES if f.name[0] == 'f' else COARSE_BYTES) + 32: continue
        x, y = map(int, f.stem.split('_')[1:]); new.add((0 if f.name[0] == 'f' else 1, x, y))
    keys = sorted(old | new)
    (REPO / 'data' / 'corridor_index.bin').write_bytes(b'CFINDEX5' + struct.pack('<2I', 1, len(keys)) + b''.join(struct.pack('<3i', *k) for k in keys))
    fine = sum(k[0] == 0 for k in keys); coarse = len(keys) - fine
    manifest = {'version': 17, 'layout': '16x16 subfolders: {f|c}<x>>4>_<y>>4>/{f|c}_<x>_<y>.cft', 'fine': fine, 'coarse': coarse, 'files': len(keys), 'complete': True,
                'source': 'https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPImagery/ImageServer',
                'attribution': 'USGS, USDA, The National Map: Orthoimagery', 'west_v35_tiles': len(new)}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    (REPO / 'data' / 'corridor_metadata_v35.json').write_text(json.dumps({
        'build_date': str(datetime.date.today()), 'v32_keys': len(old), 'west_keys': len(new),
        'west_fine': sum(k[0] == 0 for k in new), 'west_coarse': sum(k[0] == 1 for k in new),
        'elevation_grid': {'east': E0, 'north': N0, 'tiles_x': TX, 'tiles_y': TY, 'folder': 'scenery/elevation36'},
        'source': 'F:/CirrusFlightData/us (tools/download_us_scenery.py)', 'attribution': 'USGS, USDA, The National Map'}, indent=1))
    print(f'CATALOG {len(keys)} keys (v32 {len(old)}, West {len(new)}): fine {fine}, coarse {coarse}', flush=True)

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('step', choices=['reindex', 'elevation', 'base', 'usbase', 'colormap', 'photos', 'catalog', 'all'])
    ap.add_argument('--region', type=lambda s: tuple(map(float, s.split(','))))
    ap.add_argument('--workers', type=int, default=6); ap.add_argument('--plan', action='store_true'); ap.add_argument('--force', action='store_true')
    args = ap.parse_args()
    if args.step == 'reindex': reindex(args)
    if args.step == 'usbase': usbase(args)
    if args.step == 'colormap': colormap(args)
    if args.step in ('elevation', 'all'): elevation(args)
    if args.step in ('base', 'all') and not args.region: base(args)
    if args.step in ('photos', 'all'): photos(args)
    if args.step in ('catalog', 'all'): catalog(args)
