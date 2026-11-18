"""Render actual compiled ARM PFD drawing commands. Requires devkitARM, Pillow, numpy, Unicorn and pyelftools. Fonts approximate the 3DS font; not a GPU screenshot."""
from pathlib import Path
import tempfile,subprocess,os
previewDir=Path(__file__).resolve().parent.parent/"build/pfd_preview";previewDir.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parent.parent;s=(root/'source/main.c').read_text();pfd=s[s.index('static void pfdTriangle'):s.index('int main(void)')]
header=r'''
#include "flight.h"
#include "instruments.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#define PI 3.14159265359f
typedef uint32_t u32;
typedef struct {int type;float p[8];u32 color;char text[64];} Command;
Command commands[3][1024];int commandCounts[3],active;
static u32 white,black,cyan,yellow;
static u32 C2D_Color32(int r,int g,int b,int a){return r|(g<<8)|(b<<16)|((u32)a<<24);}
static Command *emit(int type,u32 color){int i=commandCounts[active]++;if(i>=1024){commandCounts[active]=1024;i=1023;}Command *c=&commands[active][i];c->type=type;c->color=color;return c;}
static void rect(float x,float y,float w,float h,u32 color){Command *c=emit(0,color);c->p[0]=x;c->p[1]=y;c->p[2]=w;c->p[3]=h;}
static void line(float x,float y,float a,float b,float w,u32 color){Command *c=emit(1,color);c->p[0]=x;c->p[1]=y;c->p[2]=a;c->p[3]=b;c->p[4]=w;}
static void label(float x,float y,float scale,u32 color,const char *fmt,...){Command *c=emit(2,color);c->p[0]=x;c->p[1]=y;c->p[2]=scale;va_list ap;va_start(ap,fmt);vsnprintf(c->text,64,fmt,ap);va_end(ap);}
static void C2D_DrawTriangle(float ax,float ay,u32 ac,float bx,float by,u32 bc,float cx,float cy,u32 cc,float z){Command *c=emit(3,ac);c->p[0]=ax;c->p[1]=ay;c->p[2]=bx;c->p[3]=by;c->p[4]=cx;c->p[5]=cy;}
static void C2D_DrawEllipseSolid(float x,float y,float z,float w,float h,u32 color){Command *c=emit(4,color);c->p[0]=x;c->p[1]=y;c->p[2]=w;c->p[3]=h;}
'''
main=r'''
int main(void){
 white=C2D_Color32(245,245,240,255);black=C2D_Color32(12,18,25,255);cyan=C2D_Color32(62,221,240,255);yellow=C2D_Color32(255,224,54,255);
 for(active=0;active<3;active++){
  Flight s;resetFlight(&s,active==0?0:1,active==2?2:0);
  if(active==1){setAttitude(&s,.28f,.08f,1.0f);s.vel=(Vec){60,65,4};s.wind=(Vec){8,0,0};s.weather=2;s.beta=.03f;s.ias=evaluateAero(&s).ias;}
  if(active==2){s.pos.z=8000;s.vel=(Vec){100,100,0};s.ias=evaluateAero(&s).ias;}
  attitude(&s,60,0);
 }
 return 0;
}
'''
(previewDir/'pfd_dump.c').write_text(header+pfd+main)

compiler=Path(os.environ.get('DEVKITARM','C:/devkitPro/devkitARM'))/'bin/arm-none-eabi-gcc.exe'
if not compiler.exists():compiler=Path('C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe')
sources=[root/'source'/f for f in ['flight.c','fbw.c','f16_aero.c','ground.c','instruments.c','terrain.c','terrain_stream.c','terrain_data.c','airports.c']]
subprocess.run([str(compiler),'-O2','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(root/'include'),*[str(f) for f in sources],str(previewDir/'pfd_dump.c'),'-Wl,-e,main','-lm','-o',str(previewDir/'pfd_dump.elf')],check=True)
from pathlib import Path
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
import struct,json
from PIL import Image,ImageDraw,ImageFont
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x02000000)
with open(previewDir/'pfd_dump.elf','rb') as f:
 elf=ELFFile(f)
 for seg in elf.iter_segments():
  if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
 symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x1f00000);u.reg_write(UC_ARM_REG_LR,0x1ff0000);u.emu_start(symbols['main'],0x1ff0000,count=500000000)
assert u.reg_read(UC_ARM_REG_PC)==0x1ff0000
counts=struct.unpack('<3i',u.mem_read(symbols['commandCounts'],12));scale=3;canvas=Image.new('RGB',(320*3*scale,260*scale),(15,22,32));titles=['SR22 / APPROACH','SF50 / BANK + WIND','SF50 / HIGH ALTITUDE'];data=[]
for scene,count in enumerate(counts):
 assert 0<count<1024
 image=Image.new('RGB',(320*scale,240*scale));draw=ImageDraw.Draw(image);commands=[]
 for i in range(count):
  raw=bytes(u.mem_read(symbols['commands']+(scene*1024+i)*104,104));kind=struct.unpack_from('<i',raw)[0];p=struct.unpack_from('<8f',raw,4);rgba=struct.unpack_from('<4B',raw,36);text=raw[40:104].split(b'\0')[0].decode('utf-8',errors='replace');color=rgba[:3];commands.append({'type':kind,'p':p,'color':rgba,'text':text})
  def pt(x,y):return (x*scale,y*scale)
  if kind in [0,4]:
   xy=[p[0]*scale,p[1]*scale,(p[0]+p[2])*scale,(p[1]+p[3])*scale]
   (draw.rectangle if kind==0 else draw.ellipse)(xy,fill=color)
  elif kind==1:draw.line([pt(p[0],p[1]),pt(p[2],p[3])],fill=color,width=max(1,round(p[4]*scale)))
  elif kind==3:draw.polygon([pt(p[0],p[1]),pt(p[2],p[3]),pt(p[4],p[5])],fill=color)
  elif kind==2:
   font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',max(1,round(30*p[2]*scale)));draw.text(pt(p[0],p[1]),text,fill=color,font=font,anchor='lt')
 image.resize((320,240),Image.Resampling.LANCZOS).save(root/'art'/f'pfd_state_{scene}.png')
 canvas.paste(image,(scene*320*scale,20*scale));ImageDraw.Draw(canvas).text((scene*320*scale+8,6),titles[scene]+' / DRAW-COMMAND PREVIEW',fill=(200,220,230),font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',20))
 data.append(commands)
canvas.save(root/'art/pfd_preview.png');(root/'validation/pfd_draw_commands.json').write_text(json.dumps({'note':'Actual compiled ARM PFD drawing commands; approximate desktop fonts, not an emulator screenshot','commandCounts':counts,'states':data}))
print(counts)
