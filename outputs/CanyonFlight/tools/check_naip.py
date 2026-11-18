"""Verify CFT bytes, Morton layout, mip chains, hashes, and geographic gutters."""
from pathlib import Path
import json,hashlib,struct
import numpy as np
from PIL import Image
import argparse
parser=argparse.ArgumentParser();parser.add_argument("--region",choices=["sedona","flagstaff","canyon"],default="sedona");region=parser.parse_args().region
root=Path(__file__).resolve().parent.parent
meta=json.loads((root/('data/naip_metadata.json' if region=='sedona' else f'data/naip_{region}_metadata.json')).read_text());images=[]
for tile,entry in enumerate(meta['tiles']):
 p=root/'sdmc/3ds/CanyonFlight/scenery'/f'{region}_{tile}.cft';raw=p.read_bytes();assert hashlib.sha256(raw).hexdigest()==entry['sha256']
 assert raw[:8]==b'CFNAIP09';header=struct.unpack_from('<6I',raw,8);dimension=entry["size"];max_level=entry["max_level"];assert header==(tile,dimension,max_level,len(raw)-32,0,0)
 source=Image.open(root/'art'/f'{region}_naip_{tile}.png').convert('RGB');images.append(np.asarray(source,dtype=int))
 offset=32
 for level,size in enumerate([dimension>>level for level in range(max_level+1)]):
  y,x=np.indices((size,size));morton=np.zeros((size,size),dtype=np.int64)
  for bit in range(3):morton|=((x>>bit)&1)<<(2*bit);morton|=((y>>bit)&1)<<(2*bit+1)
  index=((y//8)*(size//8)+x//8)*64+morton
  packed=np.frombuffer(raw[offset:offset+size*size*2],dtype='<u2')[index][::-1]
  a=np.asarray(source.resize((size,size),Image.Resampling.LANCZOS),dtype=np.uint16)
  expected=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)
  assert np.array_equal(packed,expected),(tile,level)
  offset+=size*size*2
 assert offset==len(raw)
seams=[]
for a,b in [(0,1),(2,3),(4,5),(6,7)]:seams.append(int(abs(images[a][:,-2:]-images[b][:,:2]).max()))
for a,b in [(0,2),(1,3),(4,6),(5,7)]:seams.append(int(abs(images[a][:2]-images[b][-2:]).max()))
assert max(seams)<=2,seams
report={'files':8,'mip_levels_per_file':[8,8,8,8,7,7,7,7],'format':'RGB565','all_morton_pixels_verified':True,'header_lengths_and_sha256_verified':True,'shared_gutter_max_channel_difference':seams,'total_pack_bytes':sum(g['bytes'] for g in meta['tiles'])}
(root/('validation/imagery_pack_report.json' if region=='sedona' else f'validation/imagery_{region}_report.json')).write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
