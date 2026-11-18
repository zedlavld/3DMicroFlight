"""Package the 3DMicroFlight SD-card scenery for a GitHub release (v1.0).

usage: python tools/package_release.py SRC OUT [--limit BYTES] [--extra FILE ...] [--verify]

SRC is a folder laid out like the SD card root (it contains 3ds/CanyonFlight/scenery).
Only what the game reads is packed: scenery/stream32 (manifest + tile folders),
scenery/elevation36, scenery/ground_us4000.bin and the region *.cft files.

Writes OUT/3DMicroFlight-scenery-01-of-NN.zip ... with paths from the SD root, so users
extract every zip to the card root and the folders merge. Each zip is a complete archive
(no split volumes) and stays under --limit (default 1.9e9 bytes; GitHub's release-asset
limit is 2 GiB): a file is added only if the zip so far plus that file's uncompressed size
fits. Files go in a fixed order (elevation and loose files first, then tile folders
sorted), with fixed timestamps, so the same input gives the same zips.

Also writes OUT/SHA256SUMS.txt for the zips and any --extra files (CIA, 3DSX), and
OUT/contents.json listing which zip holds which folder. --verify reopens every zip,
checks its CRCs and checks that each source file is in exactly one zip.
"""
from pathlib import Path
import sys, os, zipfile, hashlib, json, shutil, time

def files_to_pack(src):
    sc = src / '3ds' / 'CanyonFlight' / 'scenery'
    if not sc.is_dir():
        sys.exit(f'{sc} not found: SRC must be laid out like the SD card root')
    out = []
    loose = sorted(p for p in sc.iterdir() if p.is_file() and (p.suffix == '.cft' or p.name == 'ground_us4000.bin'))
    if not any(p.name == 'ground_us4000.bin' for p in loose):
        sys.exit('ground_us4000.bin missing')
    out += loose
    for sub in ('elevation36', 'stream32'):
        d = sc / sub
        if not d.is_dir():
            sys.exit(f'{d} missing')
        top = sorted(p for p in d.iterdir() if p.is_file())          # stream32/manifest.json
        out += top
        for folder in sorted(p for p in d.iterdir() if p.is_dir()):
            out += sorted(p for p in folder.rglob('*') if p.is_file())
    return out

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 22), b''):
            h.update(block)
    return h.hexdigest()

def main(argv):
    args = [a for a in argv if not a.startswith('--')]
    if len(args) < 2:
        sys.exit(__doc__)
    src, out = Path(args[0]), Path(args[1])
    limit = float(argv[argv.index('--limit') + 1]) if '--limit' in argv else 1.9e9
    extra = []
    if '--extra' in argv:
        i = argv.index('--extra') + 1
        while i < len(argv) and not argv[i].startswith('--'):
            extra.append(Path(argv[i])); i += 1
        args = [a for a in args if Path(a) not in extra]
    verify = '--verify' in argv
    out.mkdir(parents=True, exist_ok=True)
    files = files_to_pack(src)
    total = sum(p.stat().st_size for p in files)
    print(f'{len(files)} files, {total / 1e9:.2f} GB to pack, limit {limit / 1e9:.2f} GB per zip', flush=True)

    parts, current, zf, tmpname = [], [], None, None
    start = time.time(); done = 0
    def close():
        nonlocal zf
        if zf:
            zf.close(); parts.append((tmpname, current[:])); current.clear(); zf = None
    for p in files:
        size = p.stat().st_size
        if size > limit:
            sys.exit(f'{p} alone is larger than the limit')
        if zf and zf.fp.tell() + size + 1024 > limit:
            close()
        if not zf:
            tmpname = out / f'part{len(parts) + 1:02d}.zip.tmp'
            zf = zipfile.ZipFile(tmpname, 'w', zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True)
        arc = p.relative_to(src).as_posix()
        info = zipfile.ZipInfo(arc, date_time=(2026, 11, 17, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED; info.external_attr = 0o644 << 16
        with open(p, 'rb') as f, zf.open(info, 'w', force_zip64=size > 2**31) as w:
            shutil.copyfileobj(f, w, 1 << 20)
        current.append(arc); done += size
        if len(current) % 2000 == 0:
            el = time.time() - start
            print(f'  part {len(parts) + 1}: {done / total:6.1%} done, {el / 60:.0f} min, ~{el / max(done, 1) * (total - done) / 60:.0f} min left', flush=True)
    close()

    n = len(parts); names = []; contents = {}
    for i, (tmp, arcs) in enumerate(parts, 1):
        name = f'3DMicroFlight-scenery-{i:02d}-of-{n:02d}.zip'
        final = out / name
        if final.exists():
            final.unlink()
        tmp.rename(final); names.append(final)
        folders = sorted({'/'.join(a.split('/')[:5]) for a in arcs})
        contents[name] = {'files': len(arcs), 'bytes': final.stat().st_size, 'folders': folders}
        print(f'{name}: {len(arcs)} files, {final.stat().st_size / 1e9:.3f} GB', flush=True)
    (out / 'contents.json').write_text(json.dumps(contents, indent=1), encoding='utf-8')

    if verify:
        seen = {}
        for z in names:
            with zipfile.ZipFile(z) as zz:
                bad = zz.testzip()
                if bad:
                    sys.exit(f'{z.name}: bad CRC in {bad}')
                for a in zz.namelist():
                    seen[a] = seen.get(a, 0) + 1
        want = {p.relative_to(src).as_posix() for p in files}
        missing = want - set(seen); extra_in = set(seen) - want; twice = [a for a, c in seen.items() if c > 1]
        if missing or extra_in or twice:
            sys.exit(f'verify failed: {len(missing)} missing, {len(extra_in)} unexpected, {len(twice)} in two zips')
        print(f'verified: {len(want)} files, each in exactly one zip, CRCs good', flush=True)

    lines = [f'{sha256(p)}  {p.name}' for p in extra + names]
    (out / 'SHA256SUMS.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'wrote {n} zips, SHA256SUMS.txt and contents.json in {(time.time() - start) / 60:.0f} min', flush=True)

if __name__ == '__main__':
    main(sys.argv[1:])
