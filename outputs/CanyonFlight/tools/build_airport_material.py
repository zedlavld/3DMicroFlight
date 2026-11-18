"""Original seamless aggregate texture with a full RGB565 mip chain."""
from pathlib import Path
from PIL import Image
import math,random,struct
r=Path(__file__).resolve().parent.parent;rng=random.Random(2501)
im=Image.new('RGB',(128,128));px=im.load()
for y in range(128):
 for x in range(128):
  noise=rng.randrange(-17,18)
  broad=4*math.sin(2*math.pi*x/128)*math.cos(4*math.pi*y/128)
  n=int(max(175,min(255,229+noise+broad)))
  px[x,y]=(n,max(0,n-1),max(0,n-3))
def morton(x,y):return sum(((x>>b)&1)<<(2*b)|((y>>b)&1)<<(2*b+1) for b in range(3))
raw=bytearray();level=im
while level.width>=8:
 size=level.width;data=bytearray(size*size*2)
 for y in range(size):
  for x in range(size):
   red,green,blue=level.getpixel((x,y));c=((red>>3)<<11)|((green>>2)<<5)|(blue>>3)
   index=((y//8)*(size//8)+x//8)*64+morton(x%8,y%8)
   struct.pack_into('<H',data,index*2,c)
 raw.extend(data);level=level.resize((size//2,size//2),Image.Resampling.BOX)
im.save(r/'art/airport_material.png');(r/'data/airport_material.bin').write_bytes(raw)
assert len(raw)==43648
print('Airport texture: 128 square, 5 mip levels,',len(raw),'bytes')
