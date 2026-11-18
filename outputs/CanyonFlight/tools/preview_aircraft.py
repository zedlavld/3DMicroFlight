"""Desktop asset preview of actual ARM-generated mesh and PNG atlases.
This is not a 3DS screenshot. Requires Pillow and numpy.
"""
from pathlib import Path
import struct,math
import numpy as np
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parent.parent
W,H=480,380
canvas=Image.new('RGB',(1920,800),(13,22,32));draw=ImageDraw.Draw(canvas)
for aircraft,name in enumerate(['sr22','sf50']):
 raw=(root/'validation'/f'mesh_{aircraft}.bin').read_bytes();faces=[]
 for offset in range(0,len(raw),76):
  values=struct.unpack_from('<19f',raw,offset);faces.append(np.array(values[:18]).reshape(3,6))
 texture=np.array(Image.open(root/'art'/f'{name}_paint.png').convert('RGB'))
 for view,eye in enumerate([(11,14,-7),(0,18,0),(0,0,-20),(18,0,-.2)]):
  eye=np.array(eye,dtype=float);forward=-eye/np.linalg.norm(eye);right=np.cross(forward,[1,0,0] if view==2 else [0,0,-1]);right/=np.linalg.norm(right);up=np.cross(right,forward)
  image=np.zeros((H,W,3),dtype=np.uint8);image[:]=(143,179,201)
  zbuffer=np.full((H,W),np.inf)
  projected=[]
  for face in faces:
   d=face[:,:3]-eye;camera=np.stack([d@right,d@up,d@forward],axis=1)
   screen=np.stack([W/2+790*camera[:,0]/camera[:,2],H/2-790*camera[:,1]/camera[:,2]],axis=1)
   if view>0:
    screen=np.stack([W/2+35*camera[:,0],H/2-35*camera[:,1]],axis=1)
   projected.append((camera[:,2].mean(),screen,camera[:,2],face[:,3:5],face[:,5]))
  for _,p,z,uv,light in sorted(projected,key=lambda x:-x[0]):
   x0=max(0,int(p[:,0].min()));x1=min(W-1,int(p[:,0].max())+1);y0=max(0,int(p[:,1].min()));y1=min(H-1,int(p[:,1].max())+1)
   if x1<x0 or y1<y0:continue
   xx,yy=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
   denom=(p[1,1]-p[2,1])*(p[0,0]-p[2,0])+(p[2,0]-p[1,0])*(p[0,1]-p[2,1])
   if abs(denom)<1e-7:continue
   a=((p[1,1]-p[2,1])*(xx-p[2,0])+(p[2,0]-p[1,0])*(yy-p[2,1]))/denom
   b=((p[2,1]-p[0,1])*(xx-p[2,0])+(p[0,0]-p[2,0])*(yy-p[2,1]))/denom;c=1-a-b
   weights=np.stack([a,b,c],axis=-1)
   if view==0:
    weights=weights/z;depth=1/weights.sum(axis=-1);weights/=weights.sum(axis=-1,keepdims=True)
   else:depth=weights@z
   mask=(a>=0)&(b>=0)&(c>=0)&(depth<zbuffer[y0:y1+1,x0:x1+1])
   zbuffer[y0:y1+1,x0:x1+1][mask]=depth[mask]
   tex=weights@uv;shade=weights@light
   tx=np.clip((tex[:,:,0]*(texture.shape[1]-1)).astype(int),0,texture.shape[1]-1);ty=np.clip(((1-tex[:,:,1])*(texture.shape[0]-1)).astype(int),0,texture.shape[0]-1)
   pixels=np.clip(texture[ty,tx]*shade[:,:,None],0,255).astype(np.uint8)
   image[y0:y1+1,x0:x1+1][mask]=pixels[mask]
  x=view*480;y=aircraft*390+15;canvas.paste(Image.fromarray(image),(x,y))
  draw.text((x+15,y+14),name.upper()+' | '+['quarter view','side silhouette','top silhouette','front silhouette'][view],fill=(15,25,35))
draw.text((15,785),'DESKTOP ASSET PREVIEW - adapted CC BY models; see third_party/aircraft/ATTRIBUTION.md; not a 3DS screenshot',fill=(210,225,237))
canvas.save(root/'art'/'aircraft_preview.png')
detail=Image.new('RGB',(960,800),(13,22,32))
detail.paste(canvas.crop((0,0,480,780)),(0,0));detail.paste(canvas.crop((1440,0,1920,780)),(480,0))
ImageDraw.Draw(detail).text((12,785),'SR22: KOG_THORNS / E. BARANGER | SF50: hilos run / EGP-JET3D | desktop preview',fill=(210,225,237))
detail.save(root/'art'/'aircraft_detail_preview.png')
