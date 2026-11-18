"""Run production cockpit geometry on ARM; preview the resulting body-space mesh.
Preview uses software projection, clipping and a depth buffer matching the cockpit renderer,
with an illustrative sky background, not actual terrain or a GPU capture.
"""
from pathlib import Path
import json,struct,subprocess,math,sys
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parent.parent;out=r/'build/cockpit_tests';out.mkdir(parents=True,exist_ok=True);elf=out/'cockpit.elf'
subprocess.run(['C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe','-Wall','-O2','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(r/'include'),str(r/'source/cockpit.c'),str(r/'source/camera.c'),str(r/'source/flight.c'),str(r/'source/fbw.c'),str(r/'source/f16_aero.c'),str(r/'source/ground.c'),str(r/'source/airports.c'),str(r/'tests/cockpit_test.c'),'-Wl,-e,main','-lm','-o',str(elf)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x2000000)
with elf.open('rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x1f00000);u.reg_write(UC_ARM_REG_LR,0x1ff0000)
u.emu_start(sy['main'],0x1ff0000,count=100000000);assert u.reg_read(UC_ARM_REG_PC)==0x1ff0000
report={k:struct.unpack('<i',u.mem_read(sy[k],4))[0] for k in ['checks','failures','firstFailure']};counts=struct.unpack('<2i',u.mem_read(sy['counts'],8));report['mesh_triangles']=counts;fixed=struct.unpack('<2i',u.mem_read(sy['fixedCounts'],8));report['gpu_runtime_verified']=False
def clip(poly,fn):
 out=[]
 for p,q in zip(poly,poly[1:]+poly[:1]):
  a,b=fn(p),fn(q)
  if a>=0:out.append(p)
  if (a>=0)!=(b>=0):out.append(tuple(x+a/(a-b)*(y-x) for x,y in zip(p,q)))
 return out
sheet=Image.new('RGB',(1600,540),(6,12,20));sd=ImageDraw.Draw(sheet);poses=[(0,0,'Forward'),(0,-.22,'Look down'),(-.85,-.08,'Look left'),(.85,-.08,'Look right')]
for aircraft,count in enumerate(counts):
 raw=bytes(u.mem_read(sy['meshes']+aircraft*1280*40,count*40));faces=[]
 for i in range(count):
  values=struct.unpack('<9fI',raw[i*40:(i+1)*40]);points=np.array(values[:9]).reshape(3,3);a,b,c=points;color=values[9];light=1
  if i<fixed[aircraft] and not(abs(a[0]-b[0])<.00001 and abs(a[0]-c[0])<.00001 and .98<a[0]<1.018):
   normal=np.cross(b-a,c-a);light=min(1,max(.7,.7+.3*abs(np.dot(normal,[.25,.35,-.9]))/max(np.linalg.norm(normal),.000001)))
  color=(255<<24)|sum(int(((color>>(8*k))&255)*light)<<(8*k) for k in range(3))
  faces.append(([values[j:j+3] for j in (0,3,6)],color))
 for column,(yaw,pitch,name) in enumerate(poses):
  im=Image.new('RGB',(400,240));d=ImageDraw.Draw(im)
  for y in range(240):d.line((0,y,400,y),fill=(65+int(y*.15),122+int(y*.1),173+int(y*.12)))
  horizon=120+220*math.tan(pitch);d.polygon([(0,horizon),(400,horizon),(400,240),(0,240)],fill=(142,100,64))
  cy,syaw,cp,sp=math.cos(yaw),math.sin(yaw),math.cos(pitch),math.sin(pitch)
  def project(p):
   x,y,z=p;return (-syaw*x+cy*y,-sp*cy*x-sp*syaw*y-cp*z,cp*cy*x+cp*syaw*y-sp*z)
  pixels=np.array(im);depth=np.zeros((240,400),dtype=np.float32)
  projected=[([project(p) for p in f],c) for f,c in faces]
  for poly,color in sorted(projected,key=lambda f:sum(p[2] for p in f[0]),reverse=True):
   for fn in [lambda p:p[2]-.07,lambda p:p[0]+p[2]*200/220,lambda p:p[2]*200/220-p[0],lambda p:p[1]+p[2]*120/220,lambda p:p[2]*120/220-p[1]]:
    if not poly:break
    poly=clip(poly,fn)
   for k in range(1,len(poly)-1):
    points=[poly[0],poly[k],poly[k+1]];v=np.array([(200+220*x/z,120-220*y/z,1/z) for x,y,z in points])
    x0=max(0,int(np.floor(v[:,0].min())));x1=min(399,int(np.ceil(v[:,0].max())));y0=max(0,int(np.floor(v[:,1].min())));y1=min(239,int(np.ceil(v[:,1].max())))
    if x1<x0 or y1<y0:continue
    xx,yy=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5);a,b,c=v
    den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
    if abs(den)<1e-8:continue
    w0=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den;w1=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den;w2=1-w0-w1
    inv=w0*a[2]+w1*b[2]+w2*c[2];region=depth[y0:y1+1,x0:x1+1];mask=(w0>=0)&(w1>=0)&(w2>=0)&(inv>region)
    region[mask]=inv[mask];pixels[y0:y1+1,x0:x1+1][mask]=(color&255,(color>>8)&255,(color>>16)&255)
  im=Image.fromarray(pixels)
  sheet.paste(im,(column*400,aircraft*270+25));sd.text((column*400+8,aircraft*270+6),('SF50' if aircraft else 'SR22')+' / '+name,fill='white')
  if column==0:im.save(r/'art'/('cockpit_sf50.png' if aircraft else 'cockpit_sr22.png'))
sheet.save(r/'art/cockpit_views.png');(r/'validation/cockpit_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));sys.exit(bool(report['failures']))
