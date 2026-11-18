"""Give scenery/stream32 a fresh, small FAT32 directory (v35.08).

  python -P tools/fresh_stream32.py <scenery folder>

A FAT32 directory never shrinks: after tools/relayout_stream32.py moved ~31,000 tiles out of
the flat stream32 folder, that folder still held ~63,700 (deleted) entries, and the 363 new
subfolder entries were appended after them, past the ~60,000 entries the 3DS can reach, so
v35.07 on hardware could open none of them (FS 0xC8A04573) while Windows and Azahar could.

This makes scenery/stream32.new, moves every subfolder and manifest.json into it (renames on
the same volume, no data copied), renames the emptied old folder to stream32.old-<time>,
removes it if it is empty, and renames stream32.new to stream32.
"""
import os, sys, time
from pathlib import Path

def main():
    if len(sys.argv) != 2: raise SystemExit(__doc__)
    sc = Path(sys.argv[1]); old = sc / 'stream32'; new = sc / 'stream32.new'
    if not old.is_dir(): raise SystemExit(f'{old} not found')
    new.mkdir(exist_ok=True); moved = 0
    for e in list(os.scandir(old)):
        os.replace(e.path, new / e.name); moved += 1
    left = list(os.scandir(old))
    if left: raise SystemExit(f'{len(left)} entries could not be moved; stream32.new holds {moved}')
    gone = sc / f'stream32.old-{time.strftime("%H%M%S")}'
    os.replace(old, gone); os.rmdir(gone); os.replace(new, old)
    dirs = sum(1 for e in os.scandir(old) if e.is_dir())
    print(f'moved {moved} entries ({dirs} folders) into a fresh {old}')

if __name__ == '__main__':
    main()
