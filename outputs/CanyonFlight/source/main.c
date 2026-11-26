#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "flight.h"
#include "flight_recorder.h"
#include "app_menu.h"
#include "flight_save.h"
#include "cockpit.h"
#include "flight_audio.h"
#include "frame_profile.h"
#include <sys/stat.h>
#include <time.h>
#include "camera.h"
#include "aircraft_render.h"
#include "scenery_stream.h"
#include "terrain.h"
#include "terrain_stream.h"
#include "autotest.h"
#include "autopilot.h"
#include "map_render.h"
#include "maneuver.h"
#include "fbw.h"
#include "pixel_font.h"
#include "fbw.h"
#include "f16_aero.h"
#include <malloc.h>
/* v31.13: linear heap 48 MB (libctru default 32 MB). After the renderer only
   18 MB of linear memory was free, so the scenery stream could not hold two
   windows of photo tiles and fragmented the heap trying. The main heap uses
   about 8 MB.
   v31.15: the split is chosen at start-up (linked with
   --wrap=__system_allocateHeaps). New 3DS (124 MB mode, ~114 MB free):
   linear 48 MB, the rest main heap, as v31.13. Original 3DS (64 MB mode,
   ~54 MB free after the program): a fixed 48 MB would leave the main heap
   about 6 MB, short of the 8 MB used, so the main heap keeps 14 MB and
   linear gets the rest (the tile pool sizes itself to what is free). */
u32 __ctru_linear_heap_size=48u*1024u*1024u;
extern u32 __ctru_heap_size;
void __real___system_allocateHeaps(void);
void __wrap___system_allocateHeaps(void){
 Handle limit=0;s64 maxCommit=0,currentCommit=0;ResourceLimitType type=RESLIMIT_COMMIT;
 if(R_SUCCEEDED(svcGetResourceLimit(&limit,CUR_PROCESS_HANDLE))){
  svcGetResourceLimitLimitValues(&maxCommit,limit,&type,1);svcGetResourceLimitCurrentValues(&currentCommit,limit,&type,1);svcCloseHandle(limit);
  u32 remaining=(u32)(maxCommit-currentCommit)&~0xFFFu;const u32 mb=1024u*1024u;
  if(remaining>=62u*mb){
   u32 heap=remaining-48u*mb<14u*mb?14u*mb:remaining-48u*mb;
   __ctru_heap_size=heap;__ctru_linear_heap_size=remaining-heap;
  }else{__ctru_heap_size=0;__ctru_linear_heap_size=0;} /* libctru's own split */
 }
 __real___system_allocateHeaps();
}
#include "instruments.h"
#include "scenery.h"
#include "weather_visual.h"
#include "daylight.h"
#include "airport_index.h"
#define PI 3.14159265359f
static C2D_TextBuf texts;
static bool boldLowerText;
static u32 white,black,cyan,yellow;
static int audioReady;
static int profileScreen=-1;
static FrameProfile frameProfile;static FrameStats frameStats;
static char performanceNotice[96];
/* Landing gear: X, A or the lower-screen GEAR button. Retraction stays
   inhibited on the ground; the SR22 has fixed gear. Refusals are explained
   briefly on the upper screen instead of being ignored silently. */
/* v33.01: B / FLAPS step through the aircraft's detents. */
static void nextFlap(Flight *s){float step=1.f/aircraft[s->aircraft].flapSteps;s->flap=s->flap>1-step*.5f?0:fminf(1,s->flap+step);}
static float gearNotice;static const char *gearNoticeText="";
static void requestGear(Flight *s){
 if(!aircraft[s->aircraft].retractableGear){static char text[48];snprintf(text,sizeof(text),"%s has fixed landing gear",aircraft[s->aircraft].name);gearNoticeText=text;gearNotice=2;return;}
 if(s->onGround){gearNoticeText="Gear stays down on the ground";gearNotice=2;return;}
 s->gear=!s->gear;gearNotice=0;
}
/* Hardware sky-flicker bisection (v31 test build). Tap the lower screen's
   title strip to cycle; each mode removes one pass so a New 3DS session can
   show which pass produces the stray streaks. Not saved; resets to NORMAL. */
/* v31.3 adds three hardware hole/streak bisection modes: magenta clear colour
   (holes vs. drawn blue), photo textures off, and pre-v31.2 strip ranges. */
/* v31.7 adds magenta variants: world drawn twice, and no per-range culling. */
enum {RENDER_NORMAL,RENDER_NO_WORLD,RENDER_NO_MODEL,RENDER_NO_SKY,RENDER_MAGENTA_CLEAR,RENDER_NO_PHOTOS,RENDER_STRIP_RANGES,RENDER_MAGENTA_TWICE,RENDER_MAGENTA_NO_CULL,RENDER_MAGENTA_DEPTH_ALWAYS,RENDER_MAGENTA_NO_GUARD,RENDER_MAGENTA_ROTATED,RENDER_MAGENTA_ALIGNED,RENDER_MAGENTA_SMALL_DRAWS,RENDER_MAGENTA_ARRAYS,RENDER_TEST_COUNT};
#define MAGENTA_MODE(m) ((m)==RENDER_MAGENTA_CLEAR||((m)>=RENDER_MAGENTA_TWICE&&(m)<=RENDER_MAGENTA_ARRAYS))
static int renderTest;
static const char *renderTestNames[RENDER_TEST_COUNT]={"NORMAL","NO TERRAIN/RUNWAYS","NO COCKPIT/AIRCRAFT","NO 2D SKY","MAGENTA CLEAR, NO SKY","NO PHOTO TEXTURES","STRIP RANGES (v31.1)","MAGENTA, WORLD DRAWN TWICE","MAGENTA, NO RANGE CULLING","MAGENTA, DEPTH TEST OFF","MAGENTA, NO SHADER GUARD","MAGENTA, ROTATED TRIANGLES","MAGENTA, 64-BYTE ALIGNED","MAGENTA, 8-TRIANGLE DRAWS","MAGENTA, OLD ARRAY DRAWS"};
static void rect(float x,float y,float w,float h,u32 c){C2D_DrawRectSolid(x,y,0,w,h,c);}
static void line(float x,float y,float a,float b,float w,u32 c){C2D_DrawLine(x,y,c,a,b,c,w,0);}
/* v31.18: text at scale 0.36 and below uses the pixel font (sharp 1:1
   glyphs) when it has every character; larger text keeps the system font. */
#define PIXEL_FONT_MAX_SCALE .36f
#define PIXEL_FONT_Y 2
static int usePixelFont(float scale,const char *s){return scale<=PIXEL_FONT_MAX_SCALE&&pixelFontReady()&&pixelFontSupports(s);}
/* v35.05: during an autotest, the running build, case number and time since the run started
   (drawn on the top screen, so it is in every screenshot). Empty outside tests. */
static char autotestInfo[96];
static void label(float x,float y,float scale,u32 c,const char *fmt,...){
 char str[160]; va_list ap;va_start(ap,fmt);vsnprintf(str,sizeof(str),fmt,ap);va_end(ap);
 if(usePixelFont(scale,str)){pixelFontDraw(roundf(x),roundf(y)+PIXEL_FONT_Y,c,str);return;}
 /* v31.16: whole-pixel positions. Fractional ones resampled the scaled-down
    system font differently in every label (soft, broken strokes). */
 x=roundf(x);y=roundf(y);
 C2D_Text t;C2D_TextParse(&t,texts,str);C2D_TextOptimize(&t);C2D_DrawText(&t,C2D_WithColor,x,y,0,scale,scale,c);
 /* Lower-screen emboldening: the same glyphs one whole pixel right (a 0.6 px
    offset only blurred them). */
 if(boldLowerText&&scale>=0.0f)C2D_DrawText(&t,C2D_WithColor,x+1,y,0,scale,scale,c);
}
static float textWidth(const char *str,float scale){if(usePixelFont(scale,str))return pixelFontWidth(str);C2D_Text t;C2D_TextParse(&t,texts,str);float w=0,h=0;C2D_TextGetDimensions(&t,scale,scale,&w,&h);return w;}
/* Centred in [x,x+w], shrunk only if the text would not fit. */
/* Right-aligned so the text ends at x. */
static void labelRight(float x,float y,float scale,u32 c,const char *fmt,...){
 char str[160];va_list ap;va_start(ap,fmt);vsnprintf(str,sizeof(str),fmt,ap);va_end(ap);
 label(x-textWidth(str,scale),y,scale,c,"%s",str);
}
static void labelFit(float x,float y,float w,float scale,u32 c,const char *str){
 float width=textWidth(str,scale);if(width>w){scale*=w/width;width=textWidth(str,scale);}
 label(x+(w-width)/2,y,scale,c,"%s",str);
}
typedef struct {Vec v[3];float depth;u32 color;} Tri;
static Tri triangles[6000];static int count;
static CockpitFace cockpitFaces[COCKPIT_CAPACITY];
static float nearClip=.5f,cockpitEye,cockpitConvergence=2;
static int cstickPeakX,cstickPeakY; /* v35.12: C-stick readings during an autotest case */
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
  case 0:return p.z-nearClip;
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
 int jet=aircraft[s->aircraft].engine==ENGINE_TURBOFAN;float half=jet?5.3f:4.0f,span=aircraft[s->aircraft].span*.5f;
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
  float tipZ=jet?-1.8f:0;
  aircraftTri(s,(Vec){-half+1,0,0},(Vec){-half+.4f,2.1f*sign,tipZ},(Vec){-half-.3f,2.1f*sign,tipZ},body);
  aircraftTri(s,(Vec){-half+1,0,0},(Vec){-half-.3f,2.1f*sign,tipZ},tail,accent);
 }
 if(jet){
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
 float h=ap->heading*PI/180,dx=sinf(h)*ap->length/2,dy=cosf(h)*ap->length/2,rx=cosf(h)*ap->width/2,ry=-sinf(h)*ap->width/2;
 float low=runwayElevation(ap,-ap->length/2)+.025f,high=runwayElevation(ap,ap->length/2)+.025f;
 Vec a={ap->east-dx-rx,ap->north-dy-ry,low},b={ap->east+dx-rx,ap->north+dy-ry,high},c={ap->east+dx+rx,ap->north+dy+ry,high},d={ap->east-dx+rx,ap->north-dy+ry,low};
 u32 asphalt=C2D_Color32(48,49,52,255);worldTri(s,a,b,c,asphalt);worldTri(s,a,c,d,asphalt);
 for(int j=-5;j<=5;j++){
  float offset=j*ap->length/13,ex=ap->east+sinf(h)*offset,ny=ap->north+cosf(h)*offset;
  float lx=sinf(h)*22,ly=cosf(h)*22,wx=cosf(h)*1.2f,wy=-sinf(h)*1.2f;
  low=runwayElevation(ap,offset-22)+.06f;high=runwayElevation(ap,offset+22)+.06f;
  a=(Vec){ex-lx-wx,ny-ly-wy,low};b=(Vec){ex+lx-wx,ny+ly-wy,high};c=(Vec){ex+lx+wx,ny+ly+wy,high};d=(Vec){ex-lx+wx,ny-ly+wy,low};
  worldTri(s,a,b,c,white);worldTri(s,a,c,d,white);
 }
}
/* v34.03c: the game was renamed from CirrusFlight. On first start, copy
   controls and saved flights from sdmc:/3ds/CirrusFlight when the new folder
   has none; the old folder is left in place. */
static void copyIfMissing(const char *name){
 char from[96],to[96];snprintf(from,sizeof(from),"sdmc:/3ds/CirrusFlight/%s",name);snprintf(to,sizeof(to),"sdmc:/3ds/3DMicroFlight/%s",name);
 FILE *t=fopen(to,"rb");if(t){fclose(t);return;}
 FILE *f=fopen(from,"rb");if(!f)return;
 t=fopen(to,"wb");if(!t){fclose(f);return;}
 char buf[1024];size_t n;int ok=1;
 while((n=fread(buf,1,sizeof(buf),f))>0)if(fwrite(buf,1,n,t)!=n){ok=0;break;}
 fclose(f);if(fclose(t)!=0)ok=0;
 if(!ok)remove(to);
}
static void migrateOldFolder(void){
 static const char *names[]={"controls.cfg","flight.sav","flight2.sav","flight3.sav"};
 for(unsigned i=0;i<sizeof(names)/sizeof(*names);i++)copyIfMissing(names[i]);
}
static const char *savePaths[3]={"sdmc:/3ds/3DMicroFlight/flight.sav","sdmc:/3ds/3DMicroFlight/flight2.sav","sdmc:/3ds/3DMicroFlight/flight3.sav"};
/* v31.1 stick calibration. Hardware circle pads rarely rest at exactly 0;
   an uncorrected offset reads as a steady roll/pitch command (a left roll
   and dive on the player's New 3DS XL). Centre is measured with the stick
   released; a deadzone absorbs residual drift and the remaining travel is
   rescaled so full deflection still reaches +/-1. */
#define CONTROLS_PATH "sdmc:/3ds/3DMicroFlight/controls.cfg"
#define STICK_RANGE 156.f
#define STICK_CAL_LIMIT 40 /* larger offsets mean the stick was held */
static int stickCentreX,stickCentreY;
static float stickAxis(int raw,int centre,int deadzonePercent){
 float v=(raw-centre)/STICK_RANGE,dz=deadzonePercent/100.f,a=fabsf(v);
 if(a<=dz)return 0;
 return clampf((a-dz)/(1-dz),0,1)*(v<0?-1:1);
}
/* v1.1p18 lower screen: PFD + MAP (menu, saved) or autotest cases named *_map; map range */
static int lowerMap,caseMap,caseMapRange=-1,mapRangeIndex=3; /* caseMapRange: *_map0.._map5 cases (5-160 NM) */
static const float mapRangesNm[]={5,10,20,40,80,160};
#define MAP_RANGES 6
static void loadControls(AppMenu *menu){
 menu->deadzone=6;menu->level=FLIGHT_LEVEL_NORMAL;stickCentreX=stickCentreY=0;
 FILE *f=fopen(CONTROLS_PATH,"r");if(!f)return;
 int x,y,dz;
 char line[96]={0};if(!fgets(line,sizeof(line),f)){fclose(f);return;}
 /* v34.03c: renamed from CirrusFlight; files written before still load. */
 if((sscanf(line,"3DMicroFlight controls 1 %d %d %d",&x,&y,&dz)==3||sscanf(line,"CirrusFlight controls 1 %d %d %d",&x,&y,&dz)==3)&&abs(x)<=STICK_CAL_LIMIT&&abs(y)<=STICK_CAL_LIMIT&&dz>=0&&dz<=MENU_DEADZONE_MAX){stickCentreX=x;stickCentreY=y;menu->deadzone=dz;}
 /* v1.1p18: lower screen layout and map range */
 int lm,mr,lv=FLIGHT_LEVEL_NORMAL;
 if(fgets(line,sizeof(line),f)&&sscanf(line,"3DMicroFlight display %*d %d %d %d",&lm,&mr,&lv)>=2){menu->lowerMap=lm!=0;if(mr>=0&&mr<MAP_RANGES)mapRangeIndex=mr;
  if(!strncmp(line,"3DMicroFlight display 2",23)&&lv>=0&&lv<FLIGHT_LEVELS)menu->level=lv;} /* v1.1p21: display 2 adds the difficulty */
 fclose(f);
}
static int saveControls(const AppMenu *menu){
 FILE *f=fopen(CONTROLS_PATH ".tmp","w");if(!f)return 0;
 int ok=fprintf(f,"3DMicroFlight controls 1 %d %d %d\n",stickCentreX,stickCentreY,menu->deadzone)>0;
 if(ok)ok=fprintf(f,"3DMicroFlight display 2 %d %d %d\n",menu->lowerMap,mapRangeIndex,menu->level)>0;
 if(fclose(f)!=0)ok=0;
 if(ok){remove(CONTROLS_PATH);ok=rename(CONTROLS_PATH ".tmp",CONTROLS_PATH)==0;}
 return ok;
}
/* Average the released stick for half a second. */
static void calibrateStick(AppMenu *menu,char *notice,size_t size){
 long sx=0,sy=0;int n=0,lo=999,hi=-999;
 for(int i=0;i<30;i++){
  gspWaitForVBlank();hidScanInput();circlePosition c;hidCircleRead(&c);
  sx+=c.dx;sy+=c.dy;n++;lo=c.dx<lo?c.dx:lo;hi=c.dx>hi?c.dx:hi;
 }
 int x=(int)lroundf((float)sx/n),y=(int)lroundf((float)sy/n);
 if(abs(x)>STICK_CAL_LIMIT||abs(y)>STICK_CAL_LIMIT||hi-lo>24){snprintf(notice,size,"Release the Circle Pad, then calibrate again.");return;}
 stickCentreX=x;stickCentreY=y;
 snprintf(notice,size,saveControls(menu)?"Stick centre set: x %+d  y %+d (saved)":"Stick centre set: x %+d  y %+d (save failed)",x,y);
}
/* v31.4 autotest support (include/autotest.h). The top render target is
   copied to linear memory with the same display transfer the screen output
   uses, then written as a 400x240 24-bit BMP (BGR, bottom-up rows). */
/* v31.19: w = 400 (top screen) or 320 (lower screen). */
static int captureScreen(C3D_RenderTarget *target,const char *path,const int w);
static int captureTopScreen(C3D_RenderTarget *target,const char *path){return captureScreen(target,path,400);}
static int captureScreen(C3D_RenderTarget *target,const char *path,const int w){
 static u8 *pixels;if(!pixels)pixels=linearAlloc(240*400*3);if(!pixels)return 0;
 C3D_SyncDisplayTransfer((u32*)target->frameBuf.colorBuf,GX_BUFFER_DIM(240,w),(u32*)pixels,GX_BUFFER_DIM(240,w),
  GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
 GSPGPU_InvalidateDataCache(pixels,240*400*3);
 FILE *f=fopen(path,"wb");if(!f)return 0;
 const int h=240,size=54+w*h*3;
 u8 header[54]={'B','M'};header[2]=size&255;header[3]=(size>>8)&255;header[4]=(size>>16)&255;header[10]=54;header[14]=40;
 header[18]=w&255;header[19]=w>>8;header[22]=h;header[26]=1;header[28]=24;
 int ok=fwrite(header,1,54,f)==54;
 static u8 row[400*3];
 /* Framebuffer memory is column-major: screen column x holds 240 pixels with
    the bottom of the screen first, so BMP row r (from the bottom) is offset r. */
 for(int r=0;r<h&&ok;r++){
  for(int x=0;x<w;x++){const u8 *p=pixels+(x*240+r)*3;row[x*3]=p[0];row[x*3+1]=p[1];row[x*3+2]=p[2];}
  ok=fwrite(row,1,w*3,f)==(size_t)w*3;
 }
 if(fclose(f)!=0)ok=0;
 return ok;
}
/* v31.15: after a flight starts or jumps, the flight is held (LOADING
   SCENERY) until photo scenery is on screen, at most 20 s. The v31.13 runs
   showed flat ground for the first 8-10 s of every takeoff. */
static float sceneryHoldLeft,sceneryHoldTime;static int holdingScenery;
/* v31.17 player autopilot on the first softkey: OFF -> HOLD (current heading
   and altitude; tap the HSI half or the altitude tape half to change them)
   -> ROUTE (the route autopilot to the nearest other airport; tap the GPS
   line for the next one) -> OFF. On the ground the first tap is ROUTE.
   Moving the stick or the rudder keys disconnects it. */
/* v32.03: the AP softkey opens a universal autopilot panel instead (see
   drawApPanel / apPanelTouch). */
static float apSpeedMax=450;
static ApPanel apPanel;static int apPanelOpen,apPanelSet,apStep[4];static Autopilot apState;static float apNotice;static const char *apNoticeText="";
static void apOff(const char *why){if(apPanel.ap&&why){apNotice=2.5f;apNoticeText=why;}apPanel.ap=0;}
static void athrOff(const char *why){if(apPanel.athr&&why){apNotice=2.5f;apNoticeText=why;}apPanel.athr=0;}
/* Next primary airport after the current destination, ordered by distance
   from the plane; airports within 3 km (the one you are at) are skipped. */
static int apNextDestination(const Flight *s,int after){
 float afterDist=-1;if(after>=0)afterDist=hypotf(airports[after].east-s->pos.x,airports[after].north-s->pos.y);
 int best=-1,first=-1;float bestDist=1e30f,firstDist=1e30f;
 for(int i=0;i<airportCount;i++){if(!airports[i].primary)continue;   /* v1.1p20: once per tap, not per frame */
  float d=hypotf(airports[i].east-s->pos.x,airports[i].north-s->pos.y);if(d<3000)continue;
  if(d<firstDist){firstDist=d;first=i;}
  if(d>afterDist+1&&d<bestDist){bestDist=d;best=i;}
 }
 return best>=0?best:first;
}
/* Selections start from the current flight the first time the panel opens
   in a flight. */
static void apPanelPreset(const Flight *s){
 apPanel.speed=fmaxf(40,roundf(s->ias));apPanel.heading=roundf(s->yaw*180/PI)*PI/180;
 apPanel.altitude=roundf(s->pos.z*3.28084f/100)*100/3.28084f;apPanel.verticalSpeed=0;apPanelSet=1;
}
static void apEngage(Flight *s){
 if(s->onGround){apNotice=2.5f;apNoticeText="AP NOT AVAILABLE ON GROUND";return;}
 if(!apPanelSet)apPanelPreset(s);
 float throttleBias=apState.throttleBias;autopilotPanelBegin(&apState,s);if(apPanel.athr)apState.throttleBias=throttleBias;
 apPanel.ap=1;
}
static void apHold(Flight *s,float heading,float altitude){
 apPanelPreset(s);apPanel.heading=heading;apPanel.altitude=roundf(altitude*3.28084f/100)*100/3.28084f;apPanel.hdg=apPanel.alt=1;apPanel.vs=apPanel.nav=0;apEngage(s);
}
static void beginSceneryHold(void){sceneryHoldLeft=20;sceneryHoldTime=0;memset(&apPanel,0,sizeof(apPanel));apPanel.destination=-1;apPanelSet=0;apPanelOpen=0;apNotice=0;}
/* v31.15 long-frame log: frames over 150 ms with where the time went. Kept
   in memory, written per autotest case or to logs/spikes.csv. */
#define SPIKE_MS 150
#define SPIKE_CAPACITY 64
typedef struct {float t,totalMs,inputMs,physicsMs,waitMs,drawMs,endMs,streamMs,buildMs;int parts,terrainReads,hold,exterior;} Spike;
static Spike spikes[SPIKE_CAPACITY];static int spikeCount,spikeTotal;
/* Per autotest case: summed frame breakdown outside the hold. */
static double caseSum[7],caseStage[4];static int caseFrames;
/* v33.09: scripted manoeuvre, dynamics extremes and a 10 Hz trace per case. */
static Maneuver maneuver;static int maneuverOn;static DynStats dynStats;
#define DYN_CAPACITY 3600
typedef struct {float t,cas,agl,msl,pitch,roll,heading,alpha,beta,nz,p,q,r,elevator,aileron,rudder,pitchIn,rollIn,throttle,engine,flap,gear,stall;signed char stage,prot;} DynSample;
static DynSample dynTrace[DYN_CAPACITY];static int dynCount;static float dynNext,dynClock;
static int protActive(const Flight *s){const char *st=fbwStatus(s);return st&&strcmp(st,"NORMAL LAW")&&strcmp(st,"FLCS")&&strcmp(st,"GROUND LAW");}
static void dynRecord(const Flight *s,float dt,float pitchIn,float rollIn){
 float agl=s->pos.z-terrain(s->pos.x,s->pos.y);dynStatsStep(&dynStats,s,dt,agl,protActive(s));dynClock+=dt;
 if(dynClock<dynNext||dynCount>=DYN_CAPACITY)return;
 dynNext+=.1f;
 dynTrace[dynCount++]=(DynSample){dynClock,s->ias,agl,s->pos.z,s->pitch*57.29578f,s->roll*57.29578f,s->yaw*57.29578f,s->alpha*57.29578f,s->beta*57.29578f,s->loadFactor,s->p*57.29578f,s->q*57.29578f,s->r*57.29578f,
  s->elevator,s->aileron,s->rudder,pitchIn,rollIn,s->throttle,s->engine,s->flapActual,s->gearActual,s->stallWarning,(signed char)(maneuverOn?maneuver.stage:-1),(signed char)protActive(s)};
}
static void saveDynTrace(const char *path){
 FILE *f=fopen(path,"w");if(!f)return;
 fputs("t,cas,agl,msl,pitch,roll,heading,alpha,beta,nz,p,q,r,elevator,aileron,rudder,pitch_in,roll_in,throttle,engine,flap,gear,stall_warn,man_stage,prot\n",f);
 for(int i=0;i<dynCount;i++){const DynSample *d=&dynTrace[i];
  fprintf(f,"%.1f,%.1f,%.0f,%.0f,%.1f,%.1f,%.0f,%.1f,%.1f,%.2f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n",d->t,d->cas,d->agl,d->msl,d->pitch,d->roll,d->heading,d->alpha,d->beta,d->nz,d->p,d->q,d->r,
   d->elevator,d->aileron,d->rudder,d->pitchIn,d->rollIn,d->throttle,d->engine,d->flap,d->gear,d->stall,d->stage,d->prot);}
 fclose(f);
}
static int saveSpikes(const char *path,int append){
 FILE *f=fopen(path,append?"a":"w");if(!f)return 0;
 fseek(f,0,SEEK_END);if(ftell(f)==0)fputs("t,total_ms,input_terrain_ms,physics_ms,gpu_wait_ms,draw_ms,frame_end_ms,stream_ms,build_ms,parts,terrain_sd_reads,hold,exterior\n",f);
 for(int i=0;i<spikeCount;i++){const Spike *p=&spikes[i];
  fprintf(f,"%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%d,%d,%d\n",p->t,p->totalMs,p->inputMs,p->physicsMs,p->waitMs,p->drawMs,p->endMs,p->streamMs,p->buildMs,p->parts,p->terrainReads,p->hold,p->exterior);}
 fclose(f);spikeCount=0;return 1;
}
static float msSince(u64 *mark){u64 now=svcGetSystemTick();float ms=(now-*mark)/(SYSCLOCK_ARM11/1000.f);*mark=now;return ms;}
/* v31.8: cases with a destination are flown by the test autopilot. */
static Autopilot autopilot;static int autopilotOn;
static float wrapPi(float a){while(a>PI)a-=2*PI;while(a<-PI)a+=2*PI;return a;}
/* v35: AT<lat>,<lon> cases: same height above the ground, over that point. The
   aircraft's own elevation tile is read at once so the height is the real one. */
static void autotestPlace(Flight *s,float e,float n){
 float agl=s->pos.z-terrain(s->pos.x,s->pos.y);
 s->pos.x=e;s->pos.y=n;terrainStreamUpdate(s->pos);s->pos.z=terrain(e,n)+fmaxf(agl,300);
}
/* v35.04: with sleep disallowed the screens go dark when the lid closes and nothing turns them
   back on, so the test watches the hinge itself: backlights off when closed, on when opened. */
static int lidServices,lidOpen=1,lidCloses,lidHooked;static float lidClosedSeconds,lidRelight,lidAlive;static u64 lidStart;static aptHookCookie lidHook;
/* v35.06: every lid and APT event is appended to the card at once (open/close per line), so the
   record survives a hard power-off. */
static void lidLog(const char *what){
 FILE *f=fopen("sdmc:/3ds/3DMicroFlight/lid.log","a");if(!f)return;
 u64 ms=osGetTime()-lidStart;fprintf(f,"%llu.%03llu s  %s\n",ms/1000,ms%1000,what);fclose(f);
}
static void lidApt(APT_HookType hook,void *param){(void)param;
 static const char *names[]={"apt suspend","apt restore","apt sleep","apt wakeup","apt exit"};
 if(hook>=0&&hook<5)lidLog(names[hook]);
 if(hook==APTHOOK_ONWAKEUP||hook==APTHOOK_ONRESTORE)lidRelight=1.5f;
}
static void lidWatchStart(void){
 lidServices=R_SUCCEEDED(ptmuInit())?1:0;if(lidServices&&R_FAILED(gspLcdInit())){ptmuExit();lidServices=0;}
 lidOpen=1;lidCloses=0;lidClosedSeconds=0;lidRelight=0;lidAlive=0;lidStart=osGetTime();
 lidLog(lidServices?"--- " AUTOTEST_BUILD " run start, lid watch on":"--- " AUTOTEST_BUILD " run start, lid watch UNAVAILABLE");
 if(!lidHooked){aptHook(&lidHook,lidApt,NULL);lidHooked=1;}
}
static void lidWatch(float dt){
 if(!lidServices)return;
 u8 open=1;if(R_FAILED(PTMU_GetShellState(&open)))return;
 if(!open){lidClosedSeconds+=dt;lidAlive+=dt;if(lidAlive>=10){lidAlive=0;lidLog("still running, lid closed");}}
 if(open&&!lidOpen){lidLog("lid opened, screens on");lidRelight=1.5f;}
 if(!open&&lidOpen){lidCloses++;lidAlive=0;lidLog("lid closed");}
 /* v35.06: the system darkens the screens itself on closing; on opening the backlights are
    switched on repeatedly for 1.5 s, in case the first request lands before the LCDs are back. */
 if(lidRelight>0){lidRelight-=dt;static float every;every-=dt;if(every<=0){every=.25f;GSPLCD_PowerOnAllBacklights();}}
 lidOpen=open!=0;
}
/* v35.15: runs can start again from the menu, so the hook is removed even when the lid services
   were unavailable, and never added twice. */
static void lidWatchStop(void){if(lidHooked){aptUnhook(&lidHook);lidHooked=0;}if(!lidServices)return;lidLog("run end");GSPLCD_PowerOnAllBacklights();gspLcdExit();ptmuExit();lidServices=0;}
static void startAutotestCase(const AutotestCase *c,Flight *s,AppMenu *menu,int *paused){
 resetFlightPhase(s,c->aircraft,c->airport,c->phase);if(c->destination==AUTOTEST_AT_POSITION)autotestPlace(s,c->atEast,c->atNorth);{size_t nl=strlen(c->name);setCityBuildingsEnabled(!(nl>4&&!strcmp(c->name+nl-4,"_nob")));caseMap=nl>4&&!strcmp(c->name+nl-4,"_map");caseMapRange=-1;
 flightLevel=nl>5&&!strcmp(c->name+nl-5,"_easy")?0:nl>4&&!strcmp(c->name+nl-4,"_adv")?FLIGHT_LEVELS-1:FLIGHT_LEVEL_NORMAL;flightEasyRescues=0; /* v1.1p21: Normal unless *_easy / *_adv */
 if(nl>5&&!strncmp(c->name+nl-5,"_map",4)&&c->name[nl-1]>='0'&&c->name[nl-1]<='5'){caseMap=1;caseMapRange=c->name[nl-1]-'0';}}setWeather(s,c->weather);daylightSet(c->hours);{float a,b,d;nightLightFrameStats(&a,&b,&d);}beginSceneryHold();spikeCount=0;memset(caseSum,0,sizeof(caseSum));memset(caseStage,0,sizeof(caseStage));caseFrames=0;{int h,m;sceneryPartCacheStats(&h,&m);}
 autopilotOn=c->destination>=0;if(autopilotOn){autopilotBegin(&autopilot,s,c->destination);autopilotRoute(&autopilot,s,c->cruiseFt/3.28084f,c->routeMax,c->routeLand);}
 maneuverOn=c->destination<=AUTOTEST_MANEUVER_BASE-1;if(maneuverOn)maneuverBegin(&maneuver,AUTOTEST_MANEUVER_BASE-c->destination,s);
 fbwReset();dynStatsBegin(&dynStats,s);dynCount=0;dynNext=0;dynClock=0;
 if(c->destination==AUTOTEST_PLAYER_HOLD||c->destination==AUTOTEST_PLAYER_PANEL)apHold(s,s->yaw+30*PI/180,s->pos.z+500/3.28084f);
 if(c->destination==AUTOTEST_PLAYER_PANEL)apPanelOpen=1;
 if(c->destination==AUTOTEST_PLAYER_CLIMB){
  apPanelPreset(s);apPanel.heading=s->yaw+90*PI/180;apPanel.altitude=(roundf(s->pos.z*3.28084f/100)*100+3000)/3.28084f;apPanel.verticalSpeed=1500/196.85f;
  apPanel.speed=roundf(s->ias)-20;apPanel.hdg=apPanel.vs=apPanel.alt=1;apEngage(s);apState.throttleBias=s->throttle;apPanel.athr=1;
 }
 view.exterior=c->exterior;resetView(&view);renderTest=c->renderTest;
 if(c->exterior)view.orbitYaw=c->lookYaw;else{view.insideYaw=c->lookYaw;view.insidePitch=c->lookPitch;}
 menu->screen=MENU_FLIGHT;*paused=0;memset(&frameProfile,0,sizeof(frameProfile));
}
static int recorderSaveFailed;
static void saveFlightRecording(void){
 if(!flightRecorderCount())return;
 mkdir("sdmc:/3ds/3DMicroFlight/logs",0777);char path[160];
 snprintf(path,sizeof(path),"sdmc:/3ds/3DMicroFlight/logs/flight_%llu.csv",(unsigned long long)osGetTime());
 recorderSaveFailed=!flightRecorderSave(path);
}
static Vec cockpitCameraPoint(const Flight *s,Vec p){
 Vec d=bodyWorld(s,p);Vec v={d.x*activeCamera.right.x+d.y*activeCamera.right.y+d.z*activeCamera.right.z,
 d.x*activeCamera.up.x+d.y*activeCamera.up.y+d.z*activeCamera.up.z,
 d.x*activeCamera.forward.x+d.y*activeCamera.forward.y+d.z*activeCamera.forward.z};
 v.x+=cockpitEye*(v.z/cockpitConvergence-1);return v;
}
static void virtualCockpit(const Flight *s){
 int n=buildCockpit(s,cockpitFaces,COCKPIT_CAPACITY);count=0;
 for(int i=0;i<n;i++)submit(cockpitCameraPoint(s,cockpitFaces[i].v[0]),cockpitCameraPoint(s,cockpitFaces[i].v[1]),cockpitCameraPoint(s,cockpitFaces[i].v[2]),cockpitFaces[i].color);
 qsort(triangles,count,sizeof(Tri),farFirst);nearClip=.07f;
 for(int i=0;i<count;i++)raster(&triangles[i]);
 nearClip=.5f;
}
static void drawSky(const CameraPose *camera,const Flight *s){
 /* Small screen grid with interpolated colors; evaluate world elevation so
    the sky follows the horizon when the cockpit banks or looks sideways. */
 u32 colors[7][5];
 for(int y=0;y<=6;y++)for(int x=0;x<=4;x++){
  float rx=(x*100-200)/220.f,uy=(120-y*40)/220.f;
  float elevation=(camera->forward.z+rx*camera->right.z+uy*camera->up.z)/sqrtf(1+rx*rx+uy*uy),rgb[3];
  daylightSky(elevation,rgb);float storm=weatherStorm(s),brightness=storm>0?fminf(1,1-.35f*storm+weatherFlash(s)*.32f):1;
  colors[y][x]=C2D_Color32((int)(rgb[0]*255*brightness),(int)(rgb[1]*255*brightness),(int)(rgb[2]*255*brightness),255);
 }
 for(int y=0;y<6;y++)for(int x=0;x<4;x++){
  float l=x*100,t=y*40,r=l+100,b=t+40;
  C2D_DrawTriangle(l,t,colors[y][x],r,t,colors[y][x+1],r,b,colors[y+1][x+1],0);
  C2D_DrawTriangle(l,t,colors[y][x],r,b,colors[y+1][x+1],l,b,colors[y+1][x],0);
 }
}
static void scenery(const Flight *s,int showHints){
 activeCamera=makeCamera(s,&view);
 /* MAGENTA CLEAR must also drop the sky: drawSky fills the whole screen, so
    otherwise missing terrain shows the sky gradient, not the clear colour. */
 if(renderTest!=RENDER_NO_SKY&&!MAGENTA_MODE(renderTest))drawSky(&activeCamera,s);
 if(texturedReady&&renderTest!=RENDER_NO_WORLD)drawWorldScenery(s,&activeCamera);
 if(texturedReady&&renderTest!=RENDER_NO_WORLD)drawCityBuildings(s,&activeCamera);
 else {count=0;{static int near[64];int n=airportsNear(s->pos.x,s->pos.y,30000,near,64);for(int i=0;i<n;i++)runway(s,&airports[near[i]]);}if(view.exterior)aircraftModel(s);
 qsort(triangles,count,sizeof(Tri),farFirst);for(int i=0;i<count;i++)raster(&triangles[i]);}
 int drawModel=renderTest!=RENDER_NO_MODEL;
 if(view.exterior&&texturedReady&&drawModel)drawTexturedAircraft(s,&activeCamera);
 if(texturedReady)drawNightLights(s,&activeCamera,view.exterior&&drawModel);
 if(texturedReady)drawWeatherClouds(s,&activeCamera);
 float rain=weatherRain(s);
 if(rain>0){
  if(texturedReady){C2D_Prepare();C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);}
  /* Fixed budget. Cabin geometry masks streaks below the windshield. */
  u32 drop=C2D_Color32(175,196,216,(int)(95*rain));
  for(int i=0;i<36;i++){float x=fmodf(i*97.f+s->weatherTime*43,420)-10,y=fmodf(i*53.f+s->weatherTime*260,270)-15;line(x,y,x-3,y+9,.7f,drop);}
 }
 if(!view.exterior&&drawModel){if(texturedReady)drawVirtualCockpit(s,&activeCamera);else virtualCockpit(s);}
 /* Bind the overlay shader only when we will actually draw with it. Keep
    consecutive world/aircraft/cloud passes on their shared 3D shader. */
 if(texturedReady&&(!view.exterior||rain<=0)){
  C2D_Prepare();C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);
 }
 rect(0,0,400,21,black);label(5,2,.42f,white,"3DMICROFLIGHT   %s   %s",aircraft[s->aircraft].name,airports[s->airport].id);
 if(texturedReady)label(6,22,.30f,hasGroundImagery()?white:yellow,hasGroundImagery()?"NAIP | USGS / USDA":"SCENERY LOADING / PROCEDURAL");
 if(texturedReady)label(239,22,.26f,white,"%s",sceneryStreamStatus());
 if(holdingScenery){rect(118,104,164,30,black);label(130,110,.5f,yellow,"LOADING SCENERY");}
 label(6,33,.27f,white,"TEST: %s%s%s",flightPhaseName(s->startPhase),s->onGround?" | GND":"",s->engineCutoff?" | ENGINE OFF":"");
 if(*autotestInfo)labelRight(394,33,.27f,white,"%s",autotestInfo);
 if(flightRecorderActive()||flightRecorderCount())label(260,33,.27f,cyan,"REC %d%s",flightRecorderCount(),flightRecorderActive()?"":" FULL/STOP");
 if(recorderSaveFailed)label(6,44,.3f,yellow,"LOG SAVE FAILED - ZL+Y RETRIES");
 if(!texturedReady)label(6,180,.4f,yellow,"TERRAIN RENDERER UNAVAILABLE");
 if(!terrainContains(s->pos.x,s->pos.y))label(6,197,.4f,yellow,"OUTSIDE INCLUDED TERRAIN");
 if(view.exterior)label(300,2,.35f,cyan,"%.0f m",view.distance);
 if(renderTest)label(6,46,.32f,yellow,"RENDER TEST %d: %s",renderTest,renderTestNames[renderTest]);
 if(gearNotice>0){rect(4,64,250,19,black);label(8,66,.34f,yellow,"%s",gearNoticeText);}
 if(view.exterior||showHints){label(6,218,.34f,white,"X:gear | ZR:view | ZL+D-pad:zoom");
 label(6,230,.25f,white,"START:menu | ZL+X:performance snapshot");}
 if(performanceNotice[0]&&showHints)label(7,80,.30f,cyan,"%s",performanceNotice);
 if(aircraftNotice>0){rect(4,40,340,23,black);label(8,42,.36f,cyan,"%s selected - flight position retained",aircraft[s->aircraft].name);}
 if(s->crashed){rect(55,90,290,55,black);label(75,99,.65f,yellow,"AIRFRAME / HARD IMPACT");label(80,123,.4f,white,"SELECT to restart phase");}
}
/* Compact original PFD artwork inspired by Garmin instrument organization.
   References and implementation limits: PFD_SCENERY_V14.md. */
#define GEAR_BUTTON_X 218
#define GEAR_BUTTON_Y 198
#define GEAR_BUTTON_W 62
#define GEAR_BUTTON_H 16
static void pfdTriangle(float ax,float ay,float bx,float by,float cx,float cy,u32 color){C2D_DrawTriangle(ax,ay,color,bx,by,color,cx,cy,color,0);}
static void pfdCircle(float x,float y,float radius,u32 color){C2D_DrawEllipseSolid(x-radius,y-radius,0,radius*2,radius*2,color);}
static void drawSoftkeys(void){
 u32 gray=C2D_Color32(59,64,72,255),green=C2D_Color32(60,219,88,255);int on=apPanel.ap||apPanel.athr;
 const char *soft[6]={apPanelOpen?"PFD":"AP","AIRPORT","WEATHER","VIEW","MENU","FLAPS"};
 for(int i=0;i<6;i++){float x=i*320.f/6;rect(x,216,320.f/6-1,24,C2D_Color32(34,39,46,255));line(x,216,x+51,216,1,i||!on?gray:green);labelFit(x+2,222,320.f/6-5,.34f,i||!on?white:green,soft[i]);}
}
/* v32.03 autopilot panel: mode keys with green bars, and SPD / HDG / ALT /
   V/S windows with +/- keys and step selectors. Geometry is shared by the
   drawing and the touch code. */
#define APK_W 48
#define APK_H 38
static const char *apKeyNames[6]={"AP","A/THR","HDG","VS","ALT","NAV"};
static int apKeyOn(int k){return k==0?apPanel.ap:k==1?apPanel.athr:k==2?apPanel.hdg:k==3?apPanel.vs:k==4?apPanel.alt:apPanel.nav;}
static float apKeyX(int k){return 6+k*(APK_W+4);}
/* Windows: 0 SPD, 1 HDG (top row), 2 ALT, 3 V/S. */
static const int apStepCount[4]={3,2,2,1};
static const char *apStepNames[4][3]={{"x1","x10","x100"},{"x1","x10",""},{"x100","x1000",""},{"x100","",""}};
static const float apStepValue[4][3]={{1,10,100},{1,10,0},{100,1000,0},{100,0,0}};
static float apWinX(int w){return w%2?164:6;}
static float apWinY(int w){return w<2?58:136;}
static void apPanelKey(float x,float y,float w,float h,u32 face){
 rect(x,y,w,h,C2D_Color32(28,29,31,255));rect(x+1,y+1,w-2,h-2,face);line(x+1,y+1,x+w-1,y+1,1,C2D_Color32(96,97,100,255));
}
static void drawApPanel(const Flight *s){
 u32 panel=C2D_Color32(70,70,72,255),face=C2D_Color32(52,52,55,255),amber=C2D_Color32(236,186,40,255),green=C2D_Color32(52,226,82,255),dim=C2D_Color32(36,36,38,255);
 rect(0,0,320,216,panel);
 for(int k=0;k<6;k++){float x=apKeyX(k);apPanelKey(x,6,APK_W,APK_H,face);
  rect(x+8,10,APK_W-16,6,apKeyOn(k)?green:dim);
  labelFit(x+2,20,APK_W-4,k==0?.52f:.40f,white,apKeyNames[k]);}
 for(int w=0;w<4;w++){
  float x=apWinX(w),y=apWinY(w);
  for(int i=0;i<apStepCount[w];i++){float bx=x+30+i*34;apPanelKey(bx,y,32,14,face);
   if(apStep[w]==i){rect(bx+6,y+2,20,2,green);}
   labelFit(bx,y+4,32,.32f,white,apStepNames[w][i]);}
  apPanelKey(x,y+16,26,24,face);labelFit(x,y+17,26,.78f,white,"+");
  apPanelKey(x,y+42,26,24,face);labelFit(x,y+43,26,.78f,white,"-");
  rect(x+30,y+16,120,50,C2D_Color32(26,26,27,255));
  static const char *titles[4]={"SPD","HDG","ALT","V/S"};label(x+34,y+18,.36f,amber,"%s",titles[w]);
  char value[16];
  if(w==0)snprintf(value,sizeof(value),"%.0f",apPanel.speed);
  else if(w==1){float deg=roundf(fmodf(apPanel.heading*180/PI+720,360));if(deg>=360)deg-=360;snprintf(value,sizeof(value),"%03.0f",deg);}
  else if(w==2)snprintf(value,sizeof(value),"%.0f",apPanel.altitude*3.28084f);
  else snprintf(value,sizeof(value),"%+.0f",apPanel.verticalSpeed*196.85f);
  label(x+146-textWidth(value,.80f),y+32,.80f,amber,"%s",value);
 }
 label(6,202,.30f,C2D_Color32(170,172,176,255),"%s",apPanel.nav&&apPanel.destination>=0?"NAV: tap the PFD's GPS line for the next airport":"AP key: back to the PFD   stick/rudder: AP off");
 (void)s;
}
static void apAdjust(int w,int dir){
 float step=apStepValue[w][apStep[w]]*dir;
 if(w==0)apPanel.speed=clampf(apPanel.speed+step,40,apSpeedMax); /* v33: up to the aircraft's VMO */
 else if(w==1){float deg=roundf(apPanel.heading*180/PI)+step;deg=fmodf(deg+3600,360);apPanel.heading=deg*PI/180;}
 else if(w==2)apPanel.altitude=clampf(roundf(apPanel.altitude*3.28084f/100)*100+step,0,45000)/3.28084f;
 else apPanel.verticalSpeed=clampf(roundf(apPanel.verticalSpeed*196.85f/100)*100+step,-6000,6000)/196.85f;
}
/* Returns 1 when the touch landed on the panel. */
static int apPanelTouch(Flight *s,int px,int py){
 apSpeedMax=aircraft[s->aircraft].vmo;
 if(py>=216)return 0;
 if(!apPanelSet)apPanelPreset(s);
 for(int k=0;k<6;k++){float x=apKeyX(k);if(px>=x&&px<x+APK_W&&py>=6&&py<6+APK_H){
  if(k==0){if(apPanel.ap)apOff(NULL);else apEngage(s);}
  else if(k==1){if(apPanel.athr)apPanel.athr=0;else{apState.throttleBias=s->throttle;apPanel.athr=1;}}
  else if(k==2){apPanel.hdg=!apPanel.hdg;if(apPanel.hdg)apPanel.nav=0;}
  else if(k==3)apPanel.vs=!apPanel.vs;
  else if(k==4)apPanel.alt=!apPanel.alt;
  else{apPanel.nav=!apPanel.nav;if(apPanel.nav){apPanel.hdg=0;apPanel.destination=apNextDestination(s,-1);if(apPanel.destination<0){apPanel.nav=0;apNotice=2.5f;apNoticeText="NO AIRPORT";}}}
  return 1;}}
 for(int w=0;w<4;w++){float x=apWinX(w),y=apWinY(w);
  for(int i=0;i<apStepCount[w];i++){float bx=x+30+i*34;if(px>=bx&&px<bx+32&&py>=y&&py<y+14){apStep[w]=i;return 1;}}
  if(px>=x&&px<x+26&&py>=y+16&&py<y+40){apAdjust(w,1);return 1;}
  if(px>=x&&px<x+26&&py>=y+42&&py<y+66){apAdjust(w,-1);return 1;}
 }
 return 1; /* the panel covers the PFD: other touches do nothing */
}
/* v1.1p18 lower screen PFD + MAP: track-up moving map on the left (ground colours, runways,
   airport IDs, range ring, magenta line to the NAV destination or the test route), flight data
   on the right, the same NAV strip, status line, gear button and softkeys as the PFD. */
#define MAP_X1 210
#define MAP_Y0 28
#define MAP_Y1 216
#define MAP_AX 105.f
#define MAP_AY 178.f
#define MAP_RANGE_Y 198
#define MAP_RANGE_W 46
static void mapPoint(const Flight *s,float ct,float st,float ppm,float e,float n,float *x,float *y){
 float de=(e-s->pos.x)*ppm,dn=(n-s->pos.y)*ppm;*x=MAP_AX+de*ct-dn*st;*y=MAP_AY-(de*st+dn*ct);
}
static int inMap(float x,float y,float margin){return x>-margin&&x<MAP_X1+margin&&y>MAP_Y0-margin&&y<MAP_Y1+margin;}
static void mapScreen(const Flight *s,const PfdData *d,int paused){
 int ri=caseMapRange>=0?caseMapRange:mapRangeIndex;
 u32 white=C2D_Color32(238,240,242,255),black=C2D_Color32(0,0,0,255),dark=C2D_Color32(22,27,34,255),gray=C2D_Color32(59,64,72,255),
  magenta=C2D_Color32(245,74,240,255),cyan=C2D_Color32(80,220,240,255),green=C2D_Color32(60,219,88,255),yellow=C2D_Color32(250,210,60,255),red=C2D_Color32(240,60,50,255);
 float range=mapRangesNm[ri]*1852,ppm=(MAP_AY-MAP_Y0)/range;
 float trk=d->track*PI/180,ct=cosf(trk),st=sinf(trk);
 mapInit();
 /* the texture reaches the far panel corner (~190 px) with a tenth of its width to spare */
 mapUpdate(s->pos.x,s->pos.y,460/ppm);
 rect(0,MAP_Y0,MAP_X1+1,MAP_Y1-MAP_Y0,C2D_Color32(26,33,46,255));
 {C2D_Image img;float ce,cn,side;
  if(mapImage(&img,&ce,&cn,&side)){float x,y;mapPoint(s,ct,st,ppm,ce,cn,&x,&y);float k=side*ppm/128;C2D_DrawImageAtRotated(img,x,y,0,-trk,NULL,k,k);}}
 /* range ring at half range */
 for(int i=0;i<48;i+=2){float a0=i*PI/24,a1=(i+1)*PI/24,r=(MAP_AY-MAP_Y0)/2;
  float x0=MAP_AX+sinf(a0)*r,y0=MAP_AY-cosf(a0)*r,x1=MAP_AX+sinf(a1)*r,y1=MAP_AY-cosf(a1)*r;
  if(inMap(x0,y0,0)&&inMap(x1,y1,0))line(x0,y0,x1,y1,1,C2D_Color32(170,180,190,160));}
 {char half[12];float hv=mapRangesNm[ri]/2;snprintf(half,sizeof(half),hv<10?"%.1f":"%.0f",hv);label(MAP_AX+3,MAP_AY-(MAP_AY-MAP_Y0)/2-11,.30f,C2D_Color32(190,198,206,255),"%s",half);}
 /* v1.1p19: every other US airport (card file). Small fields show as the range closes:
    class 3 at 20 NM and less, class 2 to 80 NM, large and medium always; labels later still.
    At most 400 airports and 30 labels a frame. */
 {int x0,y0,x1,y1;float nm=mapRangesNm[ri];
  if(mapAirportCells(s->pos.x,s->pos.y,range*1.35f,&x0,&y0,&x1,&y1)){
   int maxCls=nm<=20?3:nm<=80?2:1,labelCls=nm<=10?3:nm<=40?2:nm<=80?1:0,drawn=0,labels=0;const MapRunway *rws=mapRunways();
   u32 paved=C2D_Color32(225,228,232,255),soft=C2D_Color32(176,150,110,255),major=cyan,minor=C2D_Color32(150,196,206,255);
   for(int cy=y0;cy<=y1&&drawn<400;cy++)for(int cx=x0;cx<=x1&&drawn<400;cx++){int n;const MapAirport *ap=mapAirportsInCell(cx,cy,&n);
    for(int k=0;k<n&&drawn<400;k++){const MapAirport *a=&ap[k];if(a->cls>maxCls)continue;
     float x,y;mapPoint(s,ct,st,ppm,a->east,a->north,&x,&y);if(!inMap(x,y,8))continue;drawn++;
     int lines=0;
     for(int r=0;r<a->runways;r++){const MapRunway *w=&rws[a->first+r];float xa,ya,xb,yb;
      mapPoint(s,ct,st,ppm,a->east+w->east1,a->north+w->north1,&xa,&ya);mapPoint(s,ct,st,ppm,a->east+w->east2,a->north+w->north2,&xb,&yb);
      if(hypotf(xb-xa,yb-ya)>=3){line(xa,ya,xb,yb,a->cls<=1?2.5f:1.5f,w->paved?paved:soft);lines++;}}
     if(!lines)pfdCircle(x,y,a->cls<=1?2:1.3f,a->cls<=2?paved:soft);
     if(a->cls<=labelCls&&labels<30&&inMap(x,y,-4)){char id[9];memcpy(id,a->id,8);id[8]=0;label(x+4,y-4,.28f,a->cls<=1?major:minor,"%s",id);labels++;}
    }}
  }}
 /* runways; airport IDs on the start runway record */
 for(int i=0;i<AIRPORT_COUNT;i++){const Airport *a=&airports[i];float x,y;mapPoint(s,ct,st,ppm,a->east,a->north,&x,&y);
  if(!inMap(x,y,30))continue;
  float h=a->heading*PI/180,hl=a->length/2,x0,y0,x1,y1;
  mapPoint(s,ct,st,ppm,a->east-sinf(h)*hl,a->north-cosf(h)*hl,&x0,&y0);mapPoint(s,ct,st,ppm,a->east+sinf(h)*hl,a->north+cosf(h)*hl,&x1,&y1);
  if(hypotf(x1-x0,y1-y0)<3)pfdCircle(x,y,2,white);else line(x0,y0,x1,y1,2.5f,white);
  if(a->primary&&inMap(x,y,-4))label(x+5,y-4,.30f,cyan,"%s",a->id);
 }
 /* route: NAV destination, or the test autopilot's route (final start, then the runway) */
 {float px=MAP_AX,py=MAP_AY;
  if(autopilotOn&&autopilot.destination>=0){const Airport *to=&airports[autopilot.destination];
   if(autopilot.land&&autopilot.stage!=AP_ROLLOUT){float h=autopilot.runwayHeading,fx,fy,ax,ay;
    mapPoint(s,ct,st,ppm,autopilot.aimX-sinf(h)*18000,autopilot.aimY-cosf(h)*18000,&fx,&fy);mapPoint(s,ct,st,ppm,autopilot.aimX,autopilot.aimY,&ax,&ay);
    if(autopilot.stage<AP_APPROACH){line(px,py,fx,fy,2,magenta);px=fx;py=fy;}
    line(px,py,ax,ay,2,magenta);}
   else{float x,y;mapPoint(s,ct,st,ppm,to->east,to->north,&x,&y);line(px,py,x,y,2,magenta);}}
  else if(apPanel.nav&&apPanel.destination>=0){const Airport *to=&airports[apPanel.destination];float x,y;mapPoint(s,ct,st,ppm,to->east,to->north,&x,&y);line(px,py,x,y,2,magenta);}}
 /* aircraft (track up) and a north pointer */
 pfdTriangle(MAP_AX,MAP_AY-10,MAP_AX-8,MAP_AY+8,MAP_AX+8,MAP_AY+8,black);
 pfdTriangle(MAP_AX,MAP_AY-7,MAP_AX-5.5f,MAP_AY+6,MAP_AX+5.5f,MAP_AY+6,white);
 {float nx=-st,ny=-ct,cx=186,cy=50;line(cx-nx*8,cy-ny*8,cx+nx*8,cy+ny*8,1.5f,white);pfdTriangle(cx+nx*10,cy+ny*10,cx+nx*4-ny*3,cy+ny*4+nx*3,cx+nx*4+ny*3,cy+ny*4-nx*3,white);label(cx-3+nx*17,cy-5+ny*17,.30f,white,"N");}
 label(4,MAP_Y0+2,.32f,white,"TRK %03.0f",d->track);
 /* range button: touch cycles 5-160 NM */
 rect(2,MAP_RANGE_Y,MAP_RANGE_W,15,dark);line(2,MAP_RANGE_Y,2+MAP_RANGE_W,MAP_RANGE_Y,1,gray);
 {char rt[12];snprintf(rt,sizeof(rt),"%.0f NM",mapRangesNm[ri]);labelFit(4,MAP_RANGE_Y+2,MAP_RANGE_W-4,.32f,white,rt);}
 /* panels over anything drawn past the map */
 rect(0,0,320,MAP_Y0,dark);rect(MAP_X1+1,MAP_Y0,320-MAP_X1-1,MAP_Y1-MAP_Y0,dark);line(MAP_X1+1,MAP_Y0,MAP_X1+1,MAP_Y1,1,gray);
 /* flight data column */
 {float maximum=pfdAirspeedLimit(s);int stall=s->stallWarning>.5f&&!s->onGround;float x0=MAP_X1+5,x1=316,y=MAP_Y0+3,dy=20.5f;
  label(x0,y+2,.30f,gray==0?white:C2D_Color32(170,178,186,255),"IAS");labelRight(x1,y,.42f,stall||s->ias>maximum?red:white,"%.0f",s->ias);y+=dy;
  label(x0,y+2,.30f,C2D_Color32(170,178,186,255),"ALT");labelRight(x1,y,.42f,cyan,"%.0f",d->altitude);y+=dy;
  label(x0,y+2,.30f,C2D_Color32(170,178,186,255),"V/S");labelRight(x1,y,.42f,white,"%+.0f",d->verticalSpeed);y+=dy;
  label(x0,y+2,.30f,C2D_Color32(170,178,186,255),"HDG");labelRight(x1,y,.42f,white,"%03.0f",d->heading);y+=dy;
  label(x0,y+2,.30f,C2D_Color32(170,178,186,255),"GS");labelRight(x1,y,.42f,white,"%.0f",d->groundSpeed);y+=dy;
  label(x0,y+2,.30f,C2D_Color32(170,178,186,255),"WIND");if(d->windSpeed<1)labelRight(x1,y+1,.36f,white,"CALM");else labelRight(x1,y+1,.36f,white,"%03.0f/%.0f",d->windFrom,d->windSpeed);y+=dy;
  {int f16=aircraft[s->aircraft].aeroModel==AERO_F16;float pwr=f16?f16Power(s->engine):s->engine*100;
   label(x0,y+2,.30f,C2D_Color32(170,178,186,255),s->spoilerActual>.02f?"PWR SPD":"PWR");labelRight(x1,y,.42f,s->spoilerActual>.02f||(f16&&pwr>50.5f)?yellow:white,"%.0f%%",pwr);y+=dy;}
  {const Aircraft *craft=&aircraft[s->aircraft];int detent=(int)roundf(s->flapActual*craft->flapSteps);
   label(x0,y+2,.30f,C2D_Color32(170,178,186,255),"FLAPS");
   if(craft->flapNames[0]&&detent>=0&&detent<=craft->flapSteps)labelRight(x1,y+1,.36f,white,"%s",craft->flapNames[detent]);else labelRight(x1,y+1,.36f,white,"%.0f",s->flapActual*100);}
 }
 (void)green;(void)paused;
}
static void attitude(const Flight *s,float fps,int paused){
 PfdData d=readPfd(s);u32 gray=C2D_Color32(59,64,72,255),dark=C2D_Color32(22,27,34,255),magenta=C2D_Color32(245,74,240,255),green=C2D_Color32(60,219,88,255),red=C2D_Color32(241,54,47,255);
 /* v1.1p18: PFD + MAP draws the map and data column, then shares the gear button, NAV strip and status */
 if(lowerMap||caseMap){mapScreen(s,&d,paused);goto mapShared;}
 float cr=cosf(s->roll),sr=sinf(s->roll),h=clampf(s->pitch*140,-260,260);
 /* Attitude horizon and pitch ladder; the surrounding panels mask overscan. */
 rect(0,0,320,240,C2D_Color32(30,108,181,255));
 float ax=160-600*cr+h*sr,ay=88+600*sr+h*cr,bx=160+600*cr+h*sr,by=88-600*sr+h*cr;
 u32 earth=C2D_Color32(123,77,39,255);
 pfdTriangle(ax,ay,bx,by,bx+600*sr,by+600*cr,earth);pfdTriangle(ax,ay,bx+600*sr,by+600*cr,ax+600*sr,ay+600*cr,earth);line(ax,ay,bx,by,1.3f,white);
 for(int deg=-30;deg<=30;deg+=5){
  if(!deg)continue;
  float yy=h-deg*PI/180*140,wide=deg%10?10:19;
  float x0=160-wide*cr+yy*sr,y0=88+wide*sr+yy*cr,x1=160+wide*cr+yy*sr,y1=88-wide*sr+yy*cr;
  if(y0>55&&y0<132&&y1>55&&y1<132){line(x0,y0,x1,y1,1,white);if(deg%10==0&&fabsf(s->roll)<.4f)label(x1+3,y1-5,.32f,white,"%d",abs(deg));}
 }
 rect(0,0,320,28,dark);rect(0,28,58,117,gray);rect(253,28,67,117,gray);rect(0,145,320,95,dark);
 /* Airspeed tape with 10-knot labels and a current-value pointer. */
 float low=stallSpeedCAS(s,0),maximum=pfdAirspeedLimit(s);
 for(int y=33;y<136;y++){
  float speed=s->ias+(88-y)/1.5f;u32 color=green;
  const Aircraft *craft=&aircraft[s->aircraft];
  if(speed<low)color=red;else if(craft->vno>0&&speed>craft->vno)color=yellow;
  if(speed>maximum)color=red;
  rect(48,y,4,1,color);
  if(craft->flapArc[1]>0&&speed>=craft->flapArc[0]&&speed<=craft->flapArc[1])rect(52,y,3,1,white);
 }
 int speedTick=(int)floorf(s->ias/10)*10;
 for(int v=speedTick-40;v<=speedTick+40;v+=10){float y=88+(s->ias-v)*1.5f;if(v>=0&&y>35&&y<132){line(39,y,47,y,1,white);if(fabsf(y-88)>13&&y>44&&y<126)label(7,y-5,.34f,white,"%3d",v);}}
 rect(2,78,44,21,black);pfdTriangle(46,78,56,88,46,99,black);label(7,77,.57f,s->ias>maximum?red:white,"%3.0f",s->ias);
 label(4,29,.34f,white,"IAS KT");label(4,134,.34f,white,"TAS %.0f",d.tas);
 /* Altitude tape: 100-foot ticks and a separate vertical-speed scale. */
 int altitudeTick=(int)floorf(d.altitude/100)*100;
 for(int v=altitudeTick-400;v<=altitudeTick+400;v+=100){float y=88+(d.altitude-v)*.15f;if(y>36&&y<133){line(255,y,263,y,1,white);if(fabsf(y-88)>13&&y>44&&y<126)label(265,y-6,.34f,white,"%d",v);}}
 if(apPanel.ap&&apPanel.alt){float y=clampf(88+(d.altitude-apPanel.altitude*3.28084f)*.15f,44,130);rect(253,y-4,4,9,cyan);label(297,34,.34f,cyan,"+");label(297,124,.34f,cyan,"-");}
 rect(260,78,44,21,black);pfdTriangle(260,78,253,88,260,99,black);label(263,80,.43f,white,"%5.0f",d.altitude);
 label(258,29,.34f,cyan,"ALT FT");label(258,134,.34f,cyan,"29.92");
 rect(305,34,14,101,dark);line(310,47,310,126,1,white);
 for(int v=-2;v<=2;v++){float y=88-v*18;line(307,y,abs(v)==2?316:313,y,1,white);}
 float vy=88-clampf(d.verticalSpeed/1000,-2.1f,2.1f)*18;pfdTriangle(305,vy,310,vy-3,310,vy+3,magenta);
 /* Bank marks, fixed aircraft symbol and specific-force slip/skid cue. */
 for(int bank=-60;bank<=60;bank+=10){if(abs(bank)==40||abs(bank)==50)continue;float a=bank*PI/180,r0=43,r1=bank%30?47:50;line(160+sinf(a)*r0,86-cosf(a)*r0,160+sinf(a)*r1,86-cosf(a)*r1,1,white);}
 float ba=-s->roll;pfdTriangle(160+sinf(ba)*40,86-cosf(ba)*40,157+sinf(ba)*35,86-cosf(ba)*35,163+sinf(ba)*35,86-cosf(ba)*35,white);
 line(120,88,145,88,4,black);line(175,88,200,88,4,black);line(120,88,145,88,2,yellow);line(175,88,200,88,2,yellow);line(145,88,145,93,2,yellow);line(175,88,175,93,2,yellow);pfdCircle(160,88,2,yellow);
 line(146,136,174,136,1,white);pfdCircle(160+d.slip*13,132,2.5f,white);
 /* Heading-up HSI. All angles are TRUE; no magnetic database or radio signal
    is invented. The simulated GPS course follows the selected runway axis. */
 pfdCircle(160,180,35,black);
 for(int tick=0;tick<36;tick++){
  float a=(tick*10-d.heading)*PI/180;float r=tick%3?32:29;
  line(160+sinf(a)*r,180-cosf(a)*r,160+sinf(a)*35,180-cosf(a)*35,1,white);
  if(tick%3==0){const char *card=tick==0?"N":tick==9?"E":tick==18?"S":tick==27?"W":NULL;if(card)label(160+sinf(a)*22-2,180-cosf(a)*22-5,.34f,white,"%s",card);}
 }
 float course=(d.course-d.heading)*PI/180,dx=sinf(course),dy=-cosf(course),rx=cosf(course),ry=sinf(course);
 line(160-dx*26,180-dy*26,160+dx*26,180+dy*26,1,magenta);
 pfdTriangle(160+dx*30,180+dy*30,160+dx*23+rx*3,180+dy*23+ry*3,160+dx*23-rx*3,180+dy*23-ry*3,magenta);
 float offset=d.cdi*13;line(160+rx*offset-dx*12,180+ry*offset-dy*12,160+rx*offset+dx*12,180+ry*offset+dy*12,2,magenta);
 for(int side=-2;side<=2;side++)if(side)pfdCircle(160+rx*side*7,180+ry*side*7,1,white);
 pfdTriangle(160,174,157,184,163,184,white);
 rect(140,141,40,15,black);
 {char hdg[8];snprintf(hdg,sizeof(hdg),"%03.0f",d.heading);labelFit(140,141,40,.40f,white,hdg);}
 /* HOLD heading bug, drawn over the heading box so it shows near the top. */
 if(apPanel.ap&&apPanel.hdg){float a=apPanel.heading-d.heading*PI/180;float bx=160+sinf(a)*39,by=180-cosf(a)*39,ox=cosf(a)*4.5f,oy=sinf(a)*4.5f;
  pfdTriangle(bx-ox,by-oy,bx+ox,by+oy,160+sinf(a)*30,180-cosf(a)*30,cyan);
  label(117,173,.34f,cyan,"<");label(197,173,.34f,cyan,">");}
 /* v31.16: one text size (0.34) in two columns beside the HSI. CRS left
    (same as DTK above); wind is one line; all angles are true. */
 label(5,148,.34f,white,"GS %.0f KT",d.groundSpeed);
 label(5,161,.34f,magenta,"GPS RWY %s",d.to?"TO":"FROM");
 label(5,174,.34f,cyan,"TRK %03.0f",d.track);
 /* v33.01: spoilers shown beside power; tap the row to cycle the lever. */
 /* v33: F-16 power is the engine's 0..100 percent (50 = military, above = afterburner). */
 {int f16=aircraft[s->aircraft].aeroModel==AERO_F16;float pwr=f16?f16Power(s->engine):s->engine*100;
  const char *spd=aircraft[s->aircraft].spoilerLift>0?"SPLR":"SPD BRK";
  if(s->spoilerActual>.02f)label(5,187,.34f,yellow,"PWR %.0f%% %s %.0f",pwr,spd,s->spoilerActual*100);
  else if(f16&&pwr>50.5f)label(5,187,.34f,yellow,"PWR %.0f%% AB",pwr);
  else label(5,187,.34f,white,"PWR %.0f%%",pwr);}
 {const Aircraft *craft=&aircraft[s->aircraft];int detent=(int)roundf(s->flapActual*craft->flapSteps);
  if(craft->flapNames[0]&&detent>=0&&detent<=craft->flapSteps)label(5,200,.34f,white,"FLAPS %s",craft->flapNames[detent]);
  else label(5,200,.34f,white,"FLAPS %.0f",s->flapActual*100);}
 /* v31.20: right column ends at the screen edge, like DTK above. */
 labelRight(316,148,.34f,white,"V/S %+.0f",d.verticalSpeed);
 if(d.windSpeed<1)labelRight(316,161,.34f,white,"WIND CALM");else labelRight(316,161,.34f,white,"WIND %03.0f/%.0f",d.windFrom,d.windSpeed);
 {float level=weatherLevel(s);labelRight(316,174,.34f,cyan,"%s",level<.5f?"CLEAR":level<1.4f?"SCATTERED":"T-STORM");}
 mapShared:
 maximum=pfdAirspeedLimit(s);
 /* Touchable gear button (colours below). The SR22's fixed gear reads FIXED.
    Touch area: GEAR_BUTTON_*. */
 {int fixedGear=!aircraft[s->aircraft].retractableGear;const char *gearText=fixedGear?"GEAR FIXED":s->gearActual>.99f?"GEAR DN":s->gearActual<.01f?"GEAR UP":"GEAR ...";
  /* v31.21: GEAR UP green, GEAR DN red, yellow while moving (owner's choice). */
  u32 gearColor=fixedGear?C2D_Color32(150,156,164,255):s->gearActual>.99f?red:s->gearActual<.01f?green:yellow;
  rect(GEAR_BUTTON_X,GEAR_BUTTON_Y,GEAR_BUTTON_W,GEAR_BUTTON_H,black);
  line(GEAR_BUTTON_X,GEAR_BUTTON_Y,GEAR_BUTTON_X+GEAR_BUTTON_W,GEAR_BUTTON_Y,1,gearColor);line(GEAR_BUTTON_X,GEAR_BUTTON_Y+GEAR_BUTTON_H,GEAR_BUTTON_X+GEAR_BUTTON_W,GEAR_BUTTON_Y+GEAR_BUTTON_H,1,gearColor);
  line(GEAR_BUTTON_X,GEAR_BUTTON_Y,GEAR_BUTTON_X,GEAR_BUTTON_Y+GEAR_BUTTON_H,1,gearColor);line(GEAR_BUTTON_X+GEAR_BUTTON_W,GEAR_BUTTON_Y,GEAR_BUTTON_X+GEAR_BUTTON_W,GEAR_BUTTON_Y+GEAR_BUTTON_H,1,gearColor);
  labelFit(GEAR_BUTTON_X+2,GEAR_BUTTON_Y+3,GEAR_BUTTON_W-4,.34f,gearColor,gearText);}
 labelRight(316,187,.34f,white,"%.0f FPS",fps);
 /* Navigation strip and honest mode/status annunciation; no fake COM/AP. */
 /* v31.16: the aircraft name is on the top screen; MANUAL never changed. */
 if(apPanel.nav&&apPanel.destination>=0){
  const Airport *to=&airports[apPanel.destination];float bearing=atan2f(to->east-s->pos.x,to->north-s->pos.y)*180/PI;if(bearing<0)bearing+=360;
  label(4,1,.36f,green,"NAV %s %.1f NM",to->id,hypotf(to->east-s->pos.x,to->north-s->pos.y)/1852);
  char brg[16];snprintf(brg,sizeof(brg),"BRG %03.0f",bearing);label(316-textWidth(brg,.36f),1,.36f,white,"%s",brg);
 }else{
  label(4,1,.36f,magenta,"GPS %s %.1f NM",airports[s->airport].id,d.distance);
  char dtk[16];snprintf(dtk,sizeof(dtk),"DTK %03.0f",d.course);label(316-textWidth(dtk,.36f),1,.36f,white,"%s",dtk);
 }
 line(0,14,320,14,1,gray);
 /* On the ground the stall cue is not annunciated (the wing is not flying). */
 int stall=s->stallWarning>.5f&&!s->onGround;
 {char status[48];u32 sc=green;
  if(stall||s->ias>maximum){sc=red;snprintf(status,sizeof(status),"%s",stall?"STALL":"OVERSPEED");}
  else if(paused){sc=yellow;snprintf(status,sizeof(status),"PAUSED");}
  else if(apNotice>0){sc=yellow;snprintf(status,sizeof(status),"%s",apNoticeText);}
  else if(apPanel.ap||apPanel.athr){
   char lateral[16]="",vertical[24]="",thrust[12]="";
   if(apPanel.ap){
    if(apPanel.hdg)snprintf(lateral,sizeof(lateral),"HDG %03.0f",fmodf(apPanel.heading*180/PI+360,360));
    else if(apPanel.nav&&apPanel.destination>=0)snprintf(lateral,sizeof(lateral),"NAV %s",airports[apPanel.destination].id);
    else snprintf(lateral,sizeof(lateral),"HDG HOLD");
    if(apPanel.vs)snprintf(vertical,sizeof(vertical),apPanel.alt?"VS%+.0f>%.0f":"VS%+.0f",apPanel.verticalSpeed*196.85f,apPanel.altitude*3.28084f);
    else if(apPanel.alt)snprintf(vertical,sizeof(vertical),"ALT %.0f",apPanel.altitude*3.28084f);
    else snprintf(vertical,sizeof(vertical),"ALT HOLD");
   }
   if(apPanel.athr)snprintf(thrust,sizeof(thrust),"AT %.0f",apPanel.speed);
   snprintf(status,sizeof(status),"%s%s %s %s",apPanel.ap?"AP ":"",lateral,vertical,thrust);
  }
  else if(s->onGround){sc=white;snprintf(status,sizeof(status),"ON GROUND");}
  else if(fbwStatus(s)){const char *law=fbwStatus(s);sc=strcmp(law,"NORMAL LAW")&&strcmp(law,"FLCS")&&strcmp(law,"GROUND LAW")?yellow:green;snprintf(status,sizeof(status),"%s",law);}
  else snprintf(status,sizeof(status),"MANUAL FLIGHT");
  labelFit(0,15,320,.34f,sc,status);}
 drawSoftkeys();
}

/* Original vector artwork: no bitmap uploads or external branding assets. */
static void splashScreen(const AppMenu *menu){
 u32 navy=C2D_Color32(10,24,42,255),blue=C2D_Color32(25,57,79,255),sand=C2D_Color32(184,101,66,255),shade=C2D_Color32(110,57,48,255);
 rect(0,0,400,240,navy);
 for(int i=0;i<20;i++)rect(0,90+i*5,400,5,C2D_Color32(28+i*3,58+i*2,79+i,255));
 C2D_DrawEllipseSolid(304,78,0,35,35,C2D_Color32(249,203,139,255));
 pfdTriangle(0,168,66,113,126,168,blue);pfdTriangle(82,168,179,126,267,168,blue);pfdTriangle(210,170,300,132,400,163,blue);
 pfdTriangle(0,164,56,150,104,240,shade);pfdTriangle(0,164,104,240,0,240,sand);
 pfdTriangle(400,151,344,171,284,240,sand);pfdTriangle(400,151,284,240,400,240,shade);
 pfdTriangle(80,240,187,169,234,240,C2D_Color32(117,68,52,255));
 pfdTriangle(175,240,254,183,309,240,C2D_Color32(150,79,54,255));
 /* A clean swept planform mark over the canyon horizon. */
 pfdTriangle(247,107,188,141,205,143,white);pfdTriangle(247,107,224,147,215,145,white);
 line(200,144,247,107,3,white);pfdTriangle(211,136,201,129,202,140,white);
 label(25,23,.88f,white,"3DMicroFlight");line(26,65,153,65,2,cyan);
 /* v1.1p21: the five aircraft and the US scenery (was the v32 two-aircraft Arizona card) */
 label(27,74,.32f,cyan,"SR22 / SF50 / 737 / A321 / F-16");
 rect(0,207,400,33,navy);label(25,212,.34f,white,"COAST TO COAST  /  %d AIRPORTS",airportStartCount());
 label(26,227,.25f,cyan,"NEW NINTENDO 3DS XL");label(268,227,.25f,white,"%s  |  LOWER 48 US",AUTOTEST_BUILD);
 (void)menu;
}
static void drawAppMenu(const AppMenu *m,const char *notice){
 static const char *weatherNames[]={"Clear / calm","Clouds / breeze","Storm / gusts","Varied by airport"};
 const char *title=m->screen==MENU_TITLE?"WELCOME ABOARD":m->screen==MENU_SETUP?"PLAN YOUR FLIGHT":m->screen==MENU_PAUSE?"FLIGHT PAUSED":m->screen==MENU_SAVE_SLOTS?"SAVE FLIGHT - CHOOSE SLOT":m->screen==MENU_LOAD_SLOTS?"LOAD FLIGHT - CHOOSE SLOT":m->screen==MENU_AUDIO?"AUDIO & CONTROLS":"EXIT 3DMICROFLIGHT?";
 rect(0,0,320,240,black);label(15,10,.48f,white,"%s",title);line(14,31,306,31,1,cyan);
 int rows=menuRows(m);
 for(int i=0;i<rows;i++){
  int disabled=(m->screen==MENU_TITLE&&i==1&&!m->hasSave)||(m->screen==MENU_PAUSE&&i==3&&!m->hasSave)||(m->screen==MENU_LOAD_SLOTS&&i<3&&!(m->hasSave&(1<<i)));
  u32 c=disabled?C2D_Color32(103,116,128,255):white;
  rect(12,38+menuRowHeight(m)*i,296,menuRowHeight(m)<23?menuRowHeight(m)-2:23,i==m->row?C2D_Color32(32,83,105,255):C2D_Color32(21,34,47,255));
  char text[120];text[0]=0;
  if(m->screen==MENU_TITLE){const char *items[]={"New flight",m->hasSave?"Continue saved flight":"Continue - no saved flight","Audio & controls","Exit"};
   if(i==4)snprintf(text,sizeof(text),"Run autotest (%d cases)",m->autotestCases);else snprintf(text,sizeof(text),"%s",items[i]);}
  else if(m->screen==MENU_SETUP){
   if(i==0)snprintf(text,sizeof(text),"Aircraft    < %s >",aircraft[m->aircraft].name);
   else if(i==1)snprintf(text,sizeof(text),"Airport  < %s  %s  %s >",airports[m->airport].state?airports[m->airport].state:"",airports[m->airport].name,airports[m->airport].id);
   else if(i==2)snprintf(text,sizeof(text),"Stage       < %s >",flightPhaseName(m->phase));
   else if(i==3)snprintf(text,sizeof(text),"Weather     < %s >",weatherNames[m->weather]);
   else if(i==4)snprintf(text,sizeof(text),"Time        < %s >",timeSettingNames[m->timeOfDay]);
   else snprintf(text,sizeof(text),"%s",i==5?"Start flight":"Back");
  }else if(m->screen==MENU_PAUSE){char swap[48];snprintf(swap,sizeof(swap),"Switch to %s",aircraft[(m->aircraft+1)%AIRCRAFT_COUNT].name);
   const char *items[]={"Resume flight",swap,"Save flight",m->hasSave?"Load saved flight":"Load - no saved flight","New flight","Title screen","Audio & controls","Exit"};
   snprintf(text,sizeof(text),"%s",items[i]);} /* v1.1p20: row 4 is New flight (the autotest label was copied from the title menu) */
  else if(m->screen==MENU_AUDIO){
   if(i==0)snprintf(text,sizeof(text),"Volume   < %d%% >",m->volume*20);
   else if(i==1)snprintf(text,sizeof(text),"Sound    < %s >",!audioReady?"Unavailable":m->muted?"Muted":"On");
   else if(i==2)snprintf(text,sizeof(text),"Calibrate stick  (release, press A)");
   else if(i==3)snprintf(text,sizeof(text),"Stick deadzone   < %d%% >",m->deadzone);
   else if(i==4)snprintf(text,sizeof(text),"Lower screen     < %s >",m->lowerMap?"PFD + MAP":"PFD");
   else if(i==5){static const char *names[FLIGHT_LEVELS]={"Easy","Forgiving","Normal","Hard","Advanced"};
    int lv=m->level<0?0:m->level>=FLIGHT_LEVELS?FLIGHT_LEVELS-1:m->level;snprintf(text,sizeof(text),"Difficulty  %s",names[lv]);
    /* v1.1p21 slider: Easy (green) .. Advanced (red), the knob on the level */
    float y=38+menuRowHeight(m)*i+menuRowHeight(m)/2-1,x0=190,x1=296;
    rect(x0,y-1,x1-x0,3,C2D_Color32(80,96,112,255));
    for(int k=0;k<FLIGHT_LEVELS;k++){float x=x0+(x1-x0)*k/(FLIGHT_LEVELS-1);u32 tc=C2D_Color32(70+45*k,200-35*k,90,255);
     if(k<=lv)rect(x-(k?(x1-x0)/(FLIGHT_LEVELS-1):0),y-1,k?(x1-x0)/(FLIGHT_LEVELS-1):0,3,tc);rect(x-1,y-4,2,9,k==lv?white:C2D_Color32(120,136,150,255));}
    float kx=x0+(x1-x0)*lv/(FLIGHT_LEVELS-1);rect(kx-4,y-6,8,13,white);rect(kx-3,y-5,6,11,C2D_Color32(70+45*lv,200-35*lv,90,255));
   }
   else snprintf(text,sizeof(text),"Back");
  }else if(m->screen==MENU_SAVE_SLOTS||m->screen==MENU_LOAD_SLOTS){
   if(i==3)snprintf(text,sizeof(text),"Back");
   else snprintf(text,sizeof(text),"Slot %d  -  %s",i+1,m->hasSave&(1<<i)?"Saved flight":"Empty");
  }else snprintf(text,sizeof(text),"%s",i==0?"Cancel - return":"Exit to launcher");
  label(21,(menuRowHeight(m)<23?40:42)+menuRowHeight(m)*i,.37f,c,"%s",text);
 }
 if(m->screen==MENU_AUDIO){
  circlePosition c;hidCircleRead(&c);
  label(16,174,.29f,white,"Stick raw x %+4d  y %+4d   centre %+d %+d",c.dx,c.dy,stickCentreX,stickCentreY);
  label(16,184,.29f,cyan,"Input  roll %+.2f  pitch %+.2f",stickAxis(c.dx,stickCentreX,m->deadzone),stickAxis(c.dy,stickCentreY,m->deadzone));
 }
 if(m->screen==MENU_AUDIO&&m->row==5){static const char *what[FLIGHT_LEVELS]={"Easy: nothing crashes - impacts put you back on a safe path","Forgiving: hard landings forgiven, terrain still crashes","Normal: crash above 4.5 m/s sink or 17 deg bank at touchdown","Hard: 3.5 m/s sink, 14 deg bank","Advanced: 3.0 m/s sink, 11 deg bank, tighter pitch"};
  int lv=m->level<0?0:m->level>=FLIGHT_LEVELS?FLIGHT_LEVELS-1:m->level;label(15,201,.27f,yellow,"%s",what[lv]);}
 if(m->screen==MENU_SAVE_SLOTS)label(16,149,.30f,yellow,"Selecting a used slot replaces that save.");
 if(m->screen==MENU_EXIT){label(16,101,.33f,yellow,"Unsaved flight changes will be lost.");label(16,117,.3f,white,"Cancel, then Save flight to keep progress.");}
 if(notice[0])label(15,194,.29f,yellow,"%s",notice);
 if(m->screen==MENU_SETUP&&m->row==1)label(15,214,.30f,cyan,"D-pad: airport   L / R: state   Y: nearest");   /* v1.1p20 */
 else label(15,214,.30f,cyan,"D-pad: select / change    A: confirm");label(15,229,.27f,white,"Touch: select    B: back    START: menu");
}

int main(void){
 gfxInitDefault();gfxSet3D(true);osSetSpeedupEnable(true);
 if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE*2)){gfxExit();return 1;}
 if(!C2D_Init(16000)){C3D_Fini();gfxExit();return 1;}C2D_Prepare();
 texts=C2D_TextBufNew(8192);if(!texts){C2D_Fini();C3D_Fini();gfxExit();return 1;}
 pixelFontInit(); /* without it, small text keeps the system font */
 C3D_RenderTarget *top=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8),*bottom=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
 C3D_RenderTargetSetOutput(top,GFX_TOP,GFX_LEFT,
  GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
 C3D_RenderTarget *right=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
 if(!top||!right||!bottom){C2D_TextBufDelete(texts);C2D_Fini();C3D_Fini();gfxExit();return 1;}
 C3D_RenderTargetSetOutput(right,GFX_TOP,GFX_RIGHT,
  GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
 white=C2D_Color32(245,245,240,255);black=C2D_Color32(12,18,25,255);cyan=C2D_Color32(62,221,240,255);yellow=C2D_Color32(255,224,54,255);
 mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/3DMicroFlight",0777);migrateOldFolder();
 /* v1.1p20: every flyable US runway, before saves and the autotest script name them */
 airportsLoad(AIRPORTS_FLY_PATH);
 int extendedInput=R_SUCCEEDED(irrstInit());
 u32 previousExtended=0;view=(View){0};resetView(&view);
 texturedReady=initAircraftRenderer();
 audioReady=initFlightAudio();
 AppMenu menu={.screen=MENU_TITLE,.phase=PHASE_TAKEOFF,.volume=3,.timeOfDay=TIME_NOON};loadControls(&menu);lowerMap=menu.lowerMap;flightLevel=menu.level;
 char menuNotice[96]="";
 Flight saved;View savedView;for(int slot=0;slot<3;slot++)if(loadFlightState(savePaths[slot],&saved,&savedView))menu.hasSave|=1<<slot;
 Flight s;resetFlight(&s,0,0);u64 previous=osGetTime();float accumulator=0,fps=60;int paused=0,zlHeldLast=0,zlZoomUsed=0;
 /* v1.1p12: the case list and summary live on the heap; as static arrays they pushed the program image past
    32 MB (cirrusflight-32mb-code-limit: such a CIA dies at its first instruction on hardware). */
 static AutotestCase *autoCases;if(!autoCases)autoCases=calloc(AUTOTEST_MAX_CASES,sizeof(AutotestCase));int autoRejected=0;
 int autoCount=!autoCases?0:autotestLoad(AUTOTEST_PATH,autoCases,AUTOTEST_MAX_CASES,&autoRejected),autoIndex=-1,autoShot=0;float autoClock=0;u64 autoRunStart=0,autoCaseStart=0;
#define AUTO_SUMMARY_SIZE 524288
 static char *autoSummary;if(!autoSummary)autoSummary=calloc(1,AUTO_SUMMARY_SIZE);if(!autoSummary)autoCount=0;int autoSummaryLen=0; /* v32.04: 8 KB filled up after ~28 cases; v33.09: ~560 B per case with the dyn line */
 /* v35.15 (as on the Thor): an autotest.txt on the card no longer takes over the game. The title menu
    offers "Run autotest (N cases)" next to normal flying; the test starts by itself only when
    autotest.go exists (the Azahar runner writes it), which is removed so the next launch is normal.
    autoCount < 0: a script is loaded but not running. A run started from the menu returns to it. */
 int autoFromMenu=0,autoStartNow=0;
 if(autoCount>0){
  menu.autotestCases=autoCount;
  FILE *go=fopen(AUTOTEST_GO_PATH,"rb");if(go){fclose(go);remove(AUTOTEST_GO_PATH);autoStartNow=1;}else autoCount=-autoCount;
 }
 while(aptMainLoop()){
  if(autoStartNow){
   autoStartNow=0;if(autoCount<0)autoCount=-autoCount;
   /* Earlier results are never overwritten: an existing autotest folder is renamed first. */
   {struct stat st;if(stat(AUTOTEST_DIR,&st)==0){char kept[96];time_t now=time(NULL);struct tm *t=localtime(&now);
     snprintf(kept,sizeof(kept),AUTOTEST_DIR "-%04d%02d%02d-%02d%02d%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);rename(AUTOTEST_DIR,kept);}}
   /* v35.02: the test keeps running with the lid closed (backlights off); normal play still sleeps */
   aptSetSleepAllowed(false);lidWatchStart();
   autoRunStart=osGetTime();autoShot=0;
   mkdir(AUTOTEST_DIR,0777);autoIndex=0;autoCaseStart=osGetTime();startAutotestCase(&autoCases[0],&s,&menu,&paused);autoClock=0;previous=osGetTime();
   autoSummaryLen=snprintf(autoSummary,AUTO_SUMMARY_SIZE,"3DMicroFlight " AUTOTEST_BUILD " autotest: %d cases, %d rejected lines, sleep on lid close off, lid watch %s, ground map %s, started from %s\n",autoCount,autoRejected,lidServices?"on":"UNAVAILABLE",sceneryGroundMapLoaded()?"loaded":"MISSING",autoFromMenu?"the menu":"autotest.go");
  }
  u64 frameMark=svcGetSystemTick();float inputMs=0,physicsMs=0,waitMs=0,drawMs=0,endMs=0;int readsBefore=terrainStreamSyncReads();
  hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();
  if(autoIndex>=0){lidWatch(1/60.f);
   unsigned el=(unsigned)((osGetTime()-autoRunStart)/1000);
   snprintf(autotestInfo,sizeof(autotestInfo),"%s  %d/%d %.31s  %u:%02u",AUTOTEST_BUILD,autoIndex+1,autoCount,autoCases[autoIndex].name,el/60,el%60);
  }
  circlePosition look={0,0};
  if(extendedInput){
   irrstScanInput();u32 extra=irrstKeysHeld();
   down=(down&~(KEY_ZL|KEY_ZR))|(extra&~previousExtended&(KEY_ZL|KEY_ZR));
   held|=extra;previousExtended=extra;
   irrstCstickRead(&look);
  }
  /* v35.12: autotests ignore the C-stick. On hardware a stick resting off centre turned the cockpit
     view every frame until it reached the stop (looking back past the left wing), which garbled the
     v35.09/v35.10 cockpit cases; Azahar has no stick input, so it never showed there. The peak
     reading per case goes into the summary. */
  if(autoIndex>=0){
   int ax=abs(look.dx),ay=abs(look.dy);if(ax>cstickPeakX)cstickPeakX=ax;if(ay>cstickPeakY)cstickPeakY=ay;
   look.dx=look.dy=0;
  }

  /* v1.1p13: osGetTime() can step back when the system clock is resynced (once in the 6.8 h v1.1p12 run);
     the unsigned difference then wrapped to 2^64 ms and broke that case's FPS line. Such a frame is not recorded. */
  u64 now=osGetTime();int clockBack=now<previous;if(clockBack)now=previous;float rawElapsed=(now-previous)/1000.f;float elapsed=clampf(rawElapsed,0,.1f);previous=now;
  holdingScenery=0;
  if(sceneryHoldLeft>0&&menu.screen==MENU_FLIGHT){
   if(!texturedReady||!rendererSceneryLoading(s.pos))sceneryHoldLeft=0;
   else{float step=fminf(rawElapsed,2.f);sceneryHoldLeft-=step;sceneryHoldTime+=step;holdingScenery=sceneryHoldLeft>0;}
  }
  setSceneryLoading(holdingScenery);
  if(autoIndex>=0&&!holdingScenery&&rawElapsed<1)autoClock+=rawElapsed;
  if(profileScreen!=menu.screen){memset(&frameProfile,0,sizeof(frameProfile));profileScreen=menu.screen;}
  /* v34.03: the frame that finishes a loading hold carries the whole scenery
     build (about 1.2 s on hardware); keep it out of FPS, p95 and worst. */
  static int heldLastFrame;int loadFrame=holdingScenery||heldLastFrame;heldLastFrame=holdingScenery;
  if(!loadFrame&&!clockBack)recordFrameProfile(&frameProfile,rawElapsed,C3D_GetProcessingTime(),C3D_GetDrawingTime(),s.aircraft,view.exterior,paused||menu.screen!=MENU_FLIGHT,osGet3DSliderState()>.01f);
  if(frameProfile.count<30||frameProfile.next%30==0)frameStats=frameProfileStats(&frameProfile);
  aircraftNotice=fmaxf(0,aircraftNotice-elapsed);gearNotice=fmaxf(0,gearNotice-elapsed);
  if(frameProfile.sum>0)fps=frameProfile.count*1000/frameProfile.sum;
  int menuWasOpen=menu.screen!=MENU_FLIGHT;
  if((down&KEY_START)||(down&KEY_B&&menuWasOpen)){
   if(menu.screen==MENU_FLIGHT){menu.aircraft=s.aircraft;menu.airport=s.airport;menu.phase=s.startPhase;menu.weather=s.weather>=WEATHER_VARIED?WEATHER_VARIED:s.weather;}
   menuBack(&menu);if(menu.screen==MENU_FLIGHT)paused=0;accumulator=0;zlHeldLast=0;zlZoomUsed=0;
  }
  if(menuWasOpen||menu.screen!=MENU_FLIGHT){
   int action=MENU_NONE;
   if(menu.screen!=MENU_FLIGHT){
    if(down&KEY_DUP)menuMove(&menu,-1);
    if(down&KEY_DDOWN)menuMove(&menu,1);
    if(down&KEY_DLEFT)menuAdjust(&menu,-1);
    if(down&KEY_DRIGHT)menuAdjust(&menu,1);
    if(down&KEY_L)menuAdjustGroup(&menu,-1);
    if(down&KEY_R)menuAdjustGroup(&menu,1);
    if((down&KEY_Y)&&menu.screen==MENU_SETUP&&menu.row==1){int near=airportNearest(s.pos.x,s.pos.y,0);if(near>=0)menu.airport=near;}
    if(down&KEY_A)action=menuAccept(&menu);
    if(down&KEY_TOUCH){touchPosition t;hidTouchRead(&t);int row=(t.py-38)/menuRowHeight(&menu);
     if(t.py>=38&&t.py<38+menuRowHeight(&menu)*menuRows(&menu)&&t.px>=12&&t.px<308){
      menu.row=row;if(menu.screen==MENU_SETUP&&row<5)menuAdjust(&menu,t.px<160?-1:1);
      else if(menu.screen==MENU_AUDIO&&row==5&&t.px>=180){int lv=(int)((t.px-190)*(FLIGHT_LEVELS-1)/106.f+.5f);menu.level=lv<0?0:lv>=FLIGHT_LEVELS?FLIGHT_LEVELS-1:lv;} /* v1.1p21 slider */
      else action=menuAccept(&menu);
     }
    }
   }
   if(action==MENU_QUIT)break;
   if(action==MENU_AUTOTEST&&autoCount<0){autoFromMenu=1;autoStartNow=1;menuNotice[0]=0;}
   if(action==MENU_CALIBRATE)calibrateStick(&menu,menuNotice,sizeof(menuNotice));
   if(menu.lowerMap!=lowerMap){lowerMap=menu.lowerMap;saveControls(&menu);} /* v1.1p18 */
   if(autoIndex<0&&menu.level!=flightLevel){flightLevel=menu.level;saveControls(&menu);} /* v1.1p21 */
   if(action==MENU_SAVE){
    if(saveFlightState(savePaths[menu.slot],&s,&view)){menu.hasSave|=1<<menu.slot;snprintf(menuNotice,sizeof(menuNotice),"Flight saved to Slot %d.",menu.slot+1);}
    else snprintf(menuNotice,sizeof(menuNotice),"Save failed. Check SD space / access.");
   }
   if(action==MENU_START_FLIGHT||action==MENU_LOAD){
    saveFlightRecording();
    if(recorderSaveFailed)snprintf(menuNotice,sizeof(menuNotice),"Log save failed. Retry before changing flight.");
    else if(action==MENU_START_FLIGHT){
     /* VARIED keeps the flight's seed when it already had one, else a new one. */
     int weather=menu.weather<WEATHER_VARIED?menu.weather:s.weather>=WEATHER_VARIED?s.weather:WEATHER_VARIED+(int)(osGetTime()%WEATHER_SEEDS);
     resetFlightPhase(&s,menu.aircraft,menu.airport,menu.phase);setWeather(&s,weather);daylightSet(timeSettingHours(menu.timeOfDay));resetView(&view);beginSceneryHold();
     menu.screen=MENU_FLIGHT;paused=0;menuNotice[0]=0;
    }else if(loadFlightState(savePaths[menu.slot],&s,&view)){beginSceneryHold();
     menu.screen=MENU_FLIGHT;paused=1;menuNotice[0]=0;
    }else {menu.hasSave&=~(1<<menu.slot);snprintf(menuNotice,sizeof(menuNotice),"Load failed. Save missing or incompatible.");}
   }
   if(action==MENU_RESUME){menu.screen=MENU_FLIGHT;paused=0;}
   if(action==MENU_SWITCH_AIRCRAFT){switchAircraft(&s,(s.aircraft+1)%AIRCRAFT_COUNT);aircraftNotice=2.5f;apOff(NULL);menu.screen=MENU_FLIGHT;paused=0;}
   accumulator=0;goto render_frame;
  }
  if(down&KEY_SELECT){int weather=s.weather,phase=s.startPhase;if(held&KEY_ZL){phase=(phase+1)%PHASE_COUNT;zlZoomUsed=1;}resetFlightPhase(&s,s.aircraft,s.airport,phase);setWeather(&s,weather);resetView(&view);accumulator=0;beginSceneryHold();}
  if(down&KEY_ZR)view.exterior=!view.exterior;
  int zlHeld=(held&KEY_ZL)!=0;
  if(zlHeld&&!zlHeldLast&&!(down&KEY_SELECT))zlZoomUsed=0;
  int zoomChord=view.exterior&&zlHeld;
  if(zoomChord&&(held&(KEY_DUP|KEY_DDOWN))){
   zlZoomUsed=1;zoomView(&view,elapsed,(held&KEY_DUP?1.f:0)-(held&KEY_DDOWN?1.f:0));
  }
  if(!zlHeld&&zlHeldLast&&!zlZoomUsed)resetView(&view);
  zlHeldLast=zlHeld;
  moveView(&view,elapsed,look.dx/156.f,look.dy/156.f);
  if(down&KEY_Y){if(zlHeld){zlZoomUsed=1;if(flightRecorderCount()){paused=1;saveFlightRecording();}else{flightRecorderBegin(&s);recorderSaveFailed=0;}}else paused=!paused;}
  if((down&KEY_X)&&zlHeld){
   FrameStats captured=frameProfileStats(&frameProfile);paused=1;zlZoomUsed=1;
   mkdir("sdmc:/3ds/3DMicroFlight/performance",0777);char path[160];
   snprintf(path,sizeof(path),"sdmc:/3ds/3DMicroFlight/performance/frame_%llu.csv",(unsigned long long)osGetTime());
   if(saveFrameProfile(&frameProfile,path))snprintf(performanceNotice,sizeof(performanceNotice),"Saved %.1f FPS | p95 %.1f ms | %d frames",captured.fps,captured.p95Ms,captured.samples);
   else snprintf(performanceNotice,sizeof(performanceNotice),"Performance save failed - check SD space.");
  }
  /* X is the gear key; the AIRCRAFT softkey swaps aircraft. A remains a gear
     alias for existing muscle memory. */
  if((down&KEY_X)&&!zlHeld)requestGear(&s);
  if(down&KEY_A){if(zlHeld){s.engineCutoff=!s.engineCutoff;zlZoomUsed=1;}else requestGear(&s);}
  if((down&KEY_B)&&!zlHeld)nextFlap(&s);
  if(down&KEY_TOUCH){touchPosition t;hidTouchRead(&t);if(t.py>=216){
   int button=t.px*6/320;
   if(button==0)apPanelOpen=!apPanelOpen;
   else if(button==1){int weather=s.weather;resetFlightPhase(&s,s.aircraft,nextAirportStart(s.airport,1),s.startPhase);setWeather(&s,weather);accumulator=0;beginSceneryHold();}
   else if(button==2)setWeather(&s,s.weather>=WEATHER_VARIED?0:s.weather==2?WEATHER_VARIED+(int)(osGetTime()%WEATHER_SEEDS):s.weather+1);
   else if(button==3)view.exterior=!view.exterior;
   else if(button==4){menu.aircraft=s.aircraft;menu.airport=s.airport;menu.phase=s.startPhase;menu.weather=s.weather>=WEATHER_VARIED?WEATHER_VARIED:s.weather;menu.screen=MENU_PAUSE;menu.row=0;paused=1;}
   else nextFlap(&s);
  }else if(t.px>=GEAR_BUTTON_X-4&&t.px<GEAR_BUTTON_X+GEAR_BUTTON_W+4&&t.py>=GEAR_BUTTON_Y-4)requestGear(&s);
  else if(apPanelOpen)apPanelTouch(&s,t.px,t.py);
  else if((lowerMap||caseMap)&&t.px<MAP_RANGE_W+6&&t.py>=MAP_RANGE_Y-4&&t.py<216){mapRangeIndex=(mapRangeIndex+1)%MAP_RANGES;saveControls(&menu);}
  else if(!(lowerMap||caseMap)&&(aircraft[s.aircraft].spoilerLift>0||aircraft[s.aircraft].spoilerDrag>0)&&t.px<118&&t.py>=184&&t.py<198)s.speedbrake=s.speedbrake>.75f?0:s.speedbrake+.5f;
  else if(!(lowerMap||caseMap)&&apPanel.ap&&apPanel.hdg&&t.px>=122&&t.px<198&&t.py>=141&&t.py<216){apPanel.heading+=(t.px<160?-10:10)*PI/180;apPanel.heading=fmodf(apPanel.heading+4*PI,2*PI);}
  else if(!(lowerMap||caseMap)&&apPanel.ap&&apPanel.alt&&t.px>=253&&t.px<305&&t.py>=28&&t.py<145)apPanel.altitude=fmaxf(apPanel.altitude+(t.py<88?500:-500)/3.28084f,terrain(s.pos.x,s.pos.y)+150);
  else if(apPanel.nav&&t.py<14){int next=apNextDestination(&s,apPanel.destination);if(next>=0)apPanel.destination=next;}
  else if(t.py<28)renderTest=(renderTest+1)%RENDER_TEST_COUNT;
  }
  s.brakeLeft=zlHeld&&((held&KEY_B)||(held&KEY_L))?1:0;
  s.brakeRight=zlHeld&&((held&KEY_B)||(held&KEY_R))?1:0;
  if(zlHeld&&(held&(KEY_A|KEY_B|KEY_L|KEY_R)))zlZoomUsed=1;
  apNotice=fmaxf(0,apNotice-elapsed);
  circlePosition cp;hidCircleRead(&cp);
  terrainStreamUpdate(s.pos); /* before physics: nearby 100 m tiles are always resident */
  inputMs=msSince(&frameMark);
  if(!paused&&!holdingScenery){
   s.throttle=clampf(s.throttle+(zoomChord?0:elapsed*.3f)*((held&KEY_DUP?1:0)-(held&KEY_DDOWN?1:0)),0,1);
   s.trim=clampf(s.trim+elapsed*.12f*((held&KEY_DRIGHT?1:0)-(held&KEY_DLEFT?1:0)),-.3f,.3f);
   accumulator+=elapsed;
   /* Autotest cases fly hands-off so hardware captures are repeatable. */
   float pitchIn=autoIndex>=0?0:-stickAxis(cp.dy,stickCentreY,menu.deadzone),rollIn=autoIndex>=0?0:stickAxis(cp.dx,stickCentreX,menu.deadzone),rudderIn=autoIndex>=0?0:(held&KEY_R?1.f:0)-(held&KEY_L?1.f:0);
   if(apPanel.ap&&(fabsf(pitchIn)>.3f||fabsf(rollIn)>.3f||(!zlHeld&&rudderIn!=0)))apOff("AP OFF - STICK");
   if(apPanel.athr&&!zoomChord&&(held&(KEY_DUP|KEY_DDOWN)))athrOff("A/THR OFF - THROTTLE");
   while(accumulator>=1/120.f){if(autoIndex>=0&&autopilotOn)autopilotStep(&autopilot,&s,1/120.f,&pitchIn,&rollIn,&rudderIn);
    else if(autoIndex>=0&&maneuverOn)maneuverStep(&maneuver,&s,1/120.f,&pitchIn,&rollIn,&rudderIn);
    else if(apPanel.ap||apPanel.athr)autopilotPanelStep(&apState,&s,&apPanel,1/120.f,&pitchIn,&rollIn,&rudderIn);
    stepFlight(&s,1/120.f,pitchIn,rollIn,rudderIn);daylightAdvance(1/120.f);flightRecorderStep(&s,1/120.f);if(autoIndex>=0)dynRecord(&s,1/120.f,pitchIn,rollIn);accumulator-=1/120.f;}
  }else accumulator=0;
  physicsMs=msSince(&frameMark);
 render_frame:
  if(!inputMs&&!physicsMs)inputMs=msSince(&frameMark);
  setFlightAudio(&s,menu.screen==MENU_FLIGHT&&!paused,view.exterior,menu.volume/5.f,menu.muted);
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);C2D_TextBufClear(texts);boldLowerText=false;
  waitMs=msSince(&frameMark);
  /* Queue all screen clears before any screen draws. In particular, avoid
     inserting a second-eye clear into a partially submitted cockpit frame. */
  float sky[3];daylightSky(0,sky);u32 clearColor=MAGENTA_MODE(renderTest)?0xFF00FFFF:((u32)(sky[0]*255)<<24|(u32)(sky[1]*255)<<16|(u32)(sky[2]*255)<<8|0xFF);
  setRendererDebug(renderTest==RENDER_NO_PHOTOS,renderTest==RENDER_STRIP_RANGES,renderTest==RENDER_MAGENTA_TWICE,renderTest==RENDER_MAGENTA_NO_CULL);
  setRendererProbe((renderTest==RENDER_MAGENTA_DEPTH_ALWAYS?PROBE_DEPTH_ALWAYS:0)|(renderTest==RENDER_MAGENTA_NO_GUARD?PROBE_NO_GUARD:0)|(renderTest==RENDER_MAGENTA_ROTATED?PROBE_ROTATE_TRIANGLES:0)
   |(renderTest==RENDER_MAGENTA_ALIGNED?PROBE_ALIGN_RANGES:0)|(renderTest==RENDER_MAGENTA_SMALL_DRAWS?PROBE_SMALL_DRAWS:0)|(renderTest==RENDER_MAGENTA_ARRAYS?PROBE_DRAW_ARRAYS:0));
  C3D_RenderTargetClear(top,C3D_CLEAR_ALL,clearColor,0);
  C3D_RenderTargetClear(right,C3D_CLEAR_ALL,clearColor,0);
  C3D_RenderTargetClear(bottom,C3D_CLEAR_ALL,__builtin_bswap32(black),0);
  int title=menu.screen==MENU_TITLE||menu.screen==MENU_SETUP||((menu.screen==MENU_EXIT||menu.screen==MENU_LOAD_SLOTS)&&menu.returnScreen==MENU_TITLE);
  if(!title)beginRendererFrame();
  setRendererExterior(view.exterior);
  float depth=osGet3DSliderState(),convergence=view.exterior?view.distance:2.f;
  /* Conservative maximum far-field disparity (~3.3 pixels at full slider). */
  float iod=depth*(view.exterior?convergence*.015f:.008f);
  for(int eye=0;eye<(depth>0?2:1);eye++){
  cockpitEye=depth>0?(eye?iod:-iod):0;cockpitConvergence=convergence;
  setRendererEye(cockpitEye,convergence);
  C3D_RenderTarget *target=eye?right:top;
  C2D_SceneBegin(target);C2D_Prepare();
  C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);
  if(title)splashScreen(&menu);else scenery(&s,paused||menu.screen!=MENU_FLIGHT);
  if(!title&&(paused||menu.screen!=MENU_FLIGHT)){label(160,35,.6f,yellow,"PAUSED");label(7,55,.36f,white,"CPU %.1f ms  GPU %.1f ms",C3D_GetProcessingTime(),C3D_GetDrawingTime());label(7,91,.30f,white,"%.1f FPS | p95 %.1f ms | worst %.1f ms | %d long frames",fps,frameStats.p95Ms,frameStats.maxMs,spikeTotal);if(!view.exterior&&texturedReady){CockpitRenderStats stats=cockpitRenderStats();label(7,67,.30f,cyan,"VC %d fixed + %d live | %.1f KiB updated",stats.fixedTriangles,stats.liveTriangles,stats.uploadBytes/1024.f);}}
  if(!extendedInput)label(6,197,.4f,yellow,"C-STICK / ZL / ZR UNAVAILABLE");
  }
  C2D_SceneBegin(bottom);boldLowerText=true;
  if(menu.screen==MENU_FLIGHT){if(apPanelOpen){drawApPanel(&s);drawSoftkeys();}else attitude(&s,fps,paused);}else drawAppMenu(&menu,menuNotice);
  drawMs=msSince(&frameMark);
  C3D_FrameEnd(0);
  endMs=msSince(&frameMark);
  {float streamMs,buildMs;int parts;rendererFrameTimes(&streamMs,&buildMs,&parts);
   float total=inputMs+physicsMs+waitMs+drawMs+endMs;
   float stage[4];rendererBuildStages(stage);
   if(autoIndex>=0&&!holdingScenery){float part[7]={total,inputMs,physicsMs,waitMs,drawMs-streamMs-buildMs,streamMs,buildMs};for(int k=0;k<7;k++)caseSum[k]+=part[k];for(int k=0;k<4;k++)caseStage[k]+=stage[k];caseFrames++;}
   if(total>SPIKE_MS&&menu.screen==MENU_FLIGHT){
    spikeTotal++;
    if(spikeCount==SPIKE_CAPACITY&&autoIndex<0){mkdir("sdmc:/3ds/3DMicroFlight/logs",0777);saveSpikes("sdmc:/3ds/3DMicroFlight/logs/spikes.csv",1);}
    if(spikeCount<SPIKE_CAPACITY)spikes[spikeCount++]=(Spike){autoIndex>=0?autoClock:osGetTime()/1000.f,total,inputMs,physicsMs,waitMs,drawMs,endMs,streamMs,buildMs,parts,terrainStreamSyncReads()-readsBefore,holdingScenery,view.exterior};
   }
  }
  if(autoIndex>=0){
   const AutotestCase *c=&autoCases[autoIndex];float t=autoClock;
   if(autoShot<c->shots&&t>=c->warmup+autoShot*c->interval){
    char path[160];snprintf(path,sizeof(path),AUTOTEST_DIR "/%s_%02d.bmp",c->name,autoShot);
    /* v31.19: the lower screen too, on each case's first shot. */
    if(!autoShot||caseMap){char bpath[160];snprintf(bpath,sizeof(bpath),AUTOTEST_DIR "/%s_%02d_bottom.bmp",c->name,autoShot);captureScreen(bottom,bpath,320);}
    if(!captureTopScreen(top,path)&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-80)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,"%s: capture %d failed\n",c->name,autoShot);
    /* v31.7: per-shot scene log; bad = ranges whose memory differs from
       what the CPU wrote. */
    {RendererWorldStats w=rendererVerifyWorld();snprintf(path,sizeof(path),AUTOTEST_DIR "/%s_scene.csv",c->name);
     FILE *log=fopen(path,autoShot?"a":"w");
     if(log){if(!autoShot)fputs("shot,t,builds,active,vertices,ranges,visible,bad_ranges,first_bad,x,y,agl,msl,cas,heading,stage,dest_km,photo,detailed,drops,sheds,cancels,far_drops,block_budget,block_slots,block_alloc,pool_fine,pool_coarse,lin_before_kb,lin_after_kb,lin_free_kb,heap_used_kb,heap_peak_kb,heap_size_kb,linear_size_kb,load_s,spikes,can_detail,weather,weather_level,wind_from,wind_kt\n",log);
      fprintf(log,"%d,%.2f,%d,%d,%d,%d,%d,%d,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%s,%.1f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.1f,%d,%d,%d,%.2f,%.0f,%.1f\n",autoShot,t,w.builds,w.active,w.vertices,w.ranges,w.visible,w.badRanges,w.firstBad,s.pos.x,s.pos.y,s.pos.z-terrain(s.pos.x,s.pos.y),
       s.pos.z,s.ias,s.yaw*57.29578f,autopilotOn?autopilotStageName(autopilot.stage):"-",autopilotOn?autopilotDistance(&autopilot,&s)/1000:-1.f,w.photo,w.detailed,w.drops,w.sheds,w.cancels,w.farDrops,w.block[0],w.block[1],w.block[2],w.pool[0],w.pool[1],w.pool[2],w.pool[3],w.linearFreeKB,(int)(mallinfo().uordblks/1024),(int)(mallinfo().arena/1024),(int)(__ctru_heap_size/1024),(int)(__ctru_linear_heap_size/1024),sceneryHoldTime,spikeTotal,sceneryWindowCanDetail(s.pos),s.weather,weatherLevel(&s),fmodf(atan2f(-s.wind.x,-s.wind.y)*180/PI+360,360),hypotf(s.wind.x,s.wind.y)*1.94384f);fclose(log);}}
    autoShot++;
    previous=osGetTime(); /* the save is not a game frame */
   }else if(autoShot>=c->shots){
    char path[160];snprintf(path,sizeof(path),AUTOTEST_DIR "/%s_profile.csv",c->name);saveFrameProfile(&frameProfile,path);
    snprintf(path,sizeof(path),AUTOTEST_DIR "/%s_spikes.csv",c->name);int caseSpikes=spikeCount;saveSpikes(path,0);
    FrameStats st=frameProfileStats(&frameProfile);
    if(autoSummaryLen<(int)AUTO_SUMMARY_SIZE-150)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,
     "%s: %d shots, %.1f FPS, p95 %.1f ms, worst %.1f ms, AGL %.0f m, load %.1f s, %d long frames, C-stick peak %d/%d (ignored)%s\n",c->name,c->shots,st.fps,st.p95Ms,st.maxMs,s.pos.z-terrain(s.pos.x,s.pos.y),sceneryHoldTime,caseSpikes,cstickPeakX,cstickPeakY,s.crashed?" (crashed)":flightEasyRescues?" (easy rescues)":"");
    cstickPeakX=cstickPeakY=0;
    {float lms,lverts,lcity;nightLightFrameStats(&lms,&lverts,&lcity);
     if(lverts>0&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-100)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,
      "%s: night lights %.1f ms/frame, %.0f vertices, %.0f city lights considered\n",c->name,lms,lverts,lcity);}
    {int bt,btri,bres;float bms;cityBuildingStats(&bt,&btri,&bres,&bms);
     if(bres>0&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-100)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,
      "%s: buildings %d tiles drawn, %d triangles, %d tiles resident\n",c->name,bt,btri,bres);}
    {int ld,ltri,lres;float lms;landmarkStats(&ld,&ltri,&lres,&lms);
     if(lres>0&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-100)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,
      "%s: landmarks %d drawn, %d triangles, %d built, last build %.0f ms\n",c->name,ld,ltri,lres,lms);}
    if(caseFrames&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-160)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,
     "%s: avg ms/frame %.1f = input+terrain %.1f, physics %.1f, gpu wait %.1f, draw %.1f, stream %.1f, scene build %.1f\n",c->name,
     caseSum[0]/caseFrames,caseSum[1]/caseFrames,caseSum[2]/caseFrames,caseSum[3]/caseFrames,caseSum[4]/caseFrames,caseSum[5]/caseFrames,caseSum[6]/caseFrames);
    int partHits,partMisses;sceneryPartCacheStats(&partHits,&partMisses);
    if(caseFrames&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-120)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,
     "%s: scene build ms/frame: mesh %.1f, sort %.1f, photo clip %.1f, upload %.1f, terrain parts reused %d of %d\n",c->name,caseStage[0]/caseFrames,caseStage[1]/caseFrames,caseStage[2]/caseFrames,caseStage[3]/caseFrames,partHits,partHits+partMisses);
     {char diag[160];if(sceneryStreamDiagnostics(diag,sizeof(diag))>0&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-200)
      autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,"%s: %s\n",c->name,diag);}
     {unsigned el=(unsigned)((osGetTime()-autoRunStart)/1000),dur=(unsigned)((osGetTime()-autoCaseStart)/1000);
      if(autoSummaryLen<(int)AUTO_SUMMARY_SIZE-80)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,"%s: case took %u s, run time %u:%02u\n",c->name,dur,el/60,el%60);}
     if(lidServices&&(lidCloses||!lidOpen)&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-100)
      autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,"%s: lid %s, closed %d times, %.0f s closed so far\n",c->name,lidOpen?"open":"closed",lidCloses,lidClosedSeconds);
    if(autopilotOn&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-120)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,
     "%s: route to %s, autopilot %s, %.1f km from it\n",c->name,airports[c->destination].id,autopilotStageName(autopilot.stage),autopilotDistance(&autopilot,&s)/1000);
    /* v1.1p15: how the landing went */
    if(autopilotOn&&c->routeLand&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-200){const Airport *rw=&airports[autopilot.runway];float h=autopilot.runwayHeading,rx=s.pos.x-rw->east,ry=s.pos.y-rw->north;
     autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,"%s: landing runway %s, touchdown sink %.2f m/s, %s, %.0f m along from runway centre, %.1f m off centreline, %.0f kt ground speed%s\n",
      c->name,fabsf(wrapPi(h-rw->heading*PI/180))<.1f?rw->le:rw->he,s.touchdownSink,s.onGround?"on the ground":"airborne",rx*sinf(h)+ry*cosf(h),rx*cosf(h)-ry*sinf(h),hypotf(s.vel.x,s.vel.y)*1.94384f,s.crashed?" (crashed)":"");}
    {const DynStats *d=&dynStats;const char *law=fbwStatus(&s);
     if(autoSummaryLen<(int)AUTO_SUMMARY_SIZE-260)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,
      "%s: dyn%s%s nz %.2f/%.2f alpha %.1f cas %.0f-%.0f bank %.0f pitch %.0f/%.0f p %.0f q %.0f beta %.1f alt %+.0f/%+.0f end %+.0f agl %.0f stall %.1f s prot %.1f s%s%s\n",c->name,
      maneuverOn?" ":"",maneuverOn?maneuverName(maneuver.kind):"",d->minNz,d->maxNz,d->maxAlpha*57.29578f,d->minCas,d->maxCas,d->maxBank*57.29578f,d->minPitch*57.29578f,d->maxPitch*57.29578f,
      d->maxRollRate*57.29578f,d->maxPitchRate*57.29578f,d->maxBeta*57.29578f,d->minAlt-d->startAlt,d->maxAlt-d->startAlt,s.pos.z-d->startAlt,d->minAgl<1e8f?d->minAgl:-1,d->stallTime,d->protTime,law?", ":"",law?law:"");
     if(maneuverOn&&*maneuverReport(&maneuver)&&autoSummaryLen<(int)AUTO_SUMMARY_SIZE-140)autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,
      "%s: %s %s, max yaw rate %.0f deg/s\n",c->name,maneuverName(maneuver.kind),maneuverReport(&maneuver),d->maxYawRate*57.29578f);
     snprintf(path,sizeof(path),AUTOTEST_DIR "/%s_dyn.csv",c->name);saveDynTrace(path);}
    /* v1.1p12: a long run keeps its results so far in partial.txt (every 10 cases), so a crash or a stop late in it loses little */
    if(autoIndex+1<autoCount&&(autoIndex+1)%10==0){FILE *pf=fopen(AUTOTEST_DIR "/partial.txt","w");if(pf){fputs(autoSummary,pf);fclose(pf);}}
    if(++autoIndex<autoCount){autoShot=0;autoCaseStart=osGetTime();startAutotestCase(&autoCases[autoIndex],&s,&menu,&paused);autoClock=0;previous=osGetTime();}
    else{
     {unsigned el=(unsigned)((osGetTime()-autoRunStart)/1000);if(autoSummaryLen<(int)AUTO_SUMMARY_SIZE-60)
      autoSummaryLen+=snprintf(autoSummary+autoSummaryLen,AUTO_SUMMARY_SIZE-autoSummaryLen,"run complete: %d cases in %u:%02u\n",autoCount,el/60,el%60);}
     FILE *f=fopen(AUTOTEST_DIR "/done.txt","w");if(f){fputs(autoSummary,f);fclose(f);}
     lidWatchStop();aptSetSleepAllowed(true);
     /* v35.15: a run started from the title menu goes back to it; one started by autotest.go quits as before. */
     if(autoFromMenu){
      autoIndex=-1;autoCount=-autoCount;autoFromMenu=0;flightLevel=menu.level;menu.screen=MENU_TITLE;menu.row=0;paused=0;
      snprintf(menuNotice,sizeof(menuNotice),"Autotest done: results in 3ds/3DMicroFlight/autotest");
     }else break;
    }
   }
  }
 }
 lidWatchStop(); /* also when the run is left through HOME */
 freeFlightAudio();
 if(extendedInput)irrstExit();
 if(spikeCount&&autoIndex<0){mkdir("sdmc:/3ds/3DMicroFlight/logs",0777);saveSpikes("sdmc:/3ds/3DMicroFlight/logs/spikes.csv",1);}
 saveFlightRecording();freeAircraftRenderer();
 pixelFontFree();C2D_TextBufDelete(texts);C2D_Fini();C3D_Fini();gfxExit();return 0;
}

