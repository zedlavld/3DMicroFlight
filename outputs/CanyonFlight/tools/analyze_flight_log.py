"""Summarize a v16-v24 hardware/emulator CSV. No SDK or third-party packages needed."""
from pathlib import Path
import argparse,csv,json,math
p=argparse.ArgumentParser(description=__doc__);p.add_argument('csv',type=Path);args=p.parse_args()
with args.csv.open() as f:rows=list(csv.DictReader(f))
assert rows,'Empty flight log'
for row in rows:
 for k,v in row.items():assert k is not None and math.isfinite(float(v)),(k,v)
assert all(float(b['time_s'])>=float(a['time_s']) for a,b in zip(rows,rows[1:])), 'Time moved backward'
profiles=['takeoff','climb','cruise','descent','approach','short_final','slow_flight','engine_out_glide'];segments=[]
for row in rows:
 key=(row['aircraft'],row['start_profile'])
 if not segments or segments[-1][0]!=key:segments.append((key,[]))
 segments[-1][1].append(row)
report={'file':str(args.csv),'samples':len(rows),'segments':[]}
for (aircraft,phase),part in segments:
 start,end=part[0],part[-1];duration=float(end['time_s'])-float(start['time_s']);height=float(end['altitude_m'])-float(start['altitude_m'])
 report['segments'].append({'aircraft':'SF50' if int(aircraft) else 'SR22','start_profile':profiles[int(phase)],'duration_s':duration,'height_change_m':height,'average_climb_fpm':height/duration*196.8504 if duration else None,'min_ias_approx_kcas':min(float(x['ias_approx_kcas']) for x in part),'max_ias_approx_kcas':max(float(x['ias_approx_kcas']) for x in part),'max_stall_warning':max(float(x['stall_warning']) for x in part),'ground_samples':sum(int(x['on_ground']) for x in part),'crashed':any(int(x['crashed']) for x in part)})
dest=args.csv.with_suffix('.summary.json');dest.write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
