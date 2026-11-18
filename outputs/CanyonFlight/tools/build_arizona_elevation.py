"""Statewide Arizona elevation (v31).

Downloads USGS 3DEP bare-earth DEM at 100 m game-grid spacing over the whole
state and writes:
  sdmc/3ds/CanyonFlight/scenery/elevation/e_<i>_<j>.bin  streamed 100 m tiles
  data/elevation_arizona500.i16                         embedded 500 m base
  source/terrain_data.c                                 base + existing detail grids
  data/scenery_metadata.json                            provenance/checksums

Coordinates are the existing local equirectangular frame centred on Sedona
(see SCENERY.md). Spacing is resampling spacing, not source accuracy. Cells
with no 3DEP coverage (Mexico) are filled smoothly from surrounding terrain;
they are placeholders, not measured elevations.

Run from the repository root (a work/ download cache is created there):
    python -P outputs/CanyonFlight/tools/build_arizona_elevation.py
"""
from pathlib import Path
import urllib.request, urllib.parse, json, math, hashlib, os, struct, zlib, sys
import numpy as np
from PIL import Image

os.environ.pop('SSLKEYLOGFILE', None)
Image.MAX_IMAGE_PIXELS = None
root = Path(__file__).resolve().parent.parent
cache = Path.cwd() / 'work' / 'usgs_az'
cache.mkdir(parents=True, exist_ok=True)
base = 'https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer'
LAT = 34.848598480225; LON = -111.78800201416
M = 6371000 * math.pi / 180; MX = M * math.cos(math.radians(LAT))

# Must match include/terrain_stream.h.
SPACING = 100; TILE = 256; E0 = -281600; N0 = -409600; TX = 21; TY = 26
CHUNK = 8                       # tiles per export side (2049 samples max < 8000)
BASE_SPACING = 500; BASE_STEP = BASE_SPACING // SPACING
W = TX * TILE + 1; H = TY * TILE + 1
# Below -100 m is bathymetry/artefact near the Colorado delta and Gulf of
# California, or NoData; such cells are filled like missing coverage.
NODATA_BELOW = -100.0


def export(name, e, n, d, w, h):
    """Exact pixel-centre-on-grid export, as in build_usgs_scenery.py."""
    bbox = [LON + (e - d / 2) / MX, LAT + (n - d / 2) / M, LON + (e + (w - .5) * d) / MX, LAT + (n + (h - .5) * d) / M]
    params = {'f': 'json', 'bbox': ','.join(map(str, bbox)), 'bboxSR': 4326, 'imageSR': 4326, 'size': f'{w},{h}',
              'format': 'tiff', 'pixelType': 'F32', 'renderingRule': json.dumps({'rasterFunction': 'None'}),
              'interpolation': 'RSP_BilinearInterpolation', 'adjustAspectRatio': 'false'}
    url = base + '/exportImage?' + urllib.parse.urlencode(params)
    path = cache / (name + '.tif')
    if not path.exists():
        for attempt in range(4):
            try:
                response = json.load(urllib.request.urlopen(url, timeout=600))
                if 'href' not in response:
                    raise RuntimeError(response)
                tmp = path.with_suffix('.part')
                urllib.request.urlretrieve(response['href'], tmp)
                tmp.replace(path)
                (cache / (name + '.json')).write_text(json.dumps(response, indent=2))
                break
            except Exception as exc:  # transient service errors are common
                print('  retry', name, attempt, exc, flush=True)
        else:
            raise RuntimeError('export failed: ' + name)
    a = np.asarray(Image.open(path), dtype=np.float32)[::-1]
    assert a.shape == (h, w), (name, a.shape)
    return a, url


def neighbours(a):
    p = np.pad(a, 1, mode='edge')
    return [p[dy:dy + a.shape[0], dx:dx + a.shape[1]] for dy in range(3) for dx in range(3) if dy != 1 or dx != 1]


def clean_mask(full):
    """3DEP has no coverage south of the border; there the export returns
    noise (0, -0, denormals, NaN and isolated kilometre spikes), not a clean
    NoData value. Reject it so the area is filled smoothly instead."""
    valid = np.isfinite(full) & (full > NODATA_BELOW) & (full < 5000) & (np.abs(full) > 1e-3)
    # Isolated spikes: no real 100 m step in Arizona exceeds 800 m.
    a = np.where(valid, full, np.nan)
    spike = np.zeros_like(valid)
    for y0 in range(0, a.shape[0], 512):  # banded to bound memory
        lo = max(y0 - 1, 0); hi = min(y0 + 513, a.shape[0])
        band = a[lo:hi]
        with np.errstate(all='ignore'):
            import warnings
            with warnings.catch_warnings():
                warnings.simplefilter('ignore')
                med = np.nanmedian(np.stack(neighbours(band) + [band]), axis=0)
        s = np.abs(band - med) > 800
        spike[y0:min(y0 + 512, a.shape[0])] = s[y0 - lo:y0 - lo + min(512, a.shape[0] - y0)]
    valid &= ~spike
    # Speckle surviving inside the no-coverage region.
    for _ in range(2):
        bad = sum((~n).astype(np.int8) for n in neighbours(valid))
        valid &= bad < 4
    sw = np.zeros_like(valid); sw[:CHUNK * TILE + 1, :CHUNK * TILE + 1] = True
    print('rejected outside the SW chunk:', int((~valid & ~sw).sum()), 'cells', flush=True)
    return valid


def push_pull_fill(a, valid):
    """Fill invalid cells from a multi-scale average of valid neighbours."""
    if valid.all():
        return a
    levels = [(np.where(valid, a, 0).astype(np.float64), valid.astype(np.float64))]
    while min(levels[-1][0].shape) > 2:
        v, wgt = levels[-1]
        hh, ww = v.shape
        v = np.pad(v, ((0, hh % 2), (0, ww % 2))); wgt = np.pad(wgt, ((0, hh % 2), (0, ww % 2)))
        v = v.reshape(v.shape[0] // 2, 2, v.shape[1] // 2, 2).sum((1, 3))
        wgt = wgt.reshape(wgt.shape[0] // 2, 2, wgt.shape[1] // 2, 2).sum((1, 3))
        levels.append((v, wgt))
    v, wgt = levels[-1]
    filled = np.where(wgt > 0, v / np.maximum(wgt, 1e-9), np.nan)
    filled = np.where(np.isnan(filled), np.nanmean(filled), filled)
    for v, wgt in reversed(levels[:-1]):
        up = np.repeat(np.repeat(filled, 2, 0), 2, 1)[:v.shape[0], :v.shape[1]]
        here = np.where(wgt > 0, v / np.maximum(wgt, 1e-9), up)
        # Blend partly covered coarse cells toward the coarser estimate.
        frac = np.clip(wgt / wgt.max(), 0, 1) if wgt.max() > 0 else wgt
        filled = here * frac + up * (1 - frac) if v is not levels[0][0] else here
    return np.where(valid, a, filled).astype(np.float32)


def main():
    full_path = cache / 'arizona_100m.npy'
    meta_chunks = []
    if full_path.exists() and '--redownload' not in sys.argv:
        full = np.load(full_path)
        meta_chunks = json.loads((cache / 'chunks.json').read_text())
    else:
        full = np.full((H, W), np.nan, np.float32)
        for cy in range(0, TY, CHUNK):
            for cx in range(0, TX, CHUNK):
                tw = min(CHUNK, TX - cx); th = min(CHUNK, TY - cy)
                w = tw * TILE + 1; h = th * TILE + 1
                e = E0 + cx * TILE * SPACING; n = N0 + cy * TILE * SPACING
                name = f'az100_{cx // CHUNK}_{cy // CHUNK}'
                a, url = export(name, e, n, SPACING, w, h)
                x0 = cx * TILE; y0 = cy * TILE
                full[y0:y0 + h, x0:x0 + w] = a
                good = a > NODATA_BELOW
                meta_chunks.append({'name': name, 'east': e, 'north': n, 'width': w, 'height': h,
                                    'valid_fraction': round(float(good.mean()), 4), 'export_request': url})
                print(name, a.shape, 'valid %.3f' % good.mean(), 'min', float(a[good].min()) if good.any() else None,
                      'max', float(a[good].max()) if good.any() else None, flush=True)
        np.save(full_path, full)
        (cache / 'chunks.json').write_text(json.dumps(meta_chunks, indent=2))

    valid = clean_mask(full)
    print('valid fraction %.4f' % valid.mean(), flush=True)
    full = push_pull_fill(np.where(valid, full, 0), valid)
    heights = np.clip(np.rint(full), -100, 4500).astype('<i2')
    assert heights.min() >= -100 and heights.max() < 4500, (heights.min(), heights.max())

    # Streamed 100 m tiles, 257 x 257 samples sharing edges with neighbours.
    out = root / 'sdmc' / '3ds' / 'CanyonFlight' / 'scenery' / 'elevation'
    out.mkdir(parents=True, exist_ok=True)
    total = 0; manifest = []
    for j in range(TY):
        for i in range(TX):
            t = heights[j * TILE:j * TILE + TILE + 1, i * TILE:i * TILE + TILE + 1]
            payload = np.ascontiguousarray(t).tobytes()
            header = struct.pack('<4sHHHHI', b'AZEL', 1, i, j, TILE + 1, zlib.crc32(payload) & 0xffffffff)
            (out / f'e_{i}_{j}.bin').write_bytes(header + payload)
            total += len(header) + len(payload)
            measured = float(valid[j * TILE:j * TILE + TILE + 1, i * TILE:i * TILE + TILE + 1].mean())
            manifest.append({'i': i, 'j': j, 'measured_fraction': round(measured, 4)})
    print('tiles', TX * TY, 'bytes', total, flush=True)

    # Embedded 500 m base: binomial low-pass at 100 m, then decimate exactly.
    k = np.array([1, 4, 6, 4, 1], np.float64) / 16
    f = full.astype(np.float64)
    f = np.apply_along_axis(lambda r: np.convolve(np.pad(r, 2, mode='edge'), k, 'valid'), 1, f)
    f = np.apply_along_axis(lambda c: np.convolve(np.pad(c, 2, mode='edge'), k, 'valid'), 0, f)
    base500 = np.clip(np.rint(f[::BASE_STEP, ::BASE_STEP]), -100, 4500).astype('<i2')
    bh, bw = base500.shape
    raw500 = base500.tobytes()
    (root / 'data' / 'elevation_arizona500.i16').write_bytes(raw500)

    meta = json.loads((root / 'data' / 'scenery_metadata.json').read_text())
    old = {g['name']: g for g in meta['grids']}
    detail = [old[n] for n in ('sedona', 'flagstaff', 'canyon')]
    source = ['#include "terrain_data.h"',
              '/* Generated by tools/build_arizona_elevation.py; USGS 3DEP, modified. */']
    decl = []
    source.append(f'static const int16_t heights_arizona500[{bw * bh}]={{')
    source.extend(','.join(map(str, row)) + ',' for row in base500)
    source.append('};')
    decl.append(f'{{{E0},{N0},{BASE_SPACING},{bw},{bh},heights_arizona500}}')
    for g in detail:
        a = np.frombuffer((root / 'data' / f"elevation_{g['name']}.i16").read_bytes(), '<i2').reshape(g['height'], g['width'])
        assert hashlib.sha256(a.tobytes()).hexdigest() == g['sha256'], g['name']
        source.append(f"static const int16_t heights_{g['name']}[{g['width'] * g['height']}]={{")
        source.extend(','.join(map(str, row)) + ',' for row in a)
        source.append('};')
        decl.append(f"{{{g['east']},{g['north']},{g['spacing']},{g['width']},{g['height']},heights_{g['name']}}}")
    source.append('const ElevationGrid elevationGrids[4]={' + ','.join(decl) + '};')
    (root / 'source' / 'terrain_data.c').write_text('\n'.join(source) + '\n')

    meta['download_date_statewide'] = '2026-11-04'
    meta['grids'] = [{'name': 'arizona500', 'east': E0, 'north': N0, 'spacing': BASE_SPACING, 'width': bw, 'height': bh,
                      'min_m': int(base500.min()), 'max_m': int(base500.max()),
                      'sha256': hashlib.sha256(raw500).hexdigest(),
                      'derived_from': 'statewide 100 m stream, [1,4,6,4,1] binomial low-pass, decimated x5'}] + detail
    meta['statewide_stream'] = {'spacing': SPACING, 'tile_cells': TILE, 'tile_samples': TILE + 1, 'east': E0, 'north': N0,
                                'tiles_x': TX, 'tiles_y': TY, 'bytes': total,
                                'format': "16-byte header '<4sHHHHI' (AZEL, version 1, i, j, 257, crc32) + 257*257 int16 LE, row 0 = south",
                                'no_coverage_fill': 'push-pull multi-scale average; Mexico cells are placeholders',
                                'valid_fraction': round(float(valid.mean()), 4), 'chunks': meta_chunks}
    (root / 'data' / 'scenery_metadata.json').write_text(json.dumps(meta, indent=2))
    (root / 'data' / 'elevation_stream_manifest.json').write_text(json.dumps(manifest))
    print('base500', base500.shape, int(base500.min()), int(base500.max()), 'bytes', len(raw500), flush=True)


if __name__ == '__main__':
    main()
