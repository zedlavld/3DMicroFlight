"""Download raw USGS 3DEP bare-earth exports; no runtime network required.
Output spacing is resampling spacing, not a guarantee of source accuracy.
"""
from pathlib import Path
import urllib.request, urllib.parse, json, math, hashlib, os
import numpy as np
from PIL import Image
os.environ.pop('SSLKEYLOGFILE',None)
root=Path(__file__).resolve().parent.parent
cache=Path.cwd()/'work'/'usgs';cache.mkdir(parents=True,exist_ok=True)
base='https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer'
LAT=34.848598480225;LON=-111.78800201416
M=6371000*math.pi/180;MX=M*math.cos(math.radians(LAT))
regions=[('region',-100000,-40000,500,361,461),('sedona',-12000,-12000,30,801,801),('flagstaff',-12000,18000,60,801,801),('canyon',-90000,95000,100,901,651)]
meta={'source':base,'download_date':'2026-10-24','source_description':'USGS multi-resolution 3DEP bare-earth DEM, raw F32, bilinear resampling','vertical_units':'meters','origin':{'latitude':LAT,'longitude':LON},'projection':'local equirectangular, spherical R=6371000m','grids':[]}
source=['#include "terrain_data.h"'];decl=[]
for name,e,n,d,w,h in regions:
 # Half-pixel margin puts exported pixel centers exactly on the game grid.
 bbox=[LON+(e-d/2)/MX,LAT+(n-d/2)/M,LON+(e+(w-.5)*d)/MX,LAT+(n+(h-.5)*d)/M]
 params={'f':'json','bbox':','.join(map(str,bbox)),'bboxSR':4326,'imageSR':4326,'size':f'{w},{h}','format':'tiff','pixelType':'F32','renderingRule':json.dumps({'rasterFunction':'None'}),'interpolation':'RSP_BilinearInterpolation','adjustAspectRatio':'false'}
 url=base+'/exportImage?'+urllib.parse.urlencode(params)
 path=cache/(name+'.tif')
 if not path.exists():
  response=json.load(urllib.request.urlopen(url,timeout=180))
  if 'href' not in response:raise RuntimeError(response)
  (cache/(name+'.json')).write_text(json.dumps(response,indent=2))
  urllib.request.urlretrieve(response['href'],path)
 a=np.asarray(Image.open(path),dtype=np.float32)[::-1]
 assert a.shape==(h,w) and np.isfinite(a).all() and a.min()>200 and a.max()<5000,(name,a.shape,a.min(),a.max())
 a=np.rint(a).astype('<i2');raw=a.tobytes()
 (root/'data'/f'elevation_{name}.i16').write_bytes(raw)
 source.append(f'static const int16_t heights_{name}[{w*h}]={{')
 source.extend(','.join(map(str,row))+',' for row in a);source.append('};')
 decl.append(f'{{{e},{n},{d},{w},{h},heights_{name}}}')
 info={'name':name,'east':e,'north':n,'spacing':d,'width':w,'height':h,'min_m':int(a.min()),'max_m':int(a.max()),'sha256':hashlib.sha256(raw).hexdigest(),'export_request':url}
 meta['grids'].append(info);print(name,a.shape,a.min(),a.max(),flush=True)
source.append('const ElevationGrid elevationGrids[4]={'+','.join(decl)+'};')
(root/'source'/'terrain_data.c').write_text('\n'.join(source))
(root/'data'/'scenery_metadata.json').write_text(json.dumps(meta,indent=2))
