"""Compare rendered terrain interpolation with the packaged USGS samples.
V07 baseline ZIP is optional; no network required. This measures mesh
approximation error, not the accuracy of the USGS measurements themselves.
"""
from pathlib import Path
import json,zipfile,sys
import numpy as np
root=Path(__file__).resolve().parent.parent
meta=json.loads((root/'data/scenery_metadata.json').read_text())
grids=[(g,np.fromfile(root/'data'/('elevation_'+g['name']+'.i16'),dtype='<i2').reshape(g['height'],g['width'])) for g in meta['grids']]
def elevation(e,n):
 result=None
 for i,(g,a) in enumerate(grids):
  x=np.clip((e-g['east'])/g['spacing'],0,g['width']-1.001);y=np.clip((n-g['north'])/g['spacing'],0,g['height']-1.001)
  ix=x.astype(int);iy=y.astype(int);fx=x-ix;fy=y-iy
  v=((1-fx)*a[iy,ix]+fx*a[iy,ix+1])*(1-fy)+((1-fx)*a[iy+1,ix]+fx*a[iy+1,ix+1])*fy
  if not i:result=v
  else:
   dx=np.minimum(e-g['east'],g['east']+(g['width']-1)*g['spacing']-e);dy=np.minimum(n-g['north'],g['north']+(g['height']-1)*g['spacing']-n)
   t=np.clip(np.minimum(dx,dy)/1200,0,1);t=t*t*(3-2*t);result=result*(1-t)+v*t
 return result
aps=json.loads((root/'data/airports.json').read_text())
baseline=zipfile.ZipFile(sys.argv[1]) if len(sys.argv)>1 else None
report={'description':'Fixed random sample locations within 16km of each validation scene, excluding 2km around airports. Absolute mesh interpolation error against packaged elevation grids; not survey accuracy.','scenes':[]}
for i,(cx,cy) in enumerate([(-1600,-2300),(10500,44500),(-41000,133000)]):
 rng=np.random.default_rng(800+i);points=rng.uniform(-16000,16000,(1200,2))+[cx,cy]
 for ap in aps:points=points[np.hypot(points[:,0]-ap['east'],points[:,1]-ap['north'])>2000]
 actual=elevation(points[:,0],points[:,1]);entry={'scene':i,'samples':len(points)}
 for version in (['v07','v08'] if baseline else ['v08']):
  raw=baseline.read(f'validation/world_{i}.bin') if version=='v07' else (root/'validation'/f'world_{i}.bin').read_bytes()
  triangles=np.frombuffer(raw,dtype='<f4').reshape(-1,3,6)[:,:,:3].astype(float)
  lo=triangles[:,:,:2].min(axis=1);hi=triangles[:,:,:2].max(axis=1);errors=[]
  for (x,y),h in zip(points,actual):
   t=triangles[(lo[:,0]<=x)&(hi[:,0]>=x)&(lo[:,1]<=y)&(hi[:,1]>=y)]
   a,b,c=t[:,0],t[:,1],t[:,2];den=(b[:,1]-c[:,1])*(a[:,0]-c[:,0])+(c[:,0]-b[:,0])*(a[:,1]-c[:,1]);valid=abs(den)>1e-6
   a,b,c,den=a[valid],b[valid],c[valid],den[valid]
   u=((b[:,1]-c[:,1])*(x-c[:,0])+(c[:,0]-b[:,0])*(y-c[:,1]))/den
   v=((c[:,1]-a[:,1])*(x-c[:,0])+(a[:,0]-c[:,0])*(y-c[:,1]))/den
   w=1-u-v;inside=(u>=-1e-6)&(v>=-1e-6)&(w>=-1e-6)
   assert inside.any(),(i,x,y)
   height=(u*a[:,2]+v*b[:,2]+w*c[:,2])[inside].max();errors.append(abs(height-h))
  entry[version]={'mean_abs_m':float(np.mean(errors)),'p95_abs_m':float(np.percentile(errors,95)),'max_abs_m':float(np.max(errors))}
 report['scenes'].append(entry)
(root/'validation/terrain_detail_comparison.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
