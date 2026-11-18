from pathlib import Path
import struct,json,io
import numpy as np
from PIL import Image
class Glb:
 def __init__(self,path):
  raw=Path(path).read_bytes();length,typ=struct.unpack_from('<II',raw,12);self.d=json.loads(raw[20:20+length]);offset=20+length;length,typ=struct.unpack_from('<II',raw,offset);self.bin=raw[offset+8:offset+8+length]
 def accessor(self,index):
  a=self.d['accessors'][index];b=self.d['bufferViews'][a['bufferView']];dtype={5121:'u1',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']];n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];offset=b.get('byteOffset',0)+a.get('byteOffset',0);item=np.dtype(dtype).itemsize
  return np.ndarray((a['count'],n),dtype=dtype,buffer=self.bin,offset=offset,strides=(b.get('byteStride',n*item),item)).copy()
 def meshes(self):
  def visit(i,parent):
   node=self.d['nodes'][i];matrix=np.eye(4)
   if 'matrix'in node:matrix=np.array(node['matrix']).reshape(4,4).T
   else:
    x,y,z,w=node.get('rotation',[0,0,0,1]);matrix[:3,:3]=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]];matrix[:3,:3]*=node.get('scale',[1,1,1]);matrix[:3,3]=node.get('translation',[0,0,0])
   matrix=parent@matrix
   if 'mesh'in node:
    mesh=self.d['meshes'][node['mesh']]
    for p in mesh['primitives']:
     attr=p['attributes'];pos=self.accessor(attr['POSITION']);pos=(np.column_stack([pos,np.ones(len(pos))])@matrix.T)[:,:3]
     uv=self.accessor(attr['TEXCOORD_0']) if 'TEXCOORD_0'in attr else np.zeros((len(pos),2))
     faces=self.accessor(p['indices']).reshape(-1,3) if 'indices'in p else np.arange(len(pos)).reshape(-1,3)
     yield node.get('name',''),p.get('material',0),pos,uv,faces
   for child in node.get('children',[]):yield from visit(child,matrix)
  for n in self.d['scenes'][self.d.get('scene',0)]['nodes']:yield from visit(n,np.eye(4))
 def image(self,index):
  image=self.d['images'][index];view=self.d['bufferViews'][image['bufferView']];return Image.open(io.BytesIO(self.bin[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']])).convert('RGBA')
