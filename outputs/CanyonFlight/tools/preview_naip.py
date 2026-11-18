"""Desktop comparison from actual ARM terrain/clipping output; not a 3DS capture."""
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parent.parent
W,H=800,440
canvas=Image.new('RGB',(1600,945),(12,22,30));draw=ImageDraw.Draw(canvas)
base=np.fromfile(root/'validation/world_0.bin',dtype='<f4').reshape(-1,3,6)
photo=np.fromfile(root/'validation/imagery_vertices.bin',dtype='<f4').reshape(-1,3,9)
materials=np.fromfile(root/'validation/imagery_materials.bin',dtype='<i4')
textures=[]
for tile in range(8):
 raw=(root/'sdmc/3ds/CanyonFlight/scenery'/f'sedona_{tile}.cft').read_bytes();size=1024 if tile<4 else 512;y,x=np.indices((size,size));m=np.zeros((size,size),dtype=np.int64)
 for bit in range(3):m|=((x>>bit)&1)<<(2*bit);m|=((y>>bit)&1)<<(2*bit+1)
 index=((y//8)*(size//8)+x//8)*64+m;p=np.frombuffer(raw[32:32+size*size*2],dtype='<u2')[index][::-1]
 textures.append(np.stack([((p>>11)&31)*255/31,((p>>5)&63)*255/63,(p&31)*255/31],axis=-1))
for row,(eye,target) in enumerate([([-800,-1200,2050],[-45,0,1460]),([-3000,-3300,3300],[0,0,1500])]):
 for col in range(2):
  eye=np.array(eye,dtype=float);f=np.array(target)-eye;f/=np.linalg.norm(f);r=np.cross(f,[0,0,1]);r/=np.linalg.norm(r);u=np.cross(r,f)
  image=np.zeros((H,W,3),dtype=np.uint8);image[:]=(125,177,207);zb=np.full((H,W),np.inf)
  vertices=photo if col else base
  for index,face in enumerate(vertices):
   d=face[:,:3]-eye;camera=np.stack([d@r,d@u,d@f],axis=1);z=camera[:,2]
   if (z<2).any():continue
   p=np.stack([W/2+580*camera[:,0]/z,H/2-580*camera[:,1]/z],axis=1)
   x0=max(0,int(p[:,0].min()));x1=min(W-1,int(p[:,0].max())+1);y0=max(0,int(p[:,1].min()));y1=min(H-1,int(p[:,1].max())+1)
   if x1<x0 or y1<y0:continue
   den=(p[1,1]-p[2,1])*(p[0,0]-p[2,0])+(p[2,0]-p[1,0])*(p[0,1]-p[2,1])
   if abs(den)<1e-8:continue
   xx,yy=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
   a=((p[1,1]-p[2,1])*(xx-p[2,0])+(p[2,0]-p[1,0])*(yy-p[2,1]))/den
   b=((p[2,1]-p[0,1])*(xx-p[2,0])+(p[0,0]-p[2,0])*(yy-p[2,1]))/den;c=1-a-b
   weights=np.stack([a,b,c],axis=-1)/z;total=weights.sum(axis=-1);depth=1/np.where(total==0,1e-30,total);weights/=np.where(total==0,1e-30,total)[:,:,None]
   mask=(a>=0)&(b>=0)&(c>=0)&(depth<zb[y0:y1+1,x0:x1+1]);zb[y0:y1+1,x0:x1+1][mask]=depth[mask]
   color=weights@(face[:,5:8] if col else face[:,3:6])*255
   if col and materials[index]>=0:
    uv=weights@face[:,3:5];sx=np.clip(uv[:,:,0]*textures[materials[index]].shape[1]-.5,0,textures[materials[index]].shape[1]-1);sy=np.clip((1-uv[:,:,1])*textures[materials[index]].shape[0]-.5,0,textures[materials[index]].shape[0]-1)
    ix=sx.astype(int);iy=sy.astype(int);jx=np.minimum(ix+1,textures[materials[index]].shape[1]-1);jy=np.minimum(iy+1,textures[materials[index]].shape[0]-1);fx=(sx-ix)[:,:,None];fy=(sy-iy)[:,:,None];tex=textures[materials[index]]
    sampled=(tex[iy,ix]*(1-fx)+tex[iy,jx]*fx)*(1-fy)+(tex[jy,ix]*(1-fx)+tex[jy,jx]*fx)*fy
    alpha=(weights@face[:,8])[:,:,None]
    if materials[index]<4:
     tile=materials[index];pworld=weights@face[:,:3];coarse=textures[tile+4]
     cu=(1+(pworld[:,:,0]-(-8192+(tile%2)*8192))*510/8192)/512
     cv=(1+(pworld[:,:,1]-(-8192+(tile//2)*8192))*510/8192)/512
     cx=np.clip(cu*512-.5,0,511);cy=np.clip((1-cv)*512-.5,0,511);ci=cx.astype(int);cj=cy.astype(int);fx=(cx-ci)[:,:,None];fy=(cy-cj)[:,:,None]
     fallback=(coarse[cj,ci]*(1-fx)+coarse[cj,np.minimum(ci+1,511)]*fx)*(1-fy)+(coarse[np.minimum(cj+1,511),ci]*(1-fx)+coarse[np.minimum(cj+1,511),np.minimum(ci+1,511)]*fx)*fy
     color=sampled*alpha+fallback*(1-alpha)
    else:color=sampled*alpha+color*(1-alpha)
   image[y0:y1+1,x0:x1+1][mask]=np.clip(color,0,255).astype('uint8')[mask]
  canvas.paste(Image.fromarray(image),(col*800,row*465+22));draw.text((col*800+15,row*465+5),'V10 - expanded NAIP imagery' if col else 'Procedural terrain colors',fill=(247,246,232))
draw.text((12,931),'DESKTOP PREVIEW - not a 3DS screenshot | USGS / USDA NAIP imagery | generic airport models retained',fill=(220,227,233))
canvas.save(root/'art/sedona_textured_preview.png')
