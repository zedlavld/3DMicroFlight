"""Build the offline two-tier Arizona corridor tile set. Public USGS/USDA NAIP.
Use --plan to write the manifest only; reruns reuse verified cached source PNGs.
"""
from pathlib import Path
import argparse,math,json,struct,hashlib,os,time,urllib.request,urllib.parse,concurrent.futures,datetime
import numpy as np
from PIL import Image
os.environ.pop('SSLKEYLOGFILE',None)
parser=argparse.ArgumentParser();parser.add_argument('--plan',action='store_true');parser.add_argument('--workers',type=int,default=4);args=parser.parse_args()
root=Path(__file__).resolve().parent.parent;cache=Path.cwd()/'work/corridor';cache.mkdir(parents=True,exist_ok=True)
out=root/'sdmc/3ds/CanyonFlight/scenery/stream';out.mkdir(parents=True,exist_ok=True)
base='https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPImagery/ImageServer'
route=[(-45.25601,.21236),(10631.13802,32229.87559),(-32759.85966,122731.63782),(-32768,135168)]
def distance(x,y):
 result=1e20
 for a,b in zip(route,route[1:]):
  dx=b[0]-a[0];dy=b[1]-a[1];t=max(0,min(1,((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy)))
  result=min(result,math.hypot(x-a[0]-t*dx,y-a[1]-t*dy))
 return result
keys=set()
for tier,span,half in [(0,2048,4096),(1,8192,16384)]:
 margin=half+span/math.sqrt(2)
 for x in range(math.floor((min(p[0] for p in route)-margin)/span),math.ceil((max(p[0] for p in route)+margin)/span)):
  for y in range(math.floor((min(p[1] for p in route)-margin)/span),math.ceil((max(p[1] for p in route)+margin)/span)):
   if distance((x+.5)*span,(y+.5)*span)<=margin:keys.add((tier,x,y))
# v31: statewide coarse tiles over Arizona plus a buffer, and detailed tiles
# around every v31 airport's runways (data/airports_v31.json).
_lat0=34.848598480225;_lon0=-111.78800201416;_m=6371000*math.pi/180;_mx=_m*math.cos(math.radians(_lat0))
AZ=[(37.0,-114.05),(37.0,-109.045),(31.332,-109.045),(31.332,-111.075),(32.495,-114.813),(32.718,-114.719),
    (33.03,-114.52),(33.41,-114.72),(33.70,-114.52),(34.10,-114.43),(34.30,-114.14),(34.72,-114.47),(35.10,-114.63),
    (35.60,-114.66),(36.10,-114.74),(36.12,-114.05),(37.0,-114.05)]
AZ=[((lon-_lon0)*_mx,(lat-_lat0)*_m) for lat,lon in AZ]
def inside(x,y):
 c=False
 for (x1,y1),(x2,y2) in zip(AZ,AZ[1:]):
  if (y1>y)!=(y2>y) and x<(x2-x1)*(y-y1)/(y2-y1)+x1:c=not c
 return c
def edge(x,y):
 r=1e20
 for (x1,y1),(x2,y2) in zip(AZ,AZ[1:]):
  dx=x2-x1;dy=y2-y1;t=max(0,min(1,((x-x1)*dx+(y-y1)*dy)/(dx*dx+dy*dy)));r=min(r,math.hypot(x-x1-t*dx,y-y1-t*dy))
 return r
STATE_BUFFER=12000;AIRPORT_FINE=3500
xs=[p[0] for p in AZ];ys=[p[1] for p in AZ];span=8192;margin=STATE_BUFFER+span/math.sqrt(2)
for x in range(math.floor((min(xs)-margin)/span),math.ceil((max(xs)+margin)/span)):
 for y in range(math.floor((min(ys)-margin)/span),math.ceil((max(ys)+margin)/span)):
  cx=(x+.5)*span;cy=(y+.5)*span
  if inside(cx,cy) or edge(cx,cy)<=margin:keys.add((1,x,y))
runways=json.loads((root/'data/airports_v31.json').read_text()) if (root/'data/airports_v31.json').exists() else []
for r in runways:
 if r['id'] in ('KSEZ','KFLG','KGCN'):continue  # already in the corridor
 h=math.radians(r['heading']);half=r['length']/2;span=2048
 ends=[(r['east']-math.sin(h)*half,r['north']-math.cos(h)*half),(r['east']+math.sin(h)*half,r['north']+math.cos(h)*half)]
 margin=AIRPORT_FINE+span/math.sqrt(2)
 for x in range(math.floor((min(e[0] for e in ends)-margin)/span),math.ceil((max(e[0] for e in ends)+margin)/span)):
  for y in range(math.floor((min(e[1] for e in ends)-margin)/span),math.ceil((max(e[1] for e in ends)+margin)/span)):
   cx=(x+.5)*span;cy=(y+.5)*span;a,b=ends;dx=b[0]-a[0];dy=b[1]-a[1]
   t=max(0,min(1,((cx-a[0])*dx+(cy-a[1])*dy)/(dx*dx+dy*dy)))
   if math.hypot(cx-a[0]-t*dx,cy-a[1]-t*dy)<=margin:keys.add((0,x,y))
keys=sorted(keys);index=b'CFINDEX5'+struct.pack('<2I',1,len(keys))+b''.join(struct.pack('<3i',*k) for k in keys)
(root/'data/corridor_index.bin').write_bytes(index)
metadata={'version':15,'v31_statewide':{'state_buffer_m':12000,'airport_fine_radius_m':3500,'airports':'data/airports_v31.json'},'source':base,'attribution':'USGS, USDA, The National Map: Orthoimagery','download_date':str(datetime.date.today()),'route_m':route,'detail_corridor_half_width_m':4096,'coarse_corridor_half_width_m':16384,'fine_count':sum(k[0]==0 for k in keys),'coarse_count':sum(k[0]==1 for k in keys),'tile_format':'CFSTRM15 + x,y,dimension,maxLevel,payloadBytes,reserved (32-byte header), RGB565 Morton mip payload','tiles':[]}
print('PLAN',metadata['fine_count'],'detail tiles,',metadata['coarse_count'],'coarse tiles; estimated bytes',metadata['fine_count']*2796192+metadata['coarse_count']*699040,flush=True)
(root/'data/corridor_metadata.json').write_text(json.dumps(metadata,indent=2))
if args.plan:raise SystemExit
lat0=34.848598480225;lon0=-111.78800201416;m=6371000*math.pi/180;mx=m*math.cos(math.radians(lat0))
def pack(image):
 a=np.asarray(image,dtype=np.uint16)[::-1];rgb=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)
 h,w=rgb.shape;y,x=np.indices((h,w));z=np.zeros((h,w),dtype=np.int64)
 for bit in range(3):z|=((x>>bit)&1)<<(2*bit);z|=((y>>bit)&1)<<(2*bit+1)
 idx=((y//8)*(w//8)+x//8)*64+z;output=np.empty(h*w,dtype='<u2');output[idx.ravel()]=rgb.ravel();return output.tobytes()
def build(key):
 tier,x,y=key;span=2048 if tier==0 else 8192;size=1024 if tier==0 else 512;level=7 if tier==0 else 6;name=f'{"f" if tier==0 else "c"}_{x}_{y}';png=cache/(name+'.png');dest=out/(name+'.cft');step=span/(size-2)
 bbox=[lon0+(x*span-step)/mx,lat0+(y*span-step)/m,lon0+((x+1)*span+step)/mx,lat0+((y+1)*span+step)/m]
 params={'f':'json','bbox':','.join(map(str,bbox)),'bboxSR':4326,'imageSR':4326,'size':f'{size},{size}','format':'png','bandIds':'0,1,2','renderingRule':json.dumps({'rasterFunction':'NaturalColor'}),'interpolation':'RSP_BilinearInterpolation','adjustAspectRatio':'false'}
 url=base+'/exportImage?'+urllib.parse.urlencode(params)
 for attempt in range(4):
  try:
   if not png.exists():
    response=json.load(urllib.request.urlopen(url,timeout=120))
    if 'href' not in response:raise RuntimeError(str(response)[:300])
    data=urllib.request.urlopen(response['href'],timeout=120).read();png.write_bytes(data)
    (cache/(name+'.json')).write_text(json.dumps(response))
   image=Image.open(png).convert('RGB');assert image.size==(size,size);a=np.asarray(image)
   # v31: statewide tiles at the border (Mexico, parts of CA/NV) have no
   # NAIP coverage. They are excluded from the catalog, not retried.
   if not (a.std()>10 and (a.max(2)>5).mean()>.99):return {'tier':tier,'x':x,'y':y,'excluded':'no imagery coverage','covered_fraction':round(float((a.max(2)>5).mean()),4)}
   sourceHash=hashlib.sha256(png.read_bytes()).hexdigest();stamp=cache/(name+'.packed.json')
   prior=json.loads(stamp.read_text()) if stamp.exists() else {}
   raw=dest.read_bytes() if dest.exists() else b''
   if prior.get('source_sha256')!=sourceHash or prior.get('sha256')!=hashlib.sha256(raw).hexdigest():
    payload=b''.join(pack(image.resize((size>>i,size>>i),Image.Resampling.LANCZOS)) for i in range(level+1))
    raw=b'CFSTRM15'+struct.pack('<2i4I',x,y,size,level,len(payload),0)+payload;temporary=dest.with_suffix('.tmp');temporary.write_bytes(raw);temporary.replace(dest)
    stamp.write_text(json.dumps({'source_sha256':sourceHash,'sha256':hashlib.sha256(raw).hexdigest()}))
   raw=dest.read_bytes();assert raw[:8]==b'CFSTRM15';assert len(raw)==(2796192 if tier==0 else 699040)
   batchMeta=cache/(name+'.batch.json')
   if batchMeta.exists():url=json.loads(batchMeta.read_text())['batch_request']
   return {'tier':tier,'x':x,'y':y,'size':size,'max_level':level,'span_m':span,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'source_sha256':hashlib.sha256(png.read_bytes()).hexdigest(),'request':url}
  except Exception:
   if attempt==3:raise
   time.sleep(2*(attempt+1))
# Request up to 2x2 tiles on one exact shared pixel lattice, below the
# service's 4000-pixel export limit. This reduces server requests substantially.
groups={}
for key in keys:groups.setdefault((key[0],key[1]//2,key[2]//2),[]).append(key)
def build_group(group):
 groupKey,items=group;tier,gx,gy=groupKey;size=1024 if tier==0 else 512;span=2048 if tier==0 else 8192
 missing=[k for k in items if not (cache/(f'{"f" if tier==0 else "c"}_{k[1]}_{k[2]}.png')).exists()]
 if missing:
  step=span/(size-2);dimension=2*(size-2)+2;e=gx*2*span;n=gy*2*span
  bbox=[lon0+(e-step)/mx,lat0+(n-step)/m,lon0+(e+2*span+step)/mx,lat0+(n+2*span+step)/m]
  params={'f':'json','bbox':','.join(map(str,bbox)),'bboxSR':4326,'imageSR':4326,'size':f'{dimension},{dimension}','format':'png','bandIds':'0,1,2','renderingRule':json.dumps({'rasterFunction':'NaturalColor'}),'interpolation':'RSP_BilinearInterpolation','adjustAspectRatio':'false'}
  request=base+'/exportImage?'+urllib.parse.urlencode(params);batch=cache/f'batch2_{tier}_{gx}_{gy}.png'
  for attempt in range(4):
   try:
    if not batch.exists():
     response=json.load(urllib.request.urlopen(request,timeout=180))
     if 'href' not in response:raise RuntimeError(str(response)[:300])
     batch.write_bytes(urllib.request.urlopen(response['href'],timeout=180).read())
    image=Image.open(batch).convert('RGB');assert image.size==(dimension,dimension)
    for _,x,y in missing:
     col=x-gx*2;row=y-gy*2;px=col*(size-2);py=(1-row)*(size-2);name=f'{"f" if tier==0 else "c"}_{x}_{y}'
     image.crop((px,py,px+size,py+size)).save(cache/(name+'.png'))
     (cache/(name+'.batch.json')).write_text(json.dumps({'batch_request':request,'batch_key':groupKey}))
    break
   except Exception as error:
    # v31: the service rejects requests wholly outside its extent (far
    # south of the border) with HTTP 400; such coarse tiles have no coverage.
    if tier==1 and '400' in str(error) and not batch.exists():
     return [{'tier':tier,'x':k[1],'y':k[2],'excluded':'outside imagery service extent'} for k in items]
    try:
     if batch.exists():batch.unlink()
    except OSError:time.sleep(1)  # Windows: file may still be open; retry overwrites
    # v31: a batch the service repeatedly truncates (seen at exactly 1 MiB)
    # falls back to single-tile requests in build().
    if attempt==3:break
    time.sleep(2*(attempt+1))
 return [build(key) for key in items]
results=[];failed=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,min(args.workers,4))) as pool:
 futures={pool.submit(build_group,group):group[0] for group in groups.items()}
 for future in concurrent.futures.as_completed(futures):
  try:results.extend(future.result())
  except Exception as e:failed.append({'batch':futures[future],'error':str(e)});print('FAILED',failed[-1],flush=True)
  print('PROGRESS',len(results),'/',len(keys),'batches failed',len(failed),flush=True)
excluded=[t for t in results if 'excluded' in t];results=[t for t in results if 'excluded' not in t]
# The game requires every catalog key to exist on disk: rewrite the index
# without excluded tiles (and without any that failed transiently).
built={(t['tier'],t['x'],t['y']) for t in results}
keys=[k for k in keys if k in built]
(root/'data/corridor_index.bin').write_bytes(b'CFINDEX5'+struct.pack('<2I',1,len(keys))+b''.join(struct.pack('<3i',*k) for k in keys))
for k in list(out.glob('*.cft')):
 tier=0 if k.name.startswith('f_') else 1;x,y=map(int,k.stem.split('_')[1:])
 if (tier,x,y) not in built:k.unlink()
metadata['fine_count']=sum(k[0]==0 for k in keys);metadata['coarse_count']=sum(k[0]==1 for k in keys)
metadata['excluded']=sorted(excluded,key=lambda t:(t['tier'],t['x'],t['y']))
metadata['tiles']=sorted(results,key=lambda t:(t['tier'],t['x'],t['y']));metadata['failures']=failed;metadata['total_bytes']=sum(t['bytes'] for t in results)
(root/'data/corridor_metadata.json').write_text(json.dumps(metadata,indent=2))
(out/'manifest.json').write_text(json.dumps({'version':15,'fine':metadata['fine_count'],'coarse':metadata['coarse_count'],'files':len(results),'complete':not failed,'source':base,'attribution':metadata['attribution']},indent=2))
print('DONE',len(results),'tiles',metadata['total_bytes'],'bytes; failed',len(failed),flush=True)
if failed:raise SystemExit(1)
