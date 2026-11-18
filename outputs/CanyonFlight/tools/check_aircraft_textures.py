"""Validate the embedded aircraft texture mip chains against their PNG atlases."""
from pathlib import Path
import numpy as np,json,hashlib
from PIL import Image
root=Path(__file__).resolve().parent.parent;report={}
for name in ['sr22','sf50']:
 image=Image.open(root/'art'/f'{name}_paint.png').convert('RGB');assert image.size==(512,512)
 raw=(root/'data'/f'{name}_texture.bin').read_bytes();assert len(raw)==699008;offset=0
 for size in [512,256,128,64,32,16,8]:
  y,x=np.indices((size,size));m=np.zeros((size,size),dtype=np.int64)
  for bit in range(3):m|=((x>>bit)&1)<<(2*bit);m|=((y>>bit)&1)<<(2*bit+1)
  idx=((y//8)*(size//8)+x//8)*64+m
  decoded=np.frombuffer(raw[offset:offset+size*size*2],dtype='<u2')[idx]
  a=np.asarray(image.resize((size,size),Image.Resampling.LANCZOS),dtype=np.uint16)
  expected=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)
  assert np.array_equal(decoded,expected),(name,size);offset+=size*size*2
 assert offset==len(raw)
 report[name]={'size':512,'mip_levels':7,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'all_pixels_verified':True}
(root/'validation/aircraft_texture_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
