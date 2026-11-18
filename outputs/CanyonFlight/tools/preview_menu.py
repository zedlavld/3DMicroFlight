"""Execute the actual menu/splash drawing functions on ARM, then rasterize their
draw commands with Pillow. Font rasterization approximates the 3DS system font;
this is a layout preview, not a GPU/emulator screenshot.
"""
from pathlib import Path
import json,struct,subprocess
from PIL import Image,ImageDraw,ImageFont
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parent.parent;out=r/'build/menu_preview';out.mkdir(parents=True,exist_ok=True)
main=(r/'source/main.c').read_text();drawing=main[main.index('static void splashScreen'):main.index('int main(void)')]
flight=(r/'source/flight.c').read_text();names=flight[flight.index('const char *flightPhaseName'):flight.index('/* Test presets')]
shim=r'''
#include "app_menu.h"
#include "flight.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
typedef uint32_t u32;
typedef struct {int kind;float v[8];u32 color;char text[160];} Command;
Command commands[8][256];int counts[8];static int screen;
static u32 white,black,cyan,yellow;static int audioReady=1;
static u32 C2D_Color32(int r,int g,int b,int a){return r|(g<<8)|(b<<16)|(a<<24);}
static Command *cmd(int k,u32 c){Command *p=&commands[screen][counts[screen]++];p->kind=k;p->color=c;return p;}
static void rect(float x,float y,float w,float h,u32 c){Command *p=cmd(0,c);p->v[0]=x;p->v[1]=y;p->v[2]=w;p->v[3]=h;}
static void line(float x,float y,float a,float b,float w,u32 c){Command *p=cmd(1,c);p->v[0]=x;p->v[1]=y;p->v[2]=a;p->v[3]=b;p->v[4]=w;}
static void label(float x,float y,float scale,u32 c,const char *fmt,...){Command *p=cmd(2,c);p->v[0]=x;p->v[1]=y;p->v[2]=scale;va_list a;va_start(a,fmt);vsnprintf(p->text,160,fmt,a);va_end(a);}
static void pfdTriangle(float ax,float ay,float bx,float by,float cx,float cy,u32 c){Command *p=cmd(3,c);p->v[0]=ax;p->v[1]=ay;p->v[2]=bx;p->v[3]=by;p->v[4]=cx;p->v[5]=cy;}
static void C2D_DrawEllipseSolid(float x,float y,float z,float w,float h,u32 c){Command *p=cmd(4,c);p->v[0]=x;p->v[1]=y;p->v[2]=w;p->v[3]=h;}
const Aircraft aircraft[2]={{.name="SR22"},{.name="SF50"}};
'''
fixture=r'''
int main(void){white=C2D_Color32(245,245,240,255);black=C2D_Color32(12,18,25,255);cyan=C2D_Color32(62,221,240,255);yellow=C2D_Color32(255,224,54,255);
 AppMenu m={.screen=MENU_TITLE};splashScreen(&m);
 screen=1;drawAppMenu(&m,"");
 screen=2;m.screen=MENU_SETUP;m.phase=PHASE_GLIDE;m.airport=2;m.aircraft=1;drawAppMenu(&m,"");
 screen=3;m.screen=MENU_PAUSE;m.hasSave=1;m.row=1;drawAppMenu(&m,"Flight saved. Ready to resume.");
 screen=4;m.screen=MENU_EXIT;m.row=0;drawAppMenu(&m,"");
 screen=5;m.screen=MENU_AUDIO;m.row=0;m.volume=3;drawAppMenu(&m,"");
 return 0;
}
'''
src=out/'drawing.c';src.write_text(shim+names+drawing+fixture);elf=out/'drawing.elf'
subprocess.run(['C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe','-O2','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(r/'include'),str(src),str(r/'source/app_menu.c'),'-Wl,-e,main','-lm','-o',str(elf)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x2000000)
with elf.open('rb') as f:
 e=ELFFile(f)
 for seg in e.iter_segments():
  if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x1f00000);u.reg_write(UC_ARM_REG_LR,0x1ff0000)
u.emu_start(sy['main'],0x1ff0000,count=10000000);assert u.reg_read(UC_ARM_REG_PC)==0x1ff0000
counts=struct.unpack('<8i',u.mem_read(sy['counts'],32));images=[];overflow=[]
for screen in range(6):
 assert counts[screen]<256
 im=Image.new('RGB',(400 if screen==0 else 320,240));d=ImageDraw.Draw(im)
 for i in range(counts[screen]):
  data=bytes(u.mem_read(sy['commands']+(screen*256+i)*200,200));kind,*v,c=struct.unpack('<i8fI',data[:40]);color=(c&255,(c>>8)&255,(c>>16)&255)
  if kind==0:d.rectangle((v[0],v[1],v[0]+v[2]-1,v[1]+v[3]-1),fill=color)
  elif kind==1:d.line(v[:4],fill=color,width=max(1,round(v[4])))
  elif kind==2:
   text=data[40:].split(b'\0')[0].decode();font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',max(1,round(30*v[2])))
   if v[0]+d.textlength(text,font=font)>im.width:overflow.append([screen,text])
   d.text((v[0],v[1]),text,font=font,fill=color,anchor='lt')
  elif kind==3:d.polygon([(v[j],v[j+1]) for j in range(0,6,2)],fill=color)
  elif kind==4:d.ellipse((v[0],v[1],v[0]+v[2],v[1]+v[3]),fill=color)
 images.append(im)
art=r/'art';art.mkdir(exist_ok=True)
images[0].save(art/'3dmicroflight_splash.png')
sheet=Image.new('RGB',(1080,560),(5,10,18));sd=ImageDraw.Draw(sheet)
for i,im in enumerate(images):
 x=(i%3)*360+(0 if i else 0);y=(i//3)*280+24
 if i==0:im=im.resize((360,216))
 sheet.paste(im,(x,y));sd.text((x+8,y-17),['Top screen','Title menu','Flight setup','Pause / save','Exit confirmation','Audio settings'][i],fill='white')
sheet.save(art/'3dmicroflight_menus.png')
report={'method':__doc__,'commands_per_screen':counts[:6],'text_right_edge_overflow':overflow};(r/'validation/menu_layout_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert not overflow
