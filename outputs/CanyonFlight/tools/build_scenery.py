"""Build offline Arizona elevation grids from public Mapzen Terrain Tiles.
Requires Pillow/numpy. Network is used only when a source tile is not cached.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import urllib.request,math,csv,json,hashlib
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parent.parent
cache=Path.cwd()/'work'/'terrain-tiles';cache.mkdir(parents=True,exist_ok=True)
LAT=34.848598480225;LON=-111.78800201416
METERS=6371000*math.pi/180;LONMETERS=METERS*math.cos(math.radians(LAT))
regions=[('region',-100000,-40000,500,361,461,10),('sedona',-12000,-12000,60,401,401,12),('flagstaff',-12000,18000,120,401,401,12),('canyon',-90000,95000,200,451,326,11)]
def fetch(key):
 z,x,y=key;p=cache/f'{z}_{x}_{y}.png'
 if not p.exists():
  req=urllib.request.Request(f'https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png',headers={'User-Agent':'CanyonFlight-scenery-build/1'})
  with urllib.request.urlopen(req,timeout=40) as r:p.write_bytes(r.read())
 return key,p
metadata={'origin':{'latitude':LAT,'longitude':LON},'projection':'local equirectangular, spherical R=6371000m','source':'Mapzen Terrain Tiles, Terrarium on AWS','download_date':'2026-10-24','grids':[]}
headers=['#ifndef TERRAIN_DATA_H','#define TERRAIN_DATA_H','#include <stdint.h>','typedef struct {float east,north,spacing;int width,height;const int16_t *data;} ElevationGrid;','extern const ElevationGrid elevationGrids[4];','#endif']
source=['#include "terrain_data.h"'];declarations=[]
for name,e,n,spacing,w,h,z in regions:
 east=e+np.arange(w)*spacing;north=n+np.arange(h)*spacing
 lon=LON+east/LONMETERS;lat=LAT+north/METERS
 px=(lon+180)/360*(2**z)*256-.5
 py=(1-np.arcsinh(np.tan(np.radians(lat)))/math.pi)/2*(2**z)*256-.5
 x0=int(np.floor(px.min()/256));x1=int(np.floor(px.max()/256));y0=int(np.floor(py.min()/256));y1=int(np.floor(py.max()/256))
 keys=[(z,x,y) for y in range(y0,y1+1) for x in range(x0,x1+1)]
 mosaic=np.zeros(((y1-y0+1)*256,(x1-x0+1)*256),np.float32)
 with ThreadPoolExecutor(max_workers=4) as pool:
  for (_,x,y),path in pool.map(fetch,keys):
   rgb=np.array(Image.open(path).convert('RGB'),dtype=np.float32)
   heights=rgb[:,:,0]*256+rgb[:,:,1]+rgb[:,:,2]/256-32768
   mosaic[(y-y0)*256:(y-y0+1)*256,(x-x0)*256:(x-x0+1)*256]=heights
 xx,yy=np.meshgrid(px-x0*256,py-y0*256);ix=xx.astype(int);iy=yy.astype(int);fx=xx-ix;fy=yy-iy
 ix=np.clip(ix,0,mosaic.shape[1]-2);iy=np.clip(iy,0,mosaic.shape[0]-2)
 height=(mosaic[iy,ix]*(1-fx)+mosaic[iy,ix+1]*fx)*(1-fy)+(mosaic[iy+1,ix]*(1-fx)+mosaic[iy+1,ix+1]*fx)*fy
 height=np.rint(height).astype('<i2');assert height.min()>200 and height.max()<5000
 (root/'data'/f'elevation_{name}.i16').write_bytes(height.tobytes())
 source.append(f'static const int16_t heights_{name}[{w*h}]={{')
 for row in height:source.append(','.join(map(str,row))+',')
 source.append('};');declarations.append(f'{{{e},{n},{spacing},{w},{h},heights_{name}}}')
 info={'name':name,'east':e,'north':n,'spacing':spacing,'width':w,'height':h,'tile_zoom':z,'source_tiles':len(keys),'min_m':int(height.min()),'max_m':int(height.max()),'sha256':hashlib.sha256(height.tobytes()).hexdigest()}
 metadata['grids'].append(info);print(info,flush=True)
source.append('const ElevationGrid elevationGrids[4]={'+','.join(declarations)+'};')
(root/'include'/'terrain_data.h').write_text('\n'.join(headers));(root/'source'/'terrain_data.c').write_text('\n'.join(source))
(root/'data'/'scenery_metadata.json').write_text(json.dumps(metadata,indent=2))
# Generate sourced airport definitions and retain only the three selected rows.
rows={r['airport_ident']:r for r in csv.DictReader((Path.cwd()/'work'/'runways.csv').open()) if r['airport_ident'] in {'KSEZ','KFLG','KGCN'}}
airports=[]
for key in ['KSEZ','KFLG','KGCN']:
 r=rows[key];lat=(float(r['le_latitude_deg'])+float(r['he_latitude_deg']))/2;lon=(float(r['le_longitude_deg'])+float(r['he_longitude_deg']))/2
 e=(lon-LON)*LONMETERS;n=(lat-LAT)*METERS;lo=float(r['le_elevation_ft'])*.3048;hi=float(r['he_elevation_ft'])*.3048
 airports.append({'id':key,'east':e,'north':n,'elevation':max(lo,hi),'heading':float(r['le_heading_degT']),'length':float(r['length_ft'])*.3048,'width':float(r['width_ft'])*.3048,'lowElevation':lo,'highElevation':hi,'source':r})
(root/'data'/'airports.json').write_text(json.dumps(airports,indent=2))
lines=['#include "flight.h"','const Airport airports[3]={']
for a in airports:lines.append('{"'+a['id']+'",'+','.join(str(round(a[k],5))+'f' for k in ['east','north','elevation','heading','length','width','lowElevation','highElevation'])+'},')
lines.append('};');(root/'source'/'airports.c').write_text('\n'.join(lines))
