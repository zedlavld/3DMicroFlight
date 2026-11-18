#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "flight.h"
#include "camera.h"
#include "aircraft_render.h"
#include "terrain.h"
#define PI 3.14159265359f
static C2D_TextBuf texts;
static u32 white,black,cyan,yellow;
static void rect(float x,float y,float w,float h,u32 c){C2D_DrawRectSolid(x,y,0,w,h,c);}
static void line(float x,float y,float a,float b,float w,u32 c){C2D_DrawLine(x,y,c,a,b,c,w,0);}
static void label(float x,float y,float scale,u32 c,const char *fmt,...){
 char str[160]; va_list ap;va_start(ap,fmt);vsnprintf(str,sizeof(str),fmt,ap);va_end(ap);
 C2D_Text t;C2D_TextParse(&t,texts,str);C2D_TextOptimize(&t);C2D_DrawText(&t,C2D_WithColor,x,y,0,scale,scale,c);
}
typedef struct {Vec v[3];float depth;u32 color;} Tri;
static Tri triangles[6000];static int count;
static View view;
static bool texturedReady;
static float aircraftNotice;
static CameraPose activeCamera;
static void submit(Vec a,Vec b,Vec c,u32 color){
 if(count>=6000)return;
 triangles[count++]=(Tri){{a,b,c},(a.z+b.z+c.z)/3,color};
}
static int farFirst(const void *a,const void *b){float d=((const Tri*)b)->depth-((const Tri*)a)->depth;return d>0?1:(d<0?-1:0);}
/* Clip in camera space before perspective division, including screen edges.
   The short near plane allows a real-size aircraft in exterior view. */
static float clipDistance(Vec p,int plane){
 switch(plane){
  case 0:return p.z-.5f;
  case 1:return p.x+p.z*(200.f/220);
  case 2:return p.z*(200.f/220)-p.x;
  case 3:return p.y+p.z*(120.f/220);
  default:return p.z*(120.f/220)-p.y;
 }
}
static void raster(const Tri *t){
 Vec poly[16],out[16];int n=3;
 for(int i=0;i<3;i++)poly[i]=t->v[i];
 for(int plane=0;plane<5&&n;plane++){
  int m=0;
  for(int i=0;i<n;i++){
   Vec a=poly[i],b=poly[(i+1)%n];float da=clipDistance(a,plane),db=clipDistance(b,plane);
   if(da>=0)out[m++]=a;
   if((da>=0)!=(db>=0)){
    float f=da/(da-db);out[m++]=(Vec){a.x+f*(b.x-a.x),a.y+f*(b.y-a.y),a.z+f*(b.z-a.z)};
   }
  }
  n=m;for(int i=0;i<n;i++)poly[i]=out[i];
 }
 for(int i=1;i<n-1;i++){
  Vec a=poly[0],b=poly[i],c=poly[i+1];
  C2D_DrawTriangle(200+220*a.x/a.z,120-220*a.y/a.z,t->color,
   200+220*b.x/b.z,120-220*b.y/b.z,t->color,
   200+220*c.x/c.z,120-220*c.y/c.z,t->color,0);
 }
}
static void worldTri(const Flight *s,Vec a,Vec b,Vec c,u32 color){
 (void)s;submit(projectCamera(&activeCamera,a),projectCamera(&activeCamera,b),projectCamera(&activeCamera,c),color);
}
static Vec aircraftPoint(const Flight *s,Vec local){
 Vec p=bodyWorld(s,local);return (Vec){p.x+s->pos.x,p.y+s->pos.y,p.z+s->pos.z};
}
static void aircraftTri(const Flight *s,Vec a,Vec b,Vec c,u32 color){
 worldTri(s,aircraftPoint(s,a),aircraftPoint(s,b),aircraftPoint(s,c),color);
}
static void aircraftModel(const Flight *s){
 /* Original placeholder silhouettes, replaced by detailed assets in a later pass. */
 float half=s->aircraft?5.3f:4.0f,span=aircraft[s->aircraft].span*.5f;
 Vec nose={half,0,0},tail={-half,0,0};
 Vec ring[4]={{.5f,0,-.85f},{.5f,.65f,0},{.5f,0,.7f},{.5f,-.65f,0}};
 u32 body=C2D_Color32(225,232,238,255),shade=C2D_Color32(153,170,189,255),accent=C2D_Color32(38,93,158,255);
 for(int i=0;i<4;i++){
  aircraftTri(s,nose,ring[i],ring[(i+1)%4],i<2?body:shade);
  aircraftTri(s,tail,ring[(i+1)%4],ring[i],i<2?body:shade);
 }
 for(int sign=-1;sign<=1;sign+=2){
  aircraftTri(s,(Vec){1,.5f*sign,.25f},(Vec){-.7f,span*sign,.1f},(Vec){-1.5f,span*sign,.1f},body);
  aircraftTri(s,(Vec){1,.5f*sign,.25f},(Vec){-1.5f,span*sign,.1f},(Vec){-1.2f,.5f*sign,.25f},accent);
  float tipZ=s->aircraft?-1.8f:0;
  aircraftTri(s,(Vec){-half+1,0,0},(Vec){-half+.4f,2.1f*sign,tipZ},(Vec){-half-.3f,2.1f*sign,tipZ},body);
  aircraftTri(s,(Vec){-half+1,0,0},(Vec){-half-.3f,2.1f*sign,tipZ},tail,accent);
 }
 if(s->aircraft){
  /* Upper aft engine pod on the SF50; V-tail above. */
  aircraftTri(s,(Vec){-1.1f,-.5f,-1.1f},(Vec){-3.8f,0,-1.65f},(Vec){-1.1f,.5f,-1.1f},shade);
  aircraftTri(s,(Vec){-1.1f,-.5f,-1.1f},(Vec){-3.8f,0,-.8f},(Vec){-3.8f,0,-1.65f},accent);
  aircraftTri(s,(Vec){-1.1f,.5f,-1.1f},(Vec){-3.8f,0,-1.65f},(Vec){-3.8f,0,-.8f},body);
 }else{
  aircraftTri(s,tail,(Vec){-half+1.4f,0,0},(Vec){-half+.2f,0,-1.7f},accent);
  aircraftTri(s,(Vec){half+.1f,-1.4f,0},(Vec){half+.1f,1.4f,0},(Vec){half+.1f,0,-.12f},shade);
 }
}
static void runway(const Flight *s,const Airport *ap){
 float h=ap->heading*PI/180,dx=sinf(h)*ap->length/2,dy=cosf(h)*ap->length/2,rx=cosf(h)*ap->width/2,ry=-sinf(h)*ap->width/2,z=ap->elevation+4;
 Vec a={ap->east-dx-rx,ap->north-dy-ry,z},b={ap->east+dx-rx,ap->north+dy-ry,z},c={ap->east+dx+rx,ap->north+dy+ry,z},d={ap->east-dx+rx,ap->north-dy+ry,z};
 u32 asphalt=C2D_Color32(48,49,52,255);worldTri(s,a,b,c,asphalt);worldTri(s,a,c,d,asphalt);
 for(int j=-5;j<=5;j++){
  float offset=j*ap->length/13,ex=ap->east+sinf(h)*offset,ny=ap->north+cosf(h)*offset;
  float lx=sinf(h)*22,ly=cosf(h)*22,wx=cosf(h)*1.2f,wy=-sinf(h)*1.2f;
  a=(Vec){ex-lx-wx,ny-ly-wy,z+1};b=(Vec){ex+lx-wx,ny+ly-wy,z+1};c=(Vec){ex+lx+wx,ny+ly+wy,z+1};d=(Vec){ex-lx+wx,ny-ly+wy,z+1};
  worldTri(s,a,b,c,white);worldTri(s,a,c,d,white);
 }
}
static void scenery(const Flight *s){
 activeCamera=makeCamera(s,&view);
 if(texturedReady)drawWorldScenery(s,&activeCamera);
 else {count=0;for(int i=0;i<3;i++)runway(s,&airports[i]);if(view.exterior)aircraftModel(s);
 qsort(triangles,count,sizeof(Tri),farFirst);for(int i=0;i<count;i++)raster(&triangles[i]);}
 if(view.exterior&&texturedReady)drawTexturedAircraft(s,&activeCamera);
 rect(0,0,400,21,black);label(5,2,.42f,white,"CANYON FLIGHT   %s   %s",aircraft[s->aircraft].name,airports[s->airport].id);
 if(s->airport==0&&texturedReady)label(6,22,.30f,hasGroundImagery()?white:yellow,hasGroundImagery()?"SEDONA NAIP | USGS / USDA":"SEDONA PHOTO PACK NOT LOADED");
 if(!texturedReady)label(6,180,.4f,yellow,"TERRAIN RENDERER UNAVAILABLE");
 if(!terrainContains(s->pos.x,s->pos.y))label(6,197,.4f,yellow,"OUTSIDE INCLUDED TERRAIN");
 if(view.exterior)label(300,2,.35f,cyan,"%.0f m",view.distance);
 label(6,218,.34f,white,"X:swap | ZR:view | ZL+D-pad:zoom");
 if(aircraftNotice>0){rect(4,40,340,23,black);label(8,42,.36f,cyan,"%s selected - flight position retained",aircraft[s->aircraft].name);}
 if(s->crashed){rect(55,90,290,55,black);label(75,99,.65f,yellow,"TERRAIN CONTACT");label(80,123,.4f,white,"SELECT to restart approach");}
}
static void attitude(const Flight *s){
 rect(0,0,320,240,C2D_Color32(35,108,176,255));
 float cr=cosf(s->roll),sr=sinf(s->roll),h=s->pitch*130;
 float ax=160-600*cr+(h)*sr,ay=98+600*sr+h*cr;
 float bx=160+600*cr+(h)*sr,by=98-600*sr+h*cr;
 u32 ground=C2D_Color32(127,77,42,255);
 C2D_DrawTriangle(ax,ay,ground,bx,by,ground,bx+600*sr,by+600*cr,ground,0);
 C2D_DrawTriangle(ax,ay,ground,bx+600*sr,by+600*cr,ground,ax+600*sr,ay+600*cr,ground,0);
 line(ax,ay,bx,by,2,white);
 for(int deg=-30;deg<=30;deg+=10){if(!deg)continue;float yy=h-deg*PI/180*130;
  line(160-16*cr+yy*sr,98+16*sr+yy*cr,160+16*cr+yy*sr,98-16*sr+yy*cr,1,white);
 }
 line(129,98,151,98,3,yellow);line(169,98,191,98,3,yellow);line(160,95,160,103,3,yellow);
 rect(0,0,320,22,black);rect(0,22,66,147,black);rect(254,22,66,147,black);rect(0,169,320,71,black);
 
 label(6,3,.43f,cyan,"%s  EXPERIMENTAL PFD",aircraft[s->aircraft].name);
 label(5,29,.38f,white,"IAS KT");label(4,67,.85f,white,"%.0f",s->ias);
 label(5,116,.36f,white,"FLAP %.0f",s->flapActual*100);label(5,140,.36f,white,"TRM %.0f",s->trim*100);
 label(260,29,.38f,white,"ALT FT");label(255,67,.68f,white,"%.0f",s->pos.z*3.28084f);
 label(258,112,.33f,white,"V/S FPM");label(258,129,.45f,white,"%+.0f",s->vel.z*196.85f);
 label(258,151,.32f,cyan,"29.92 IN");
 label(107,147,.38f,s->stallWarning>.5f?yellow:white,"%+.1f G %s",s->loadFactor,s->stallWarning>.5f?"STALL":"");
 label(6,174,.4f,white,"HDG %03.0f",s->yaw*180/PI);
 const Airport *ap=&airports[s->airport];float e=ap->east-s->pos.x,n=ap->north-s->pos.y;
 float bearing=fmodf(atan2f(e,n)*180/PI+360,360);
 label(127,174,.38f,cyan,"%s %03.0f  %.1f NM",ap->id,bearing,hypotf(e,n)/1852);
 label(6,195,.37f,white,"PWR %3.0f%%   %s   FPS",s->engine*100,s->gear?"GEAR DOWN":"GEAR UP");
 rect(3,213,99,25,C2D_Color32(24,48,65,255));
 label(7,220,.34f,cyan,"X: TO %s",aircraft[1-s->aircraft].name);label(111,220,.36f,cyan,"AIRPORT");label(219,220,.36f,cyan,"FLAPS/GEAR");
}
int main(void){
 gfxInitDefault();gfxSet3D(true);osSetSpeedupEnable(true);
 if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE*2)){gfxExit();return 1;}
 if(!C2D_Init(16000)){C3D_Fini();gfxExit();return 1;}C2D_Prepare();
 texts=C2D_TextBufNew(8192);if(!texts){C2D_Fini();C3D_Fini();gfxExit();return 1;}
 C3D_RenderTarget *top=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8),*bottom=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
 C3D_RenderTargetSetOutput(top,GFX_TOP,GFX_LEFT,
  GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
 C3D_RenderTarget *right=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
 if(!top||!right||!bottom){C2D_TextBufDelete(texts);C2D_Fini();C3D_Fini();gfxExit();return 1;}
 C3D_RenderTargetSetOutput(right,GFX_TOP,GFX_RIGHT,
  GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
 white=C2D_Color32(245,245,240,255);black=C2D_Color32(12,18,25,255);cyan=C2D_Color32(62,221,240,255);yellow=C2D_Color32(255,224,54,255);
 int extendedInput=R_SUCCEEDED(irrstInit());
 u32 previousExtended=0;view=(View){0};resetView(&view);
 texturedReady=initAircraftRenderer();
 Flight s;resetFlight(&s,0,0);u64 previous=osGetTime();float accumulator=0,fps=60;int paused=0,zlHeldLast=0,zlZoomUsed=0;
 /* Dedicated emulator inspection harness; production source is unchanged. */
 resetFlight(&s,1,0);view.exterior=1;view.distance=14;paused=1;u64 visualStart=osGetTime();
 while(aptMainLoop()){
  hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();
  circlePosition look={0,0};
  if(extendedInput){
   irrstScanInput();u32 extra=irrstKeysHeld();
   down=(down&~(KEY_ZL|KEY_ZR))|(extra&~previousExtended&(KEY_ZL|KEY_ZR));
   held|=extra;previousExtended=extra;
   irrstCstickRead(&look);
  }
  if(down&KEY_START)break;
  u64 now=osGetTime();float elapsed=clampf((now-previous)/1000.f,0,.1f);previous=now;
  aircraftNotice=fmaxf(0,aircraftNotice-elapsed);
  if(elapsed>0)fps=.95f*fps+.05f/elapsed;
  if(down&KEY_SELECT){resetFlight(&s,s.aircraft,s.airport);resetView(&view);}
  if(down&KEY_ZR)view.exterior=!view.exterior;
  int zlHeld=(held&KEY_ZL)!=0;
  if(zlHeld&&!zlHeldLast)zlZoomUsed=0;
  int zoomChord=view.exterior&&zlHeld;
  if(zoomChord&&(held&(KEY_DUP|KEY_DDOWN))){
   zlZoomUsed=1;zoomView(&view,elapsed,(held&KEY_DUP?1.f:0)-(held&KEY_DDOWN?1.f:0));
  }
  if(!zlHeld&&zlHeldLast&&!zlZoomUsed)resetView(&view);
  zlHeldLast=zlHeld;
  moveView(&view,elapsed,look.dx/156.f,look.dy/156.f);
  if(down&KEY_Y)paused=!paused;
  int swapRequested=(down&KEY_X)!=0;
  if(down&KEY_A)s.gear=s.aircraft?!s.gear:1;
  if(down&KEY_B)s.flap=s.flap>.9f?0:s.flap+.5f;
  if(down&KEY_TOUCH){touchPosition t;hidTouchRead(&t);if(t.py>211){
   if(t.px<105)swapRequested=1;
   else if(t.px<211)resetFlight(&s,s.aircraft,(s.airport+1)%3);
   else s.flap=s.flap>.9f?0:s.flap+.5f;
  }}
  if(swapRequested){switchAircraft(&s,1-s.aircraft);accumulator=0;elapsed=0;aircraftNotice=2.5f;}
  circlePosition cp;hidCircleRead(&cp);
  if(!paused){
   s.throttle=clampf(s.throttle+(zoomChord?0:elapsed*.3f)*((held&KEY_DUP?1:0)-(held&KEY_DDOWN?1:0)),0,1);
   s.trim=clampf(s.trim+elapsed*.12f*((held&KEY_DRIGHT?1:0)-(held&KEY_DLEFT?1:0)),-.3f,.3f);
   accumulator+=elapsed;
   while(accumulator>=1/120.f){stepFlight(&s,1/120.f,-cp.dy/156.f,cp.dx/156.f,(held&KEY_R?1.f:0)-(held&KEY_L?1.f:0));accumulator-=1/120.f;}
  }else accumulator=0;
  int visualPose=1;
  switchAircraft(&s,(int)((now-visualStart)/15000)%2);
  const float visualYaw[6]={.8f,1.5708f,3.14159f,.8f,0,.8f};
  const float visualPitch[6]={.24f,.12f,.1f,-.6f,.2f,1.f};
  view.exterior=1;view.distance=14;view.orbitYaw=visualYaw[visualPose%6];view.orbitPitch=visualPitch[visualPose%6];
  paused=1;s.flapActual=s.flap=visualPose<6?0:1;s.gearActual=s.gear=visualPose<6?1:0;
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);C2D_TextBufClear(texts);
  beginRendererFrame();
  float depth=osGet3DSliderState(),convergence=view.exterior?view.distance:120.f;
  /* Conservative maximum far-field disparity (~3.3 pixels at full slider). */
  float iod=depth*convergence*.015f;
  for(int eye=0;eye<(depth>0?2:1);eye++){
  setRendererEye(depth>0?(eye?iod:-iod):0,convergence);
  C3D_RenderTarget *target=eye?right:top;
  C3D_RenderTargetClear(target,C3D_CLEAR_ALL,0x7DB1CFFF,0);C2D_SceneBegin(target);
  C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);scenery(&s);
  if(paused){label(160,35,.6f,yellow,"PAUSED");label(7,55,.36f,white,"CPU %.1f ms  GPU %.1f ms",C3D_GetProcessingTime(),C3D_GetDrawingTime());}
  label(7,78,.32f,cyan,"VISUAL TEST %d/12 - AUTO CAMERA",visualPose+1);
  if(!extendedInput)label(6,197,.4f,yellow,"C-STICK / ZL / ZR UNAVAILABLE");
  }
  C2D_TargetClear(bottom,black);C2D_SceneBegin(bottom);attitude(&s);label(270,195,.37f,white,"%.0f",fps);
  C3D_FrameEnd(0);
 }
 if(extendedInput)irrstExit();
 freeAircraftRenderer();
 C2D_TextBufDelete(texts);C2D_Fini();C3D_Fini();gfxExit();return 0;
}

