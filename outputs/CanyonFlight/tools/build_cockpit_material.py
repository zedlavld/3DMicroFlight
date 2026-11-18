"""Build original subtle matte grain; no third-party photograph pixels used."""
from pathlib import Path
from PIL import Image
import random,struct
r=Path(__file__).resolve().parent.parent;rng=random.Random(2101)
im=Image.new('RGB',(128,128));pix=im.load();raw=bytearray(32768)
def morton(x,y):return sum(((x>>b)&1)<<(2*b)|((y>>b)&1)<<(2*b+1) for b in range(3))
for y in range(128):
 for x in range(128):
  n=255 if x<8 and y<8 else max(210,min(250,233+rng.randrange(-12,13)+(2 if (x+y)%4==0 else 0)))
  pix[x,y]=(n,n,n);color=((n>>3)<<11)|((n>>2)<<5)|(n>>3)
  index=((y//8)*16+x//8)*64+morton(x%8,y%8);struct.pack_into('<H',raw,index*2,color)
im.save(r/'art/cockpit_material.png');(r/'data/cockpit_material.bin').write_bytes(raw)
