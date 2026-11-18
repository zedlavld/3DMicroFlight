#include "cockpit.h"
#define PI 3.14159265359f
static CockpitFace *out;static int count,limit;
/* Citro2D packed ABGR color. */
static uint32_t rgb(int r,int g,int b){return (uint32_t)r|((uint32_t)g<<8)|((uint32_t)b<<16)|0xff000000u;}
static Vec add(Vec a,Vec b){return (Vec){a.x+b.x,a.y+b.y,a.z+b.z};}
static Vec sub(Vec a,Vec b){return (Vec){a.x-b.x,a.y-b.y,a.z-b.z};}
static Vec scale(Vec a,float k){return (Vec){a.x*k,a.y*k,a.z*k};}
static Vec cross(Vec a,Vec b){return (Vec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
/* v34.03 over-the-nose view. The panel was authored with its glare-shield
   lip about 4 degrees under the eye (compass top about 1 degree), which hid
   the ground on every approach. Everything below eye level (z > 0) is now
   lowered by a per-aircraft drop, and the default view tilts down a little
   so the displays stay on screen. Lip angles with the drop: SR22 ~9,
   SF50 ~10, 737/A321 ~14, F-16 ~16 degrees (ref: roughly 10-12 for light
   aircraft, 15-20 for airliners and fighters). */
static const float panelDrop[AIRCRAFT_COUNT]={.08f,.10f,.165f,.165f,.20f};
static const float viewTilt[AIRCRAFT_COUNT]={3,4,5,5,5};
float cockpitViewTilt(int type){return type>=0&&type<AIRCRAFT_COUNT?viewTilt[type]*PI/180:0;}
static float drop;static int keepZ; /* keepZ: HUD parts authored at their true height */
static Vec lowered(Vec p){if(p.z>0&&!keepZ)p.z+=drop;return p;}
static void tri(Vec a,Vec b,Vec c,uint32_t color){if(count<limit)out[count++]=(CockpitFace){{lowered(a),lowered(b),lowered(c)},color};}
static void quad(Vec a,Vec b,Vec c,Vec d,uint32_t color){tri(a,b,c,color);tri(a,c,d,color);}
static void box(Vec a,Vec b,uint32_t color){
 Vec p[8];for(int i=0;i<8;i++)p[i]=(Vec){i&1?b.x:a.x,i&2?b.y:a.y,i&4?b.z:a.z};
 quad(p[0],p[1],p[3],p[2],color);quad(p[4],p[6],p[7],p[5],color);
 quad(p[0],p[4],p[5],p[1],color);quad(p[2],p[3],p[7],p[6],color);
 quad(p[0],p[2],p[6],p[4],color);quad(p[1],p[5],p[7],p[3],color);
}
/* Box from eight corners (index bits: x, y, z), for tilted shapes. */
static void prism(const Vec p[8],uint32_t color){
 quad(p[0],p[1],p[3],p[2],color);quad(p[4],p[6],p[7],p[5],color);
 quad(p[0],p[4],p[5],p[1],color);quad(p[2],p[3],p[7],p[6],color);
 quad(p[0],p[2],p[6],p[4],color);quad(p[1],p[5],p[7],p[3],color);
}
/* v31.19: window sill line (centre of the sill beam) at station x. */
static float sillZ(float x){return .31f+(x+.72f)*(.12f-.31f)/2.10f;}
/* Inner face of a door/side wall at y, its top edge 1 cm under the sill
   line so the sill beam always overlaps it (was a flat top at z .30, which
   left a wedge of scenery under the sloping sill). Two-tone with an
   armrest ledge stepping inward (inward = +1 left wall, -1 right wall). */
static void sideWall(float y,float inward,uint32_t upper,uint32_t lower,uint32_t ledge){
 const float x0=-1.45f,x1=-.72f,x2=1.10f;float z0=.30f,z1=sillZ(x1)-.01f,z2=sillZ(x2)-.01f;
 /* aft of the door post: flat top under the rear window */
 quad((Vec){x0,y,z0},(Vec){x1,y,z1},(Vec){x1,y,.50f},(Vec){x0,y,.50f},upper);
 quad((Vec){x0,y,.50f},(Vec){x1,y,.50f},(Vec){x1,y,.88f},(Vec){x0,y,.88f},lower);
 /* door: upper trim band follows the sill, armrest ledge, dark lower panel */
 quad((Vec){x1,y,z1},(Vec){x2,y,z2},(Vec){x2,y,z2+.16f},(Vec){x1,y,z1+.16f},upper);
 float ya=y+inward*.055f,xa=x1+.25f,xb=x2-.35f;
 float za=z1+.17f+(xa-x1)*(z2-z1)/(x2-x1),zb=z1+.17f+(xb-x1)*(z2-z1)/(x2-x1);
 quad((Vec){xa,y,za},(Vec){xb,y,zb},(Vec){xb,ya,zb},(Vec){xa,ya,za},ledge);
 quad((Vec){xa,ya,za},(Vec){xb,ya,zb},(Vec){xb,ya,zb+.03f},(Vec){xa,ya,za+.03f},lower);
 quad((Vec){x1,y,z1+.16f},(Vec){x2,y,z2+.16f},(Vec){x2,y,.88f},(Vec){x1,y,.88f},lower);
}
/* Seat across y0..y1: cushion, reclined back, headrest. */
static void seatShape(float y0,float y1,uint32_t cushion,uint32_t back){
 box((Vec){-.62f,y0,.56f},(Vec){.02f,y1,.70f},cushion);
 Vec p[8];for(int i=0;i<8;i++){int top=!(i&4);float x=(i&1)?(top?-.68f:-.54f):(top?-.80f:-.66f);p[i]=(Vec){x,(i&2)?y1:y0,top?-.12f:.58f};}
 prism(p,back);
 box((Vec){-.86f,y0+.07f,-.32f},(Vec){-.74f,y1-.07f,-.11f},back);
}
static void beam(Vec a,Vec b,float radius,uint32_t color){
 Vec d=sub(b,a),u=cross(d,(Vec){0,1,0});float len=sqrtf(u.x*u.x+u.y*u.y+u.z*u.z);
 if(len<.001f){u=cross(d,(Vec){0,0,1});len=sqrtf(u.x*u.x+u.y*u.y+u.z*u.z);}
 if(len<.001f)return;
 u=scale(u,radius/len);Vec v=cross(d,u);float n=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);v=scale(v,radius/n);
 Vec ring[4]={add(u,v),sub(v,u),scale(add(u,v),-1),sub(u,v)};
 for(int i=0;i<4;i++)quad(add(a,ring[i]),add(b,ring[i]),add(b,ring[(i+1)%4]),add(a,ring[(i+1)%4]),color);
 quad(add(a,ring[0]),add(a,ring[1]),add(a,ring[2]),add(a,ring[3]),color);
 quad(add(b,ring[3]),add(b,ring[2]),add(b,ring[1]),add(b,ring[0]),color);
}
/* Flat instrument faces lie just ahead of the panel towards the pilot. */
static void screenRect(float x,float y,float z,float w,float h,uint32_t c){quad((Vec){x,y,z},(Vec){x,y+w,z},(Vec){x,y+w,z+h},(Vec){x,y,z+h},c);}
static void disc(float x,float y,float z,float radius,int steps,uint32_t color){
 for(int i=0;i<steps;i++){
  float a=2*PI*i/steps,b=2*PI*(i+1)/steps;
  tri((Vec){x,y,z},(Vec){x,y+cosf(a)*radius,z+sinf(a)*radius},(Vec){x,y+cosf(b)*radius,z+sinf(b)*radius},color);
 }
}
static void dial(float y,float z,float radius,float angle,int vent){
 disc(.998f,y,z,radius,12,rgb(132,140,143));disc(.995f,y,z,radius*.78f,12,rgb(13,19,24));
 if(vent){for(int i=-1;i<=1;i++)screenRect(.992f,y-radius*.50f,z+radius*i*.32f,radius, .003f,rgb(78,84,89));}
 else {
  for(int i=0;i<8;i++){float a=i*PI/4;screenRect(.992f,y+cosf(a)*radius*.56f-.002f,z+sinf(a)*radius*.56f-.002f,.004f,.004f,rgb(224,226,214));}
  if(isfinite(angle))beam((Vec){.990f,y,z},(Vec){.990f,y+sinf(angle)*radius*.54f,z-cosf(angle)*radius*.54f},.002f,rgb(243,233,191));
 }
}
static void controls(float y,float z,int cols,int rows,float step){
 screenRect(1.002f,y-.014f,z-.014f,cols*step+.014f,rows*step+.014f,rgb(12,17,22));
 for(int row=0;row<rows;row++)for(int col=0;col<cols;col++)screenRect(.998f,y+col*step,z+row*step,step*.67f,step*.53f,rgb(118,127,132));
}
static void controller(float y,float z,float width){
 screenRect(1.002f,y-.012f,z-.012f,width+.024f,.12f,rgb(14,17,22));
 screenRect(.998f,y,z,width,.085f,rgb(24,42,52));
 for(int row=0;row<2;row++)for(int col=0;col<3;col++)screenRect(.995f,y+.012f+col*width*.32f,z+.01f+row*.035f,width*.23f,.023f,rgb(56,115,134));
 disc(.994f,y+width*.2f,z+.103f,.008f,8,rgb(134,140,142));disc(.994f,y+width*.8f,z+.103f,.008f,8,rgb(134,140,142));
}
static void shapedPanel(float right,int jet){
 /* Curved shoulder/glare-shield strips fitted visually to photo references. */
 for(int i=0;i<16;i++){
  float a=-.65f+(right+.65f)*i/16,b=-.65f+(right+.65f)*(i+1)/16;
  float u=(a-(right-.65f)/2)/((right+.65f)/2),v=(b-(right-.65f)/2)/((right+.65f)/2);
  float za=.095f+.075f*u*u,zb=.095f+.075f*v*v;
  quad((Vec){1.02f,a,za},(Vec){1.02f,b,zb},(Vec){1.02f,b,.59f},(Vec){1.02f,a,.59f},rgb(jet?54:45,jet?58:49,jet?62:53));
  quad((Vec){.94f,a,za-.025f},(Vec){.94f,b,zb-.025f},(Vec){1.20f,b,zb-.012f},(Vec){1.20f,a,za-.012f},rgb(22,25,29));
  quad((Vec){.94f,a,za-.025f},(Vec){1.02f,a,za+.012f},(Vec){1.02f,b,zb+.012f},(Vec){.94f,b,zb-.025f},rgb(33,37,42));
 }
}
static void segment(float x,float y,float z,float w,float h,int digit,uint32_t c){
 static const unsigned char mask[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
 float t=w*.15f;unsigned m=mask[digit%10];
 if(m&1)screenRect(x,y+t,z,w-2*t,t,c);
 if(m&2)screenRect(x,y+w-t,z+t,t,h*.5f-1.5f*t,c);
 if(m&4)screenRect(x,y+w-t,z+h*.5f+t*.5f,t,h*.5f-1.5f*t,c);
 if(m&8)screenRect(x,y+t,z+h-t,w-2*t,t,c);
 if(m&16)screenRect(x,y,z+h*.5f+t*.5f,t,h*.5f-1.5f*t,c);
 if(m&32)screenRect(x,y,z+t,t,h*.5f-1.5f*t,c);
 if(m&64)screenRect(x,y+t,z+h*.5f-t*.5f,w-2*t,t,c);
}
static void digits(float x,float y,float z,int value,int places,float size,uint32_t c){
 value=value<0?0:value;int div=1;for(int i=1;i<places;i++)div*=10;
 for(int i=0;i<places;i++){segment(x,y+i*size*1.25f,z,size,size*1.65f,(value/div)%10,c);div/=10;}
}
static float clampf01(float x){return x<0?0:x>1?1:x;}
static float screenZ=.175f,screenH=.235f;
static void pfd(const Flight *s,float y,float width){
 float x=1.004f,z=screenZ,h=screenH;uint32_t ink=rgb(8,14,23),white=rgb(238,246,248),yellow=rgb(255,211,59);
 screenRect(x+.008f,y-.018f,z-.018f,width+.036f,h+.036f,rgb(10,12,16));
 screenRect(x,y,z,width,h,rgb(34,107,163));
 /* Clip the attitude horizon in the screen plane before triangulation. */
 float cr=cosf(s->roll),sr=sinf(s->roll),offset=clampf(s->pitch,-.7f,.7f)*h*.85f;
 Vec a[8]={{0,0,0},{0,width,0},{0,width,h},{0,0,h}},b[8];int n=4,m=0;
 for(int i=0;i<n;i++){
  Vec p=a[i],q=a[(i+1)%n];float dp=(p.z-h*.5f-offset)*cr+(p.y-width*.5f)*sr,dq=(q.z-h*.5f-offset)*cr+(q.y-width*.5f)*sr;
  if(dp>=0)b[m++]=p;
  if((dp>=0)!=(dq>=0)){float t=dp/(dp-dq);b[m++]=(Vec){0,p.y+t*(q.y-p.y),p.z+t*(q.z-p.z)};}
 }
 for(int i=1;i<m-1;i++)tri((Vec){x-.001f,y+b[0].y,z+b[0].z},(Vec){x-.001f,y+b[i].y,z+b[i].z},(Vec){x-.001f,y+b[i+1].y,z+b[i+1].z},rgb(137,84,43));
 screenRect(x-.003f,y,z,width*.19f,h,ink);screenRect(x-.003f,y+width*.76f,z,width*.24f,h,ink);
 screenRect(x-.005f,y+width*.36f,z+h*.49f,width*.11f,.006f,yellow);
 screenRect(x-.005f,y+width*.53f,z+h*.49f,width*.11f,.006f,yellow);
 digits(x-.007f,y+.009f,z+.102f,(int)fmaxf(s->ias,0),3,.014f,white);
 digits(x-.007f,y+width*.78f,z+.102f,(int)fmaxf(s->pos.z*3.28084f,0),5,.009f,white);
 screenRect(x-.004f,y,z+h-.033f,width,.033f,ink);
 float heading=fmodf(s->yaw*180/PI+360,360);digits(x-.007f,y+width*.43f,z+h-.027f,(int)heading,3,.012f,rgb(88,221,240));
}
static void mapScreen(const Flight *s,float y,float width){
 float x=1.004f,z=screenZ,h=screenH;
 screenRect(x+.008f,y-.018f,z-.018f,width+.036f,h+.036f,rgb(10,12,16));
 screenRect(x,y,z,width,h,rgb(37,57,46));
 for(int i=1;i<5;i++){screenRect(x-.002f,y+width*i/5,z,.0015f,h,rgb(58,77,62));screenRect(x-.002f,y,z+h*i/5,width,.0015f,rgb(58,77,62));}
 /* Selected-airport bearing cue: schematic, not a second terrain map. */
 float east=airports[s->airport].east-s->pos.x,north=airports[s->airport].north-s->pos.y;
 float angle=atan2f(east,north)-s->yaw,dy=sinf(angle)*.08f,dz=-cosf(angle)*.08f;
 beam((Vec){x-.004f,y+width*.5f,z+h*.5f},(Vec){x-.004f,y+width*.5f+dy,z+h*.5f+dz},.002f,rgb(239,97,229));
 tri((Vec){x-.009f,y+width*.5f,z+h*.5f-.014f},(Vec){x-.009f,y+width*.5f-.008f,z+h*.5f+.01f},(Vec){x-.009f,y+width*.5f+.008f,z+h*.5f+.01f},rgb(244,244,235));
}
/* v34.03b airliner and fighter cockpits: simple shapes, no imported
   interiors. Airliners: captain on the left at y = 0, first officer
   mirrored about the centre line yc. Parts the pilot should see from the
   default view (yokes, thrust levers) are authored higher than life
   because everything below eye level is lowered by the panel drop. */
static float mirror(float yc,float y,float w){return 2*yc-y-w;}
static void screenFrame(float y,float z,float w,float h,uint32_t face){
 screenRect(1.012f,y-.016f,z-.016f,w+.032f,h+.032f,rgb(10,12,16));screenRect(1.004f,y,z,w,h,face);
}
/* Boeing MCP / Airbus FCU hanging under the glare-shield lip. */
static void glareshieldUnit(float y0,float y1,int airbus){
 uint32_t body=airbus?rgb(56,62,72):rgb(74,76,78),win=rgb(18,20,22);
 box((Vec){.925f,y0,.07f},(Vec){.945f,y1,.125f},body);
 int n=airbus?5:7;
 for(int i=0;i<n;i++){float y=y0+.04f+(y1-y0-.08f)*i/(n-1);
  screenRect(.922f,y-.022f,.080f,.044f,.016f,win);
  screenRect(.921f,y-.016f,.084f,.032f,.008f,airbus?rgb(255,160,40):rgb(255,190,60));
  disc(.921f,y,.110f,.007f,6,rgb(170,172,170));}
}
static void yoke(float y){
 uint32_t c=rgb(52,48,44),grip=rgb(30,30,32);
 beam((Vec){.66f,y,.95f},(Vec){.56f,y,.20f},.022f,c);
 beam((Vec){.555f,y-.13f,.19f},(Vec){.555f,y+.13f,.19f},.016f,grip);
 beam((Vec){.555f,y-.13f,.19f},(Vec){.55f,y-.14f,.13f},.016f,grip);
 beam((Vec){.555f,y+.13f,.19f},(Vec){.55f,y+.14f,.13f},.016f,grip);
 screenRect(.54f,y-.03f,.17f,.06f,.035f,rgb(150,40,30));
}
static void sideConsole(float y0,float y1,int stick){
 box((Vec){-.15f,y0,.40f},(Vec){.70f,y1,.92f},rgb(44,48,54));
 if(stick){float y=(y0+y1)/2;
  beam((Vec){.42f,y,.40f},(Vec){.40f,y,.29f},.016f,rgb(28,30,32));
  box((Vec){.37f,y-.022f,.25f},(Vec){.43f,y+.022f,.30f},rgb(36,38,40));}
}
static void airlinerStatic(float right,int airbus){
 float yc=(right-.65f)/2;
 glareshieldUnit(-.10f,2*yc+.10f,airbus);
 /* Engine/system screens: Boeing upper + lower EICAS, Airbus E/WD + SD. */
 screenFrame(yc-.095f,.175f,.19f,.20f,rgb(6,8,12));
 screenFrame(yc-.095f,.42f,.19f,airbus?.19f:.15f,rgb(6,8,12));
 if(airbus){sideConsole(-.62f,-.40f,1);sideConsole(right-.24f,right-.02f,1);}
 else {yoke(0);yoke(2*yc);sideConsole(-.62f,-.46f,0);sideConsole(right-.18f,right-.02f,0);}
 /* Centre pedestal (thrust levers are drawn live). */
 box((Vec){.30f,yc-.14f,.40f},(Vec){.92f,yc+.14f,.92f},rgb(44,48,54));
}
/* v34.04 F-16 tub: frameless bubble canopy (only the bow behind the
   seat), low canopy rails about 42 cm under the eye so the pilot sees down
   past the sides, plain tub walls under them. Authored at true height. */
static void fighterShell(float right){
 uint32_t rail=rgb(64,68,70),wall=rgb(52,56,58),bow=rgb(46,50,52);
 float yl=-.60f,yr=right-.02f,yc=(yl+yr)/2,a=(yr-yl)/2;
 keepZ=1;
 for(int side=0;side<2;side++){float y=side?yr:yl;
  beam((Vec){-1.30f,y,.42f},(Vec){.95f,y,.42f},.03f,rail);
  quad((Vec){-1.30f,y,.43f},(Vec){.95f,y,.43f},(Vec){.95f,y,.90f},(Vec){-1.30f,y,.90f},wall);}
 /* Canopy bow: an arch over the head just behind the seat back. */
 Vec prev=(Vec){-1.0f,yl,.42f};
 for(int i=1;i<=10;i++){float t=PI*i/10;Vec p={-1.0f,yc-a*cosf(t),.42f-.74f*sinf(t)};beam(prev,p,.035f,bow);prev=p;}
 /* Rear deck behind the bow up to rail height. */
 quad((Vec){-1.30f,yl,.42f},(Vec){-1.30f,yr,.42f},(Vec){-1.02f,yr,.42f},(Vec){-1.02f,yl,.42f},wall);
 keepZ=0;
}
static void fighterStatic(float right){
 /* Single centred seat, side consoles: stick on the right, throttle left. */
 sideConsole(-.60f,-.34f,0);sideConsole(.34f,right-.04f,1);
 /* Up-front controls under the HUD. */
 box((Vec){.93f,-.09f,.10f},(Vec){1.00f,.09f,.21f},rgb(40,44,48));
 for(int r=0;r<3;r++)for(int c=0;c<4;c++)screenRect(.928f,-.07f+c*.036f,.115f+r*.03f,.024f,.018f,rgb(110,118,122));
 /* HUD base and combiner frame at their true height. */
 keepZ=1;
 box((Vec){.86f,-.085f,.17f},(Vec){.95f,.085f,.31f},rgb(30,33,36));
 beam((Vec){.80f,-.078f,.135f},(Vec){.80f,.078f,.135f},.004f,rgb(60,64,66));
 beam((Vec){.80f,-.06f,.135f},(Vec){.87f,-.06f,.17f},.004f,rgb(60,64,66));beam((Vec){.80f,.06f,.135f},(Vec){.87f,.06f,.17f},.004f,rgb(60,64,66));
 beam((Vec){.80f,-.078f,.135f},(Vec){.80f,-.078f,-.02f},.004f,rgb(60,64,66));
 beam((Vec){.80f,.078f,.135f},(Vec){.80f,.078f,-.02f},.004f,rgb(60,64,66));
 beam((Vec){.80f,-.078f,-.02f},(Vec){.80f,.078f,-.02f},.004f,rgb(60,64,66));
 keepZ=0;
}
int buildCockpitStatic(int type,CockpitFace *faces,int capacity){
 int style=aircraft[type].cockpitStyle;
 out=faces;count=0;limit=capacity;if(!faces||capacity<=0)return 0;
 drop=type>=0&&type<AIRCRAFT_COUNT?panelDrop[type]:0;
 uint32_t trim=rgb(177,173,160),dark=rgb(29,33,38),panel=rgb(48,53,59),seat=rgb(115,102,83);
 float right=aircraft[type].cockpitRight;
 /* Window openings remain empty. Shell is behind, beside and above the eye. */
 if(style==4)fighterShell(right);
 else {
  box((Vec){-1.45f,-.65f,-.76f},(Vec){1.05f,right,-.69f},trim);
  uint32_t upper=rgb(style?150:140,style?148:136,style?142:126),lower=rgb(40,44,50),ledge=rgb(96,98,100);
  sideWall(-.60f,1,upper,lower,ledge);sideWall(right-.06f,-1,upper,lower,ledge);
  for(int side=0;side<2;side++){
   float y=side?right-.02f:-.63f;
   beam((Vec){.25f,y,-.69f},(Vec){1.38f,y,.12f},.025f,trim);
   beam((Vec){-.72f,y,-.69f},(Vec){-.72f,y,.32f},.035f,trim);
   beam((Vec){-.72f,y,.31f},(Vec){1.38f,y,.12f},.026f,trim);
  }
  beam((Vec){1.05f,-.63f,-.69f},(Vec){1.05f,right,-.69f},.035f,trim);
 }
 box((Vec){-1.35f,-.62f,.87f},(Vec){1.1f,right,.93f},dark);
 shapedPanel(right,style);
 /* Close the footwell so terrain cannot show through under the panel. */
 quad((Vec){1.10f,-.65f,.55f},(Vec){1.10f,right,.55f},(Vec){1.10f,right,.91f},(Vec){1.10f,-.65f,.91f},rgb(26,29,34));
 if(style<=1){
  box((Vec){.22f,.48f,.56f},(Vec){1.02f,.75f,.91f},panel);
  box((Vec){.58f,.57f,.42f},(Vec){.63f,.64f,.57f},dark);
  box((Vec){.55f,.55f,.40f},(Vec){.66f,.66f,.43f},rgb(211,214,214));
  beam((Vec){.0f,-.48f,.48f},(Vec){.13f,-.48f,.25f},.022f,dark);
  box((Vec){.10f,-.51f,.22f},(Vec){.18f,-.45f,.29f},dark);
 }
 if(style==4)seatShape(-.28f,.28f,rgb(70,72,66),rgb(56,58,54));
 else {seatShape(-.40f,.25f,seat,rgb(88,78,64));seatShape(.72f,right-.09f,seat,rgb(88,78,64));}
 /* Magnetic compass on the glare shield, centred between the seats. */
 if(style!=4){float yc=(right-.65f)/2;box((Vec){1.10f,yc-.035f,.02f},(Vec){1.17f,yc+.035f,.075f},dark);screenRect(1.098f,yc-.022f,.03f,.044f,.026f,rgb(214,210,190));}
 if(style==2||style==3)airlinerStatic(right,style==3);
 else if(style==4)fighterStatic(right);
 else if(style){
  /* Early Perspective Touch: two large screens, three lower controllers. */
  controller(-.37f,.455f,.29f);controller(.03f,.455f,.29f);controller(.43f,.455f,.29f);
  dial(-.54f,.27f,.047f,0,1);dial(1.17f,.27f,.047f,0,1);
  controls(.40f,.64f,4,2,.032f);
  beam((Vec){1.40f,.34f,.095f},(Vec){1.08f,.34f,-.69f},.015f,dark);
  for(int i=0;i<4;i++)screenRect(.998f,1.08f,.35f+i*.045f,.085f,.022f,i==1?rgb(73,133,81):dark);
 }else{
  /* SR22 Perspective/G1000-era two-screen panel and center control stack. */
  dial(-.53f,.27f,.045f,0,1);dial(.91f,.27f,.045f,0,1);
  dial(-.31f,.49f,.041f,NAN,0);dial(-.18f,.49f,.041f,NAN,0);dial(-.05f,.49f,.041f,NAN,0);
  controls(.28f,.465f,5,2,.039f);controls(.40f,.61f,4,4,.034f);
  disc(.991f,.16f,.29f,.015f,8,rgb(131,135,139));disc(.991f,.16f,.37f,.015f,8,rgb(131,135,139));
 }
 if(style==1){box((Vec){.94f,.78f,.46f},(Vec){1.0f,.83f,.53f},rgb(221,227,229));}
 return count;
}

/* Engine gauge: dark dial, needle swept through 270 degrees by frac. */
static void gauge(float y,float z,float r,float frac,uint32_t needle){
 disc(1.0f,y,z,r,8,rgb(40,44,48));disc(.999f,y,z,r*.86f,8,rgb(6,8,12));
 float a=(-.75f+1.5f*clampf01(frac))*PI,w=r*.09f;
 tri((Vec){.997f,y-cosf(a)*w,z-sinf(a)*w},(Vec){.997f,y+sinf(a)*r*.8f,z-cosf(a)*r*.8f},(Vec){.997f,y+cosf(a)*w,z+sinf(a)*w},needle);
}
static void engineScreens(const Flight *s,float yc,int airbus){
 float n1=.2f+.8f*s->engine;uint32_t white=rgb(240,240,240),second=airbus?rgb(80,255,90):white;
 gauge(yc-.045f,.225f,.034f,n1,white);gauge(yc+.045f,.225f,.034f,n1,white);
 gauge(yc-.045f,.31f,.026f,.3f+.6f*s->engine,second);gauge(yc+.045f,.31f,.026f,.3f+.6f*s->engine,second);
 for(int i=0;i<3;i++)screenRect(1.0f,yc-.075f,.44f+i*.03f,.08f+.03f*(i&1),.008f,airbus?rgb(70,230,90):white);
 if(airbus)for(int i=0;i<2;i++)screenRect(1.0f,yc+.01f,.47f+i*.05f,.06f,.03f,rgb(40,120,200));
}
static void throttles(float yc,float t){
 for(int i=-1;i<=1;i+=2){float y=yc+i*.045f,x=.52f+.16f*t;
  beam((Vec){.62f,y,.40f},(Vec){x,y,.27f},.010f,rgb(150,152,150));
  box((Vec){x-.025f,y-.03f,.24f},(Vec){x+.02f,y+.03f,.28f},rgb(26,26,28));}
}
/* F-16 HUD: boresight, horizon and pitch ladder every 5 degrees, speed
   (kt), altitude (ft) and heading, drawn on the combiner plane. */
static void hud(const Flight *s){
 keepZ=1;const float d=.795f,ztop=-.012f,zbot=.13f;const uint32_t g=rgb(90,255,0);
 screenRect(d,-.018f,-.0025f,.012f,.005f,g);screenRect(d,.006f,-.0025f,.012f,.005f,g);
 float th=s->pitch,ph=s->roll,cth=cosf(th),cph=cosf(ph);
 if(cph>.25f)for(int k=-3;k<=3;k++){float e=k*5*PI/180,half=k?.035f:.065f;
  float base=d*(sinf(th)-sinf(e)),za=(base+sinf(ph)*cth*half)/(cph*cth),zb=(base-sinf(ph)*cth*half)/(cph*cth);
  if(za<ztop||za>zbot||zb<ztop||zb>zbot)continue;
  if(k){/* ladder rungs with a centre gap */
   beam((Vec){d,-half,za},(Vec){d,-half*.3f,za+(zb-za)*.35f},.0022f,g);
   beam((Vec){d,half*.3f,za+(zb-za)*.65f},(Vec){d,half,zb},.0022f,g);}
  else beam((Vec){d,-half,za},(Vec){d,half,zb},.0026f,g);
 }
 /* Flight path marker: where the aircraft is actually going (alpha below
    and beta beside the boresight), a small ring with wings. */
 {float fy=-d*tanf(s->beta),fz=d*tanf(s->alpha);
  if(fy>-.06f&&fy<.06f&&fz>ztop&&fz<zbot){const float r=.0055f;
   for(int i=0;i<6;i++){float a0=PI*i/3,a1=PI*(i+1)/3;beam((Vec){d,fy+r*cosf(a0),fz+r*sinf(a0)},(Vec){d,fy+r*cosf(a1),fz+r*sinf(a1)},.0016f,g);}
   beam((Vec){d,fy-r-.010f,fz},(Vec){d,fy-r,fz},.0018f,g);beam((Vec){d,fy+r,fz},(Vec){d,fy+r+.010f,fz},.0018f,g);
   beam((Vec){d,fy,fz-r},(Vec){d,fy,fz-r-.006f},.0018f,g);}}
 /* Load factor top left, one decimal. */
 {int gx=(int)lroundf(fminf(fmaxf(s->loadFactor,-9.9f),9.9f)*10);if(gx<0)gx=-gx;
  digits(d,-.071f,.002f,gx/10,1,.009f,g);screenRect(d,-.0585f,.015f,.003f,.003f,g);digits(d,-.054f,.002f,gx%10,1,.009f,g);}
 digits(d,-.071f,.03f,(int)fmaxf(s->ias*1.94384f,0),3,.011f,g);
 digits(d,.019f,.03f,(int)fmaxf(s->pos.z*3.28084f/10,0)*10,5,.0088f,g);
 digits(d,-.020f,.100f,(int)fmodf(s->yaw*180/PI+360,360),3,.011f,g);
 keepZ=0;
}
int buildCockpitDynamic(const Flight *s,CockpitFace *faces,int capacity){
 out=faces;count=0;limit=capacity;if(!faces||capacity<=0)return 0;
 drop=s->aircraft>=0&&s->aircraft<AIRCRAFT_COUNT?panelDrop[s->aircraft]:0;
 int style=aircraft[s->aircraft].cockpitStyle;float right=aircraft[s->aircraft].cockpitRight,yc=(right-.65f)/2;
 if(style==2||style==3){
  /* Captain PFD outboard, ND inboard; first officer mirrored. */
  screenZ=.175f;screenH=.20f;
  pfd(s,-.24f,.19f);mapScreen(s,-.02f,.19f);mapScreen(s,mirror(yc,-.02f,.19f),.19f);pfd(s,mirror(yc,-.24f,.19f),.19f);
  screenZ=.175f;screenH=.235f;
  engineScreens(s,yc,style==3);throttles(yc,s->throttle);
 }else if(style==4){
  screenZ=.22f;screenH=.17f;pfd(s,-.30f,.17f);mapScreen(s,.13f,.17f);screenZ=.175f;screenH=.235f;
  hud(s);
  float x=.18f+.22f*s->throttle;box((Vec){x-.04f,-.50f,.31f},(Vec){x+.03f,-.42f,.40f},rgb(30,32,34));
 }else if(style){pfd(s,-.40f,.66f);mapScreen(s,.36f,.66f);}
 else {
  pfd(s,-.40f,.50f);mapScreen(s,.25f,.50f);
  float y[3]={-.31f,-.18f,-.05f},angle[3]={s->roll,s->ias*.04f,s->pos.z*.003f};
  for(int i=0;i<3;i++)beam((Vec){.990f,y[i],.49f},(Vec){.990f,y[i]+sinf(angle[i])*.041f*.54f,.49f-cosf(angle[i])*.041f*.54f},.002f,rgb(243,233,191));
 }
 return count;
}
int buildCockpit(const Flight *s,CockpitFace *faces,int capacity){
 if(!faces||capacity<=0)return 0;
 int fixed=buildCockpitStatic(s->aircraft,faces,capacity);
 return fixed+buildCockpitDynamic(s,faces+fixed,capacity-fixed);
}
