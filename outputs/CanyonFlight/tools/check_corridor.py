from pathlib import Path
import json,struct,hashlib,math
import numpy as np
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parent.parent
meta=json.loads((root/'data/corridor_metadata.json').read_text());cache=Path.cwd()/'work/corridor';out=root/'sdmc/3ds/CanyonFlight/scenery/stream'
index=(root/'data/corridor_index.bin').read_bytes();assert index[:8]==b'CFINDEX5';version,count=struct.unpack_from('<2I',index,8);assert version==1 and len(index)==16+count*12
keys=[struct.unpack_from('<3i',index,16+i*12) for i in range(count)];assert keys==sorted(set(keys));assert len(meta['tiles'])==count and not meta.get('failures')
checks=0;total=0;maxseam=0;seams=0;meanseams=[]
for t in meta['tiles']:
 tier,x,y=t['tier'],t['x'],t['y'];name=f'{"f" if tier==0 else "c"}_{x}_{y}';data=(out/(name+'.cft')).read_bytes();size=1024 if tier==0 else 512;level=7 if tier==0 else 6
 assert data[:8]==b'CFSTRM15';assert struct.unpack_from('<2i4I',data,8)==(x,y,size,level,len(data)-32,0)
 assert len(data)-32==sum((size>>i)**2*2 for i in range(level+1));assert hashlib.sha256(data).hexdigest()==t['sha256'];total+=len(data);checks+=4
 # Decode every Morton mip and compare it against the retained source.
 with Image.open(cache/(name+'.png')) as im:image=im.convert('RGB')
 a=np.asarray(image,dtype=np.uint16);offset=32
 for mip in range(level+1):
  dim=size>>mip;raw=np.frombuffer(data,offset=offset,count=dim*dim,dtype='<u2');yy,xx=np.indices((dim,dim));z=np.zeros_like(xx)
  for bit in range(3):z|=((xx>>bit)&1)<<(2*bit);z|=((yy>>bit)&1)<<(2*bit+1)
  ix=((yy//8)*(dim//8)+xx//8)*64+z;decoded=raw[ix]
  pixels=np.asarray(image.resize((dim,dim),Image.Resampling.LANCZOS),dtype=np.uint16)[::-1]
  expected=((pixels[:,:,0]>>3)<<11)|((pixels[:,:,1]>>2)<<5)|(pixels[:,:,2]>>3)
  assert np.array_equal(decoded,expected),(name,mip);offset+=dim*dim*2;checks+=1
 assert offset==len(data)
 for dx,dy in [(1,0),(0,1)]:
  neighbor=cache/(f'{"f" if tier==0 else "c"}_{x+dx}_{y+dy}.png')
  if (tier,x+dx,y+dy) not in keys:continue
  with Image.open(neighbor) as im:b=np.asarray(im.convert('RGB'),dtype=np.int16)
  first=a[:,-2:].astype(np.int16) if dx else a[:2].astype(np.int16);second=b[:,:2] if dx else b[-2:]
  diff=np.abs(first-second);maxseam=max(maxseam,int(diff.max()));meanseams.append(float(diff.mean()));seams+=1
assert total==meta['total_bytes']
assert float(np.quantile(meanseams,.95))<2,'Unexpected widespread source-image seam mismatch'
# Bounds and route coverage: every 100 m along the route, both 2x2 windows exist.
route=meta['route_m'];samples=0
for a,b in zip(route,route[1:]):
 length=math.dist(a,b)
 for distance in np.arange(0,length+100,100):
  t=min(1,distance/length);p=[a[i]+t*(b[i]-a[i]) for i in range(2)]
  for tier,span in [(0,2048),(1,8192)]:
   center=[math.floor(v/span+.5) for v in p]
   for dx in [-1,0]:
    for dy in [-1,0]:assert (tier,center[0]+dx,center[1]+dy) in keys;checks+=1
  samples+=1
report={'tiles':count,'fine':sum(k[0]==0 for k in keys),'coarse':sum(k[0]==1 for k in keys),'bytes':total,'checks':checks,'route_samples':samples,'all_morton_mips_verified':True,'neighbor_seams':seams,'maximum_source_seam_channel_difference':maxseam,'p95_mean_source_seam_channel_difference':float(np.quantile(meanseams,.95)),'mean_source_seam_channel_difference':sum(meanseams)/len(meanseams),'failures':0}
(root/'validation/corridor_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
# Survey-style overview of actual downloaded imagery. This is not a GPU capture.
scale=.018;minx=-65536;maxx=40960;miny=-24576;maxy=163840;w=int((maxx-minx)*scale);h=int((maxy-miny)*scale)
im=Image.new('RGB',(w,h),(32,38,42))
for tier in [1,0]:
 for t in meta['tiles']:
  if t['tier']!=tier:continue
  span=t['span_m'];x=t['x']*span;y=t['y']*span;size=t['size'];name=f'{"f" if tier==0 else "c"}_{t["x"]}_{t["y"]}'
  box=(round((x-minx)*scale),round((maxy-y-span)*scale),round((x+span-minx)*scale),round((maxy-y)*scale))
  with Image.open(cache/(name+'.png')) as tile:piece=tile.crop((1,1,size-1,size-1)).resize((box[2]-box[0],box[3]-box[1]),Image.Resampling.LANCZOS)
  im.paste(piece,box[:2])
d=ImageDraw.Draw(im);points=[((x-minx)*scale,(maxy-y)*scale) for x,y in route];d.line(points,fill=(255,220,70),width=5)
for p,label in zip(points,['SEDONA / KSEZ','FLAGSTAFF / KFLG','GRAND CANYON / KGCN','SOUTH RIM']):
 d.ellipse((p[0]-6,p[1]-6,p[0]+6,p[1]+6),fill='yellow');d.text((p[0]+10,p[1]-10),label,fill='white',stroke_width=2,stroke_fill='black',font_size=22)
d.rectangle((0,0,w,100),fill=(10,20,30));d.text((20,16),'CANYON FLIGHT v15 - downloaded scenery coverage',fill='white',font_size=28);d.text((20,57),'USGS / USDA NAIP | ~2 m detail corridor, ~16 m outer tiles | North up',fill='white',font_size=21)
d.text((20,h-45),'Coverage overview from source imagery; not an emulator screenshot.',fill='white',stroke_width=1,stroke_fill='black',font_size=21)
im.save(root/'art/corridor_overview.png')
