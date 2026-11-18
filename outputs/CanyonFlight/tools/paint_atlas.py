"""Pack edited current aircraft PNGs into 512-square RGB565 mip chains.
For source-model regeneration use import_aircraft.py. This tool preserves PNGs.
"""
from pathlib import Path
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parent.parent
for name in ['sr22','sf50']:
 image=Image.open(root/'art'/f'{name}_paint.png').convert('RGB');assert image.size==(512,512)
 # Keep PNG row order: the UV convention already handles V orientation.
 # An additional row flip inverted liveries in the actual PICA renderer.
 parts=[]
 for size in [512,256,128,64,32,16,8]:
  a=np.asarray(image.resize((size,size),Image.Resampling.LANCZOS),dtype=np.uint16)
  rgb=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3);y,x=np.indices((size,size));m=np.zeros((size,size),dtype=np.int64)
  for bit in range(3):m|=((x>>bit)&1)<<(bit*2);m|=((y>>bit)&1)<<(bit*2+1)
  index=((y//8)*(size//8)+x//8)*64+m;out=np.empty(size*size,dtype='<u2');out[index.ravel()]=rgb.ravel();parts.append(out.tobytes())
 raw=b''.join(parts);assert len(raw)==699008;(root/'data'/f'{name}_texture.bin').write_bytes(raw);print(name,len(raw))
