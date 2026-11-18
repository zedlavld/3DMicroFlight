"""Convert the officially downloaded Sketchfab GLBs to 3DS exterior meshes.
Requires numpy, Pillow, fast-simplification. Sources/credits are in third_party/.
"""
from pathlib import Path
import sys,struct,json,hashlib,math
import numpy as np
from PIL import Image,ImageDraw
from glb_reader import Glb
import fast_simplification
root=Path(__file__).resolve().parent.parent
src=root/'third_party/aircraft';src.mkdir(parents=True,exist_ok=True)
def reduce(p,uv,f,target):
 if len(f)<=target:return p,uv,f
 q,g,collapses=fast_simplification.simplify(p,f.astype(np.int32),target_count=target,return_collapses=True,preserve_border=True,agg=7)
 # Replay the QEM collapses to transfer UVs within each mesh's seam boundaries.
 q,g,mapping=fast_simplification.replay_simplification(p,f.astype(np.int32),collapses)
 u=np.zeros((len(q),2));weights=np.zeros(len(q));valid=mapping>=0
 np.add.at(u,mapping[valid],uv[valid]);np.add.at(weights,mapping[valid],1);u/=np.maximum(weights[:,None],1)
 print('reduce',len(f),'->',len(g),flush=True);return q,u,g
palette=[(240,242,244),(35,65,100),(24,43,60),(28,29,32),(145,156,165),(210,30,22),(24,184,65)]
report={}
for name,type in [('sr22',0),('sf50',1)]:
 path=src/(name+'.glb');g=Glb(path);meshes=list(g.meshes())
 keep=[14,15,12,16,10,9] if type==0 else [0,1]
 allp=np.concatenate([meshes[j][2] for j in keep]);forward=allp[:,0] if type==0 else allp[:,2];lateral=allp[:,2] if type==0 else allp[:,0]
 length=7.9248 if type==0 else 9.35736;span=11.684 if type==0 else 11.79576
 sx=length/np.ptp(forward);sy=span/np.ptp(lateral);offset=(3.05 if type==0 else 4.05)-forward.max()*sx
 def coordinates(p):
  return np.column_stack([p[:,0]*sx+offset,(p[:,2]-(lateral.min()+lateral.max())/2)*sy,-(p[:,1]-2)*sy]) if type==0 else np.column_stack([p[:,2]*sx+offset,(p[:,0]-(lateral.min()+lateral.max())/2)*sy,-p[:,1]*sy])
 # One diffuse atlas per aircraft, plus a protected material palette.
 base=g.image(16 if type==0 else 0).convert('RGB').resize((496,496),Image.Resampling.LANCZOS)
 atlas=Image.new('RGB',(512,512),(238,240,242));atlas.paste(base,(8,8));d=ImageDraw.Draw(atlas)
 for j,color in enumerate(palette):d.rectangle((j*32,0,j*32+31,6),fill=color)
 atlas.save(root/'art'/(name+'_paint.png'))
 result=[];parts=[];propfirst=0;propcount=0;propPivot=[0,0,0]
 for j in keep:
  node,material,p,uv,f=meshes[j];p=coordinates(p)
  if type==0:
   target={14:4200,15:1600,12:350,16:274,10:250,9:300}[j];p,uv,f=reduce(p,uv,f,target)
  else:
   f=f[:,[0,2,1]]
   # Preserve the entire source shell. A height cutoff previously mistaken
   # for gear removal also deleted belly/wing-root faces and opened a large
   # boundary. Animated landing
   # gear is supplied separately by aircraft_mesh.c.
  normals=np.zeros_like(p);n=np.cross(p[f[:,1]]-p[f[:,0]],p[f[:,2]]-p[f[:,0]])
  for k in range(3):np.add.at(normals,f[:,k],n)
  normals/=np.maximum(np.linalg.norm(normals,axis=1,keepdims=True),1e-12)
  light=np.clip(.70+.25*(-normals[:,2])+.08*normals[:,1],.45,1)
  glass=(type==0 and j in [12,16]) or (type==1 and material==0)
  if glass:uv=np.tile([80.5/512,1-3.5/512],(len(p),1))
  else:uv=np.column_stack([(8.5+np.clip(uv[:,0],0,1)*495)/512,1-(8.5+np.clip(uv[:,1],0,1)*495)/512])
  if type==0 and j==9:
   propfirst=len(result);propcount=len(f);propPivot=((p.min(0)+p.max(0))/2).tolist()
  for face in f:
   v=np.column_stack([p[face],uv[face],light[face]]).astype('<f4');result.append(v.tobytes()+struct.pack('<f',0))
  parts.append({'mesh':j,'material':material,'triangles':len(f)})
 assert len(result)<7800,(name,len(result))
 raw=struct.pack('<3I3f',len(result),propfirst,propcount,*propPivot)+b''.join(result)
 (root/'data'/(name+'_imported_mesh.bin')).write_bytes(raw)
 # RGB565 mip chain, matching the existing 512-square aircraft texture loader.
 chain=[]
 for size in [512,256,128,64,32,16,8]:
  a=np.asarray(atlas.resize((size,size),Image.Resampling.LANCZOS),dtype=np.uint16);rgb=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)
  y,x=np.indices((size,size));m=np.zeros((size,size),dtype=np.int64)
  for bit in range(3):m|=((x>>bit)&1)<<(bit*2);m|=((y>>bit)&1)<<(bit*2+1)
  index=((y//8)*(size//8)+x//8)*64+m;out=np.empty(size*size,dtype='<u2');out[index.ravel()]=rgb.ravel();chain.append(out.tobytes())
 (root/'data'/(name+'_texture.bin')).write_bytes(b''.join(chain))
 report[name]={'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'converted_sha256':hashlib.sha256(raw).hexdigest(),'triangles':len(result),'parts':parts,'length_scale':sx,'span_scale':sy,'prop_first':propfirst,'prop_count':propcount,'prop_pivot':propPivot,'transparency':'opaque glass approximation; source interiors omitted'}
 print(name,len(result),'triangles',flush=True)
(root/'validation/imported_aircraft_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
