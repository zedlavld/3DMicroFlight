"""v33: convert the Boeing 737 MAX, A321 and F-16 GLBs to 3DS exterior meshes.
Requires numpy, Pillow, fast-simplification. Sources/credits/licences are in
third_party/aircraft/ATTRIBUTION.md. The SR22/SF50 path stays in import_aircraft.py.

Output data/<name>_imported_mesh.bin:
  header <7I4f: faceCount, dynamicFirst, dynamicCount (0), gearFirst, gearCount,
                 reserved x2, gearSplitX, noseGearTop, mainGearTop, reserved
  then faceCount AircraftFace records (3 x {x,y,z,u,v,light}, depth) = 76 bytes.
Body axes: x forward, y right, z down, metres, origin at the simulator cg.
Gear faces are last; aircraft_mesh.c folds them up toward noseGearTop /
mainGearTop (split at gearSplitX) as the gear retracts.
data/<name>_texture.bin: 512-square RGB565 mip chain (same layout as the SR22/SF50).
"""
from pathlib import Path
import sys,struct,json,hashlib
import numpy as np
from PIL import Image,ImageDraw
sys.path.insert(0,str(Path(__file__).resolve().parent))
from glb_reader import Glb
import fast_simplification
root=Path(__file__).resolve().parent.parent
src=root/'third_party/aircraft'

def reduce(p,uv,f,target):
 """QEM on the position-welded mesh (source UV seams would otherwise freeze as
 borders), then UVs per face corner from the nearest source triangle, so a
 reduced face never blends two UV islands. Returns unwelded corners, f=arange."""
 if target is None or len(f)<=target:return p,uv,f
 key=np.round(p/(np.ptp(p,0).max()*1e-5)).astype(np.int64)
 _,first,inverse=np.unique(key,axis=0,return_index=True,return_inverse=True);inverse=inverse.ravel()
 wp=p[first];wf=inverse[f];wf=wf[(wf[:,0]!=wf[:,1])&(wf[:,1]!=wf[:,2])&(wf[:,0]!=wf[:,2])]
 q,g=fast_simplification.simplify(wp,wf.astype(np.int32),target_count=target,preserve_border=True,agg=7)
 print('reduce',len(f),'->',len(g),flush=True)
 tri=p[f];cent=tri.mean(1);out=np.zeros((len(g),3,2))
 for a in range(0,len(g),400):
  G=g[a:a+400];c=q[G].mean(1)
  k=np.argmin(((c[:,None,:]-cent[None,:,:])**2).sum(2),axis=1)
  A,B,C=tri[k,0],tri[k,1],tri[k,2];v0=B-A;v1=C-A
  d00=(v0*v0).sum(1);d01=(v0*v1).sum(1);d11=(v1*v1).sum(1);den=np.maximum(d00*d11-d01*d01,1e-20)
  for j in range(3):
   v2=q[G[:,j]]-A;d20=(v2*v0).sum(1);d21=(v2*v1).sum(1)
   bv=(d11*d20-d01*d21)/den;bw=(d00*d21-d01*d20)/den;bu=1-bv-bw
   T=uv[f[k]];out[a:a+400,j]=bu[:,None]*T[:,0]+bv[:,None]*T[:,1]+bw[:,None]*T[:,2]
  lo=uv[f[k]].min(1);hi=uv[f[k]].max(1);out[a:a+400]=np.clip(out[a:a+400],lo[:,None],hi[:,None])
 return q[g].reshape(-1,3),out.reshape(-1,2),np.arange(len(g)*3).reshape(-1,3)

def smoothLight(p,f,welded):
 n=np.cross(p[f[:,1]]-p[f[:,0]],p[f[:,2]]-p[f[:,0]])
 if welded:
  key=np.round(p/(np.ptp(p,0).max()*1e-5)).astype(np.int64);_,inv=np.unique(key,axis=0,return_inverse=True);inv=inv.ravel()
 else:inv=np.arange(len(p))
 acc=np.zeros((inv.max()+1,3))
 for k in range(3):np.add.at(acc,inv[f[:,k]],n)
 normals=acc[inv];normals/=np.maximum(np.linalg.norm(normals,axis=1,keepdims=True),1e-12)
 return np.clip(.70+.25*(-normals[:,2])+.08*normals[:,1],.45,1)

def srgb(c):return tuple(int(round(255*min(1,max(0,x))**(1/2.2))) for x in c[:3])

def cylinder(centre,axis,radius,width,n=12):
 """Closed cylinder (tyre or hub) around axis (unit vector), as points and faces."""
 axis=np.asarray(axis,float);a=np.array([1.,0,0]) if abs(axis[0])<.9 else np.array([0,0,1.])
 u=np.cross(axis,a);u/=np.linalg.norm(u);v=np.cross(axis,u)
 ang=np.linspace(0,2*np.pi,n,endpoint=False);ring=np.outer(np.cos(ang),u)+np.outer(np.sin(ang),v)
 c=np.asarray(centre,float);p=np.concatenate([c+radius*ring-axis*width/2,c+radius*ring+axis*width/2,[c-axis*width/2,c+axis*width/2]])
 f=[]
 for i in range(n):
  j=(i+1)%n;f+=[[i,j,n+i],[j,n+j,n+i],[2*n,j,i],[2*n+1,n+i,n+j]]
 return p,np.array(f)
def box(a,b,size):
 """Square strut of side size from point a to point b."""
 a=np.asarray(a,float);b=np.asarray(b,float);w=(b-a)/np.linalg.norm(b-a)
 r=np.array([1.,0,0]) if abs(w[0])<.9 else np.array([0,1.,0]);u=np.cross(w,r);u/=np.linalg.norm(u);v=np.cross(w,u);h=size/2
 p=np.array([e+s*u*h+t*v*h for e in (a,b) for s,t in ((-1,-1),(1,-1),(1,1),(-1,1))])
 f=[]
 for i in range(4):
  j=(i+1)%4;f+=[[i,j,4+i],[j,4+j,4+i]]
 f+=[[0,1,2],[0,2,3],[4,6,5],[4,7,6]]
 return p,np.array(f)
def proceduralGear(g,groundZ):
 """Our own simple gear for models built without one: one nose wheel, two
 mains, struts and hubs, wheels resting at groundZ (body z, down)."""
 out=[];n=g['nose'];m=g['main']
 r=n['wheelD']/2;hub=(n['x'],n['y'],groundZ-r)
 out+=[(*cylinder(hub,(0,1,0),r,n['wheelW']),'tyre'),(*cylinder(hub,(0,1,0),r*.55,n['wheelW']*1.08),'hub'),
       (*box((n['x']+.15,0,n['top']),(n['x'],0,groundZ-r),.12),'strut')]
 for s in (-1,1):
  r=m['wheelD']/2;hub=(m['x'],s*m['y'],groundZ-r);inner=(m['x'],s*(m['y']-m['wheelW']/2-.05),groundZ-r)
  out+=[(*cylinder(hub,(0,1,0),r,m['wheelW']),'tyre'),(*cylinder(hub,(0,1,0),r*.55,m['wheelW']*1.08),'hub'),
        (*box((m['x']+.25,s*m['strutY'],m['top']),inner,.14),'strut'),(*box(inner,hub,.09),'strut')]
 return out

# Per aircraft: axis map glb->body, target dimensions, gear parts, per-part face targets.
CONFIG={
 'b737':dict(file='b737.glb',
  # glTF: x left, y up, z forward (nose +z). Body: x=z, y=-x, z=-y.
  axes=lambda p:np.column_stack([p[:,2],-p[:,0],-p[:,1]]),
  length=39.52,span=35.92,          # 737 MAX 8: 39.52 m, 35.92 m
  gear=[12,13,14,15,16,17,18,19],targets={},
  mainGearX=-1.0,gearArmZ=2.6),
 'a321':dict(file='a321.glb',
  # Cleaned by tools/clean_a321.py. glTF: x left, y up, z forward (as the 737).
  axes=lambda p:np.column_stack([p[:,2],-p[:,0],-p[:,1]]),
  length=44.51,span=35.80,          # A321-200: 44.51 m, 35.80 m
  gear=list(range(2,17)),targets={}, # Object_13..41: doors, nose gear, both main gears
  mainGearX=-1.0,gearArmZ=2.7),
 'f16':dict(file='f16_rickslash.glb',
  # v35.24: "F-16 jet" by rickslash, CC BY 4.0 (replaces the CC BY-NC-SA model).
  # glTF: x left, y up, z forward (as the 737). Modelled in flight: no gear,
  # so the gear is generated here (proceduralGear) and is our own work.
  axes=lambda p:np.column_stack([p[:,2],-p[:,0],-p[:,1]]),
  length=15.06,span=9.96,           # F-16C: 15.06 m; 9.96 m with wingtip missiles
  gear=[],targets={},
  noseX=9.04,bellyZ=.61,            # same placement as the v33 F-16 (nose, belly near the cg)
  proceduralGear=dict(
   nose=dict(x=3.25,y=0,wheelD=.46,wheelW=.16,top=.45),
   main=dict(x=-.35,y=1.18,wheelD=.70,wheelW=.22,top=.62,strutY=.80)),
  gearArmZ=1.6),
}
report={}
for name,c in [(n,c) for n,c in CONFIG.items() if len(sys.argv)<2 or n in sys.argv[1:]]:
 path=src/c['file'];g=Glb(path);meshes=list(g.meshes())
 mats=g.d.get('materials',[])
 def texOf(m):
  t=mats[m].get('pbrMetallicRoughness',{}).get('baseColorTexture',{}).get('index')
  return None if t is None else g.d['textures'][t]['source']
 # Body-axis geometry and per-axis scale (length from x, span from y).
 P=np.concatenate([c['axes'](m[2]) for m in meshes])
 sx=c['length']/np.ptp(P[:,0]);sy=c['span']/np.ptp(P[:,1])
 sz=sx
 def place(p):return np.column_stack([p[:,0]*sx,(p[:,1]-(P[:,1].min()+P[:,1].max())/2)*sy,p[:,2]*sz])
 if c.get('proceduralGear'):
  # Align: nose at noseX, fuselage belly near the cg at bellyZ; gear added below.
  allP=place(P);dx=c['noseX']-allP[:,0].max()
  near=allP[(np.abs(allP[:,0]+dx)<1.5)&(np.abs(allP[:,1])<.6)];dz=c['bellyZ']-near[:,2].max()
  pg=c['proceduralGear'];split=(pg['nose']['x']+pg['main']['x'])/2
 else:
  # Align: main-gear wheel centre at mainGearX, lowest wheel point at gearArmZ.
  gearP=place(np.concatenate([c['axes'](meshes[j][2]) for j in c['gear']]))
  bottom=gearP[:,2].max();low=gearP[gearP[:,2]>bottom-.25*(bottom-gearP[:,2].min())]
  split=(low[:,0].min()+low[:,0].max())/2;mainLow=low[low[:,0]<split]
  dx=c['mainGearX']-(mainLow[:,0].min()+mainLow[:,0].max())/2;dz=c['gearArmZ']-bottom
  split+=dx
 def coordinates(p):q=place(c['axes'](p));q[:,0]+=dx;q[:,2]+=dz;return q
 # Atlas: textured images in rectangles, flat-colour materials as 48 px swatches.
 images=sorted({texOf(m[1]) for m in meshes if texOf(m[1]) is not None})
 boxes={}
 for im in images:
  uv=np.concatenate([np.clip(m[3],0,1) for m in meshes if texOf(m[1])==im]);boxes[im]=(uv.min(0),uv.max(0))
 rects={1:[(8,8,496,344)],2:[(8,8,344,344),(356,8,148,340)],3:[(8,8,340,344),(356,8,148,168),(356,184,148,168)]}[len(images)]
 atlas=Image.new('RGB',(512,512),(230,232,234))
 place_of={}
 for im,(x,y,w,h) in zip(images,rects):
  lo,hi=boxes[im];img=g.image(im).convert('RGB');W,H=img.size
  crop=img.crop((int(lo[0]*W),int(lo[1]*H),max(int(lo[0]*W)+1,int(np.ceil(hi[0]*W))),max(int(lo[1]*H)+1,int(np.ceil(hi[1]*H)))))
  atlas.paste(crop.resize((w,h),Image.Resampling.LANCZOS),(x,y));place_of[im]=(x,y,w,h,lo,hi)
 d=ImageDraw.Draw(atlas);swatch={}
 flat=[m for m in range(len(mats)) if texOf(m) is None]
 for k,m in enumerate(flat):
  x,y=8+(k%10)*50,362+(k//10)*50;d.rectangle((x,y,x+47,y+47),fill=srgb(mats[m].get('pbrMetallicRoughness',{}).get('baseColorFactor',[1,1,1,1])))
  swatch[m]=((x+24)/512,1-(y+24)/512)
 gearSwatch={}
 if c.get('proceduralGear'):
  for k,(key,col) in enumerate((('tyre',(38,38,40)),('strut',(196,198,200)),('hub',(150,152,156)))):
   x,y=8+(len(flat)+k)%10*50,362+(len(flat)+k)//10*50;d.rectangle((x,y,x+47,y+47),fill=col);gearSwatch[key]=((x+24)/512,1-(y+24)/512)
 atlas.save(root/'art'/(name+'_paint.png'))
 body=[];gear=[];parts=[]
 for j,(node,mat,p,uv,f) in enumerate(meshes):
  p=coordinates(p);uv=np.clip(uv,0,1)
  reduced=c['targets'].get(j) is not None and len(f)>c['targets'][j]
  p,uv,f=reduce(p,uv,f,c['targets'].get(j))
  light=smoothLight(p,f,reduced)
  im=texOf(mat)
  if im is None:uv=np.tile(swatch[mat],(len(p),1))
  else:
   x,y,w,h,lo,hi=place_of[im];t=(uv-lo)/np.maximum(hi-lo,1e-6)
   uv=np.column_stack([(x+.5+t[:,0]*(w-1))/512,1-(y+.5+t[:,1]*(h-1))/512])
  out=gear if j in c['gear'] else body
  for face in f:
   v=np.column_stack([p[face],uv[face],light[face]]).astype('<f4');out.append(v.tobytes()+struct.pack('<f',0))
  parts.append({'part':j,'node':node,'material':mats[mat].get('name'),'triangles':int(len(f)),'gear':j in c['gear']})
 if c.get('proceduralGear'):
  for p,f,key in proceduralGear(c['proceduralGear'],c['gearArmZ']):
   light=smoothLight(p,f,True);uv=np.tile(gearSwatch[key],(len(p),1))
   for face in f:
    v=np.column_stack([p[face],uv[face],light[face]]).astype('<f4');gear.append(v.tobytes()+struct.pack('<f',0))
  parts.append({'part':'procedural gear','triangles':len(gear),'gear':True})
 faces=body+gear;print(name,"faces",len(faces),[(q["part"],q["triangles"]) for q in parts]);assert len(faces)<16000
 G=np.frombuffer(b''.join(gear),dtype='<f4').reshape(-1,19)[:,:18].reshape(-1,3,6)
 gx=G[:,:,0].mean(1);gz=G[:,:,2]
 noseTop=float(gz[gx>=split].min());mainTop=float(gz[gx<split].min())
 raw=struct.pack('<7I4f',len(faces),0,0,len(body),len(gear),0,0,split,noseTop,mainTop,0)+b''.join(faces)
 (root/'data'/(name+'_imported_mesh.bin')).write_bytes(raw)
 chain=[]
 for size in [512,256,128,64,32,16,8]:
  a=np.asarray(atlas.resize((size,size),Image.Resampling.LANCZOS),dtype=np.uint16);rgb=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)
  y,x=np.indices((size,size));m=np.zeros((size,size),dtype=np.int64)
  for bit in range(3):m|=((x>>bit)&1)<<(bit*2);m|=((y>>bit)&1)<<(bit*2+1)
  index=((y//8)*(size//8)+x//8)*64+m;o=np.empty(size*size,dtype='<u2');o[index.ravel()]=rgb.ravel();chain.append(o.tobytes())
 (root/'data'/(name+'_texture.bin')).write_bytes(b''.join(chain))
 A=np.frombuffer(b''.join(faces),dtype='<f4').reshape(-1,19)[:,:18].reshape(-1,3,6)
 report[name]={'source':c['file'],'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'converted_sha256':hashlib.sha256(raw).hexdigest(),
  'triangles':len(faces),'gear_triangles':len(gear),'scale':[sx,sy,sz],'gear_split_x':split,'nose_gear_top':noseTop,'main_gear_top':mainTop,
  'extent_min':A[:,:,:3].reshape(-1,3).min(0).tolist(),'extent_max':A[:,:,:3].reshape(-1,3).max(0).tolist(),'parts':parts,
  'transparency':'opaque approximation of glass materials'}
 print(name,len(faces),'triangles,',len(gear),'gear; extent',np.round(report[name]['extent_min'],2),np.round(report[name]['extent_max'],2),flush=True)
rp=root/'validation/imported_aircraft_v33_report.json';report={**(json.loads(rp.read_text(encoding='utf-8')) if rp.exists() else {}),**report}
rp.write_text(json.dumps(report,indent=1),encoding='utf-8')
