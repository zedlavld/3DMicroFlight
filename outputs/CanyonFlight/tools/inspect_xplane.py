"""Inspect user-supplied X-Plane ACFs for comparison, without copying assets.
Example: python tools/inspect_xplane.py "D:/X-Plane 12/Aircraft/Laminar Research/Cirrus SR22" --output sr22-reference.json
Raw property names/values are not interpreted as universal physical units.
"""
from pathlib import Path
import argparse,json,re,hashlib
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('folder',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
if not args.folder.is_dir():p.error('Aircraft folder does not exist')
report={'source':str(args.folder.resolve()),'purpose':'Reference inspection only; no model or texture assets exported','aircraft':[]}
for acf in sorted(args.folder.rglob('*.acf')):
 raw=acf.read_bytes();text=raw.decode('utf-8',errors='replace');records=[]
 for line in text.splitlines():
  parts=line.strip().split(maxsplit=2)
  if len(parts)==3 and parts[0]=='P' and re.search(r'wing|stab|fuse|mass|weight|cg|inertia|engn|prop|control|elev|rudder|aileron|flap|gear|acf_descrip|acf/_(?:m_|Jxx|Jyy|Jzz|name|descrip|throt|idle|thrust)',parts[1],re.I):
   records.append({'key':parts[1],'raw_value':parts[2]})
 report['aircraft'].append({'file':str(acf.relative_to(args.folder)),'sha256':hashlib.sha256(raw).hexdigest(),'header':text.splitlines()[:4],'selected_raw_properties':records})
report['airfoil_files']=[str(f.relative_to(args.folder)) for f in sorted(args.folder.rglob('*.afl'))]
report['object_files']=[str(f.relative_to(args.folder)) for f in sorted(args.folder.rglob('*.obj'))]
report['texture_files']=[str(f.relative_to(args.folder)) for f in sorted(args.folder.rglob('*')) if f.suffix.lower() in {'.png','.dds'}]
report['limitations']='ACF versions, units, airfoil polars, plugins and runtime corrections require manual interpretation. This does not reproduce the X-Plane solver or grant asset redistribution rights.'
if not report['aircraft']:p.error('No .acf files found; supply an individual aircraft folder')
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(f"Inspected {len(report['aircraft'])} ACF file(s); wrote {args.output}")
