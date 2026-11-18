"""Summarize a v24 runtime frame snapshot. CPU/GPU columns are the most recent
completed Citro3D counters, not necessarily synchronized with each frame row.
"""
from pathlib import Path
import argparse,csv,json,math
p=argparse.ArgumentParser(description=__doc__);p.add_argument('csv',type=Path);a=p.parse_args()
with a.csv.open() as f:rows=list(csv.DictReader(f))
assert rows,'No recorded frames'
times=[float(row['frame_ms']) for row in rows];assert all(math.isfinite(t) and t>0 for t in times)
ordered=sorted(times);n=len(times)
report={'file':str(a.csv),'frames':n,'duration_s':sum(times)/1000,'average_fps':n*1000/sum(times),'p95_frame_ms':ordered[math.ceil(n*.95)-1],'worst_frame_ms':max(times),'aircraft':rows[0]['aircraft'],'exterior':rows[0]['exterior'],'paused':rows[0]['paused'],'stereo':rows[0]['stereo']}
a.csv.with_suffix('.summary.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
