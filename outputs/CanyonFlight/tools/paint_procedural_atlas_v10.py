"""Original composite-aircraft liveries and material atlas, authored in code.
No third-party aircraft textures. PNGs are 512 square; packed RGB565 mip chain.
Use --pack-only after editing the PNGs while retaining the UV layout.
"""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import numpy as np,math,sys,json
if '--legacy' not in sys.argv:raise SystemExit('Legacy v10 artwork: pass --legacy to overwrite current source-model textures intentionally.')
root=Path(__file__).resolve().parent.parent
for name,accent in [('sr22',(28,65,112)),('sf50',(42,48,57))]:
 path=root/'art'/f'{name}_paint.png'
 if '--pack-only' not in sys.argv:
  y,x=np.indices((1024,1024));ratio=np.clip((x/1024-.02)/.71,0,1);theta=(y/1024-.04)/.92*2*math.pi
  height=np.sin(theta);cut=.23+.23*np.sin(ratio*math.pi*.8)**2
  noise=np.random.default_rng(22 if name=='sr22' else 50).normal(0,.65,(1024,1024))
  base=np.stack([238+2*np.cos(theta),241+2*np.cos(theta),243+2*np.cos(theta)],axis=-1).astype(float)
  base+=noise[:,:,None];base-=np.maximum(height,0)[:,:,None]*3
  blue=height>cut;base[blue]=np.array(accent)+noise[blue,None]
  pin=(height>cut-.045)&(height<cut-.018);base[pin]=(145,161,174) if name=='sr22' else (174,158,129)
  # A narrow dark companion pinstripe; composite skins get restrained weathering.
  pin=(height>cut-.09)&(height<cut-.075);base[pin]=np.array(accent)*.72
  im=Image.fromarray(np.clip(base,0,255).astype('uint8'));d=ImageDraw.Draw(im)
  def body(xfrac,th):return (int((.02+.71*xfrac)*1024),int((.04+.92*th/(2*math.pi))*1024))
  # Cowl/radome seam and aft service seam. Doors/windows are conforming mesh patches.
  for f in ([.055,.17] if name=='sr22' else [.055,.12]):
   xx=body(f,0)[0];d.line((xx,41,xx,983),fill=(187,194,199),width=1)
  for th in [0,math.pi,2*math.pi]:
   pts=[body(f,th+.035*math.sin(f*5)) for f in np.linspace(.10,.36,30)]
   d.line(pts,fill=(207,214,219),width=1)
  # Fictional registration, two side positions. Not an official manufacturer livery.
  try:font=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',19)
  except OSError:font=ImageFont.load_default()
  for th in [-.23,math.pi+.23,2*math.pi-.23]:
   px,py=body(.58,th)
   if py<42 or py>980:continue
   d.text((px,py-10),'N22CF' if name=='sr22' else 'N50CF',font=font,fill=(53,61,70),stroke_width=0)
  # Material strip: paint, accent, glazing, rubber, metal, position lights.
  d.rectangle((784,0,1023,383),fill=(238,241,243))
  for cy,color in [(20,(238,241,243)),(52,accent),(232,(29,31,34)),(264,(145,158,169)),(296,(212,35,28)),(328,(28,186,74))]:
   d.rectangle((790,cy-12,1023,cy+12),fill=color)
  for row in range(100,202):
   f=(row-100)/102;shine=math.exp(-((f-.23)/.075)**2)*37
   d.line((790,row,1023,row),fill=(int(21+18*(1-f)+shine),int(38+24*(1-f)+shine),int(55+32*(1-f)+shine)))
  # Distinct upper/lower wing strips: chord across x, span down y.
  for lower,x0 in [(False,784),(True,902)]:
   d.rectangle((x0,400,x0+110,1012),fill=(233,237,240) if lower else (242,244,245))
   for row in range(402,1011):
    t=(row-400)/610;shade=int(2*math.sin(t*6))
    d.line((x0,row,x0+108,row),fill=(233+shade,237+shade,240+shade) if lower else (241+shade,243+shade,244+shade))
   d.line((x0+79,400,x0+79,1010),fill=(144,154,163),width=1)
   d.line((x0+9,400,x0+9,1010),fill=(206,214,220),width=1)
   d.polygon([(x0+5,930),(x0+107,954),(x0+107,982),(x0+5,954)],fill=accent)
   d.line((x0+5,922,x0+107,946),fill=(143,160,173),width=2)
   for row in [550,825]:
    d.rounded_rectangle((x0+34,row,x0+52,row+21),radius=4,outline=(160,170,180),width=1)
   if not lower:
    d.ellipse((x0+36,544,x0+48,556),fill=(131,143,153),outline=(71,85,98),width=1)
    d.line((x0+38,550,x0+46,550),fill=(225,228,230),width=1)
    d.text((x0+82,750),'NO',fill=(89,99,109));d.text((x0+82,761),'STEP',fill=(89,99,109))
  im.resize((512,512),Image.Resampling.LANCZOS).save(path)
 im=Image.open(path).convert('RGB');assert im.size==(512,512)
 payload=[]
 for size in [512,256,128,64,32,16,8]:
  a=np.asarray(im.resize((size,size),Image.Resampling.LANCZOS),dtype=np.uint16)[::-1]
  rgb=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3);y,x=np.indices((size,size));m=np.zeros((size,size),dtype=np.int64)
  for bit in range(3):m|=((x>>bit)&1)<<(2*bit);m|=((y>>bit)&1)<<(2*bit+1)
  index=((y//8)*(size//8)+x//8)*64+m;out=np.empty(size*size,dtype='<u2');out[index.ravel()]=rgb.ravel();payload.append(out.tobytes())
 raw=b''.join(payload);assert len(raw)==699008;(root/'data'/f'{name}_texture.bin').write_bytes(raw)
 print(name,len(raw),'bytes with seven mip levels')
