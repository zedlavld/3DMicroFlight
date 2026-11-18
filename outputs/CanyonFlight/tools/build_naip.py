"""Prepare the offline Sedona NAIP scenery pack (Pillow/numpy required).
Public USGS/USDA natural-color imagery; no network access in the simulator.
"""
from pathlib import Path
import urllib.request,urllib.parse,json,math,os,hashlib
import argparse
parser=argparse.ArgumentParser();parser.add_argument("--region",choices=["sedona","flagstaff","canyon"],default="sedona");region=parser.parse_args().region
centerE,centerN={"sedona":(0,0),"flagstaff":(10624,32256),"canyon":(-32768,128000)}[region]
import numpy as np
from PIL import Image
os.environ.pop('SSLKEYLOGFILE',None)
root=Path(__file__).resolve().parent.parent
cache=Path.cwd()/'work'/'naip';cache.mkdir(parents=True,exist_ok=True)
out=root/'sdmc'/'3ds'/'CanyonFlight'/'scenery';out.mkdir(parents=True,exist_ok=True)
base='https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPImagery/ImageServer'
lat0=34.848598480225;lon0=-111.78800201416
m=6371000*math.pi/180;mx=m*math.cos(math.radians(lat0))
metadata={'source':base,'attribution':'USGS, USDA, The National Map: Orthoimagery','download_date':'2026-10-24','license':'Public-domain NAIP aerial imagery, per USGS service description','origin':{'latitude':lat0,'longitude':lon0},'region':region,'world_center_m':[centerE,centerN],'bounds_m':[centerE-8192,centerN-8192,centerE+8192,centerN+8192],'tiles':[],'format':'CFNAIP09 header + RGB565 PICA200 Morton-swizzled mip levels: inner 1024 to 8, outer 512 to 8'}
# Shared pixel lattice and a one-pixel gutter keep interpolation continuous.
size=1024;step=2048/(size-2)
def pack(image):
 a=np.asarray(image,dtype=np.uint16)[::-1]
 rgb=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)
 h,w=rgb.shape;y,x=np.indices((h,w));morton=np.zeros((h,w),dtype=np.int64)
 for bit in range(3):morton|=((x>>bit)&1)<<(2*bit);morton|=((y>>bit)&1)<<(2*bit+1)
 index=((y//8)*(w//8)+x//8)*64+morton
 output=np.empty(h*w,dtype='<u2');output[index.ravel()]=rgb.ravel();return output.tobytes()
mosaic=Image.new('RGB',(2044,2044))
for tile in range(8):
 grid=tile%4;col=grid%2;row=grid//2
 size=1024 if tile<4 else 512;span=2048 if tile<4 else 8192;step=span/(size-2)
 e=-span+col*span;n=-span+row*span
 bbox=[lon0+(e+centerE-step)/mx,lat0+(n+centerN-step)/m,lon0+(e+centerE+span+step)/mx,lat0+(n+centerN+span+step)/m]
 params={'f':'json','bbox':','.join(map(str,bbox)),'bboxSR':4326,'imageSR':4326,'size':f'{size},{size}','format':'png','bandIds':'0,1,2','renderingRule':json.dumps({'rasterFunction':'NaturalColor'}),'interpolation':'RSP_BilinearInterpolation','adjustAspectRatio':'false'}
 url=base+'/exportImage?'+urllib.parse.urlencode(params);path=cache/f'{region}_{tile}.png'
 if not path.exists():
  response=json.load(urllib.request.urlopen(url,timeout=180))
  if 'href' not in response:raise RuntimeError(response)
  (cache/f'{region}_{tile}.json').write_text(json.dumps(response,indent=2),encoding='utf-8')
  urllib.request.urlretrieve(response['href'],path)
 image=Image.open(path).convert('RGB');assert image.size==(size,size)
 a=np.asarray(image);assert a.std()>10 and (a.max(axis=2)>5).mean()>.99,'Missing imagery'
 # Raw binary is portable and requires no image decoder on the handheld.
 import struct
 payload=b''.join(pack(image.resize((s,s),Image.Resampling.LANCZOS)) for s in [size>>level for level in range(int(math.log2(size))-2)])
 header=b'CFNAIP09'+struct.pack('<6I',tile,size,int(math.log2(size))-3,len(payload),0,0)
 raw=header+payload;(out/f'{region}_{tile}.cft').write_bytes(raw)
 (root/'art'/f'{region}_naip_{tile}.png').write_bytes(path.read_bytes())
 if tile<4:mosaic.paste(image.crop((1,1,size-1,size-1)),(col*1022,(1-row)*1022))
 metadata['tiles'].append({'tile':tile,'east':e,'north':n,'span_m':span,'size':size,'max_level':int(math.log2(size))-3,'pixel_m':step,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'request':url})
 print('tile',tile,'bytes',len(raw),flush=True)
mosaic.resize((1024,1024),Image.Resampling.LANCZOS).save(root/'art'/f'{region}_naip_overview.png')
# Record source dates/resolution where the service exposes them.
query={'f':'json','where':'Category=1','geometry':','.join(map(str,[lon0+(centerE-8192)/mx,lat0+(centerN-8192)/m,lon0+(centerE+8192)/mx,lat0+(centerN+8192)/m])),'geometryType':'esriGeometryEnvelope','inSR':4326,'spatialRel':'esriSpatialRelIntersects','outFields':'Name,Year,raster_name,acquisition_date,resolution_value,resolution_units,download_url','returnGeometry':'false'}
catalog=json.load(urllib.request.urlopen(base+'/query?'+urllib.parse.urlencode(query),timeout=90))
(root/'data'/('naip_catalog.json' if region=='sedona' else f'naip_{region}_catalog.json')).write_text(json.dumps(catalog,indent=2),encoding='utf-8')
(root/'data'/('naip_metadata.json' if region=='sedona' else f'naip_{region}_metadata.json')).write_text(json.dumps(metadata,indent=2),encoding='utf-8')
(out/'README.txt').write_text('Three-region NAIP scenery for Canyon Flight v14. Copy the sdmc/3ds folder to the SD-card root. Public-domain imagery: USGS, USDA, The National Map. Each region has eight .cft files; preserve all filenames.\n',encoding='utf-8')
