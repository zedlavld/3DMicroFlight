"""Render the actual ARM-generated world meshes for offline visual inspection.
Desktop preview only; requires numpy and Pillow. Not a handheld screenshot.
"""
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parent.parent
W,H=800,440
canvas=Image.new('RGB',(800,1380),(12,22,30));draw=ImageDraw.Draw(canvas)
views=[('SEDONA - sample airport',[-800,-1200,2050],[-45,0,1460]),('FLAGSTAFF - San Francisco Peaks',[10500,44500,2800],[10500,56000,2600]),('GRAND CANYON - South Rim',[-41000,133000,3100],[-37000,143000,1200])]
for i,(name,eye,target) in enumerate(views):
 vertices=np.fromfile(root/'validation'/f'world_{i}.bin',dtype='<f4').reshape(-1,3,6)
 eye=np.array(eye,dtype=float);f=np.array(target)-eye;f=f/np.linalg.norm(f);r=np.cross(f,[0,0,1]);r/=np.linalg.norm(r);u=np.cross(r,f)
 image=np.zeros((H,W,3),dtype=np.uint8);image[:]=(125,177,207);zb=np.full((H,W),np.inf)
 for face in vertices:
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
  color=np.clip((weights@face[:,3:])*255,0,255).astype('uint8');image[y0:y1+1,x0:x1+1][mask]=color[mask]
 y=i*455;canvas.paste(Image.fromarray(image),(0,y));draw.text((15,y+13),name,fill=(247,246,232))
draw.text((12,1367),'DESKTOP MESH PREVIEW | USGS 3DEP elevation; generic sample airport buildings',fill=(220,227,233))
canvas.save(root/'art'/'terrain_preview.png')
