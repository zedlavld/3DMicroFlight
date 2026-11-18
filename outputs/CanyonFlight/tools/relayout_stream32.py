"""Move a flat scenery/stream32 folder into the v35.07 subfolder layout, in place.

  python -P tools/relayout_stream32.py <stream32 folder> [--link-from <flat folder>]

  {f|c}_<x>_<y>.cft  ->  {f|c}<x>>4>_<y>>4>/{f|c}_<x>_<y>.cft   (16 x 16 tiles per folder)

One flat FAT32 folder of ~31,000 long names used ~63,700 directory entries and the 3DS
could not open files past ~60,000 (FS 0xC8A04573). Moves are renames on the same volume,
so the data is not rewritten. manifest.json is set to "version": 17, which v35.07 and later
require; older builds will not use a re-sorted card (they fall back to procedural ground).
--link-from builds the layout from another flat folder with hard links (same volume),
leaving that folder untouched (used for the Azahar sdmc test folder).
"""
import json, os, re, sys
from pathlib import Path

PAT = re.compile(r'^([fc])_(-?\d+)_(-?\d+)\.cft$')

def target(dest, name):
    m = PAT.match(name)
    if not m: return None
    t, x, y = m.group(1), int(m.group(2)), int(m.group(3))
    return dest / f'{t}{x >> 4}_{y >> 4}' / name

def main():
    args = sys.argv[1:]
    if not args: raise SystemExit(__doc__)
    dest = Path(args[0]); src = Path(args[args.index('--link-from') + 1]) if '--link-from' in args else dest
    dest.mkdir(parents=True, exist_ok=True); made = set(); n = 0
    names = [e.name for e in os.scandir(src) if e.is_file()]
    for name in names:
        t = target(dest, name)
        if t is None: continue
        if t.parent not in made: t.parent.mkdir(exist_ok=True); made.add(t.parent)
        if src == dest: os.replace(src / name, t)
        elif not t.exists(): os.link(src / name, t)
        n += 1
        if n % 2000 == 0: print(n, 'tiles', flush=True)
    man = (src / 'manifest.json')
    if man.exists():
        m = json.loads(man.read_text()); m['version'] = 17
        m['layout'] = '16x16 subfolders: {f|c}<x>>4>_<y>>4>/{f|c}_<x>_<y>.cft'
        (dest / 'manifest.json').write_text(json.dumps(m, indent=2))
    left = sum(1 for e in os.scandir(dest) if e.is_file() and e.name.endswith('.cft'))
    print(f'{n} tiles in {len(made)} folders; {left} flat .cft files left in {dest}')

if __name__ == '__main__':
    main()
