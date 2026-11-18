#include "cockpit.h"
#define PI 3.14159265359f
static CockpitFace *out;static int count,limit;
/* Citro2D packed ABGR color. */
static uint32_t rgb(int r,int g,int b){return (uint32_t)r|((uint32_t)g<<8)|((uint32_t)b<<16)|0xff000000u;}
static Vec add(Vec a,Vec b){return (Vec){a.x+b.x,a.y+b.y,a.z+b.z};}
static Vec sub(Vec a,Vec b){return (Vec){a.x-b.x,a.y-b.y,a.z-b.z};}
static Vec scale(Vec a,float k){return (Vec){a.x*k,a.y*k,a.z*k};}
static Vec cross(Vec a,Vec b){return (Vec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static void tri(Vec a,Vec b,Vec c,uint32_t color){if(count<limit)out[count++]=(CockpitFace){{a,b,c},color};}
static void quad(Vec a,Vec b,Vec c,Vec d,uint32_t color){tri(a,b,c,color);tri(a,c,d,color);}
static void box(Vec a,Vec b,uint32_t color){
 Vec p[8];for(int i=0;i<8;i++)p[i]=(Vec){i&1?b.x:a.x,i&2?b.y:a.y,i&4?b.z:a.z};
 quad(p[0],p[1],p[3],p[2],color);quad(p[4],p[6],p[7],p[5],color);
 quad(p[0],p[4],p[5],p[1],color);quad(p[2],p[3],p[7],p[6],color);
 quad(p[0],p[2],p[6],p[4],color);quad(p[1],p[5],p[7],p[3],color);
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
  beam((Vec){.990f,y,z},(Vec){.990f,y+sinf(angle)*radius*.54f,z-cosf(angle)*radius*.54f},.002f,rgb(243,233,191));
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
static void pfd(const Flight *s,float y,float width){
 float x=1.004f,z=.175f,h=.235f;uint32_t ink=rgb(8,14,23),white=rgb(238,246,248),yellow=rgb(255,211,59);
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
 float x=1.004f,z=.175f,h=.235f;
 screenRect(x+.008f,y-.018f,z-.018f,width+.036f,h+.036f,rgb(10,12,16));
 screenRect(x,y,z,width,h,rgb(37,57,46));
 for(int i=1;i<5;i++){screenRect(x-.002f,y+width*i/5,z,.0015f,h,rgb(58,77,62));screenRect(x-.002f,y,z+h*i/5,width,.0015f,rgb(58,77,62));}
 /* Selected-airport bearing cue: schematic, not a second terrain map. */
 float east=airports[s->airport].east-s->pos.x,north=airports[s->airport].north-s->pos.y;
 float angle=atan2f(east,north)-s->yaw,dy=sinf(angle)*.08f,dz=-cosf(angle)*.08f;
 beam((Vec){x-.004f,y+width*.5f,z+h*.5f},(Vec){x-.004f,y+width*.5f+dy,z+h*.5f+dz},.002f,rgb(239,97,229));
 tri((Vec){x-.009f,y+width*.5f,z+h*.5f-.014f},(Vec){x-.009f,y+width*.5f-.008f,z+h*.5f+.01f},(Vec){x-.009f,y+width*.5f+.008f,z+h*.5f+.01f},rgb(244,244,235));
}
int buildCockpit(const Flight *s,CockpitFace *faces,int capacity){
 out=faces;count=0;limit=capacity;if(!faces||capacity<=0)return 0;
 uint32_t trim=rgb(177,173,160),dark=rgb(29,33,38),panel=rgb(48,53,59),seat=rgb(115,102,83);
 float right=s->aircraft?1.40f:1.22f;
 /* Window openings remain empty. Shell is behind, beside and above the eye. */
 box((Vec){-1.45f,-.65f,-.76f},(Vec){1.05f,right,-.69f},trim);
 box((Vec){-1.45f,-.67f,.30f},(Vec){1.1f,-.60f,.88f},panel);
 box((Vec){-1.45f,right-.06f,.30f},(Vec){1.1f,right,.88f},panel);
 box((Vec){-1.35f,-.62f,.87f},(Vec){1.1f,right,.93f},dark);
 for(int side=0;side<2;side++){
  float y=side?right-.02f:-.63f;
  beam((Vec){.25f,y,-.69f},(Vec){1.38f,y,.12f},.025f,trim);
  beam((Vec){-.72f,y,-.69f},(Vec){-.72f,y,.32f},.035f,trim);
  beam((Vec){-.72f,y,.31f},(Vec){1.38f,y,.12f},.026f,trim);
  box((Vec){-.65f,y-.018f,.39f},(Vec){-.15f,y+.018f,.47f},seat);
 }
 beam((Vec){1.05f,-.63f,-.69f},(Vec){1.05f,right,-.69f},.035f,trim);
 shapedPanel(right,s->aircraft);
 box((Vec){.22f,.48f,.56f},(Vec){1.02f,.75f,.91f},panel);
 box((Vec){.58f,.57f,.42f},(Vec){.63f,.64f,.57f},dark);
 box((Vec){.55f,.55f,.40f},(Vec){.66f,.66f,.43f},rgb(211,214,214));
 beam((Vec){.0f,-.48f,.48f},(Vec){.13f,-.48f,.25f},.022f,dark);
 box((Vec){.10f,-.51f,.22f},(Vec){.18f,-.45f,.29f},dark);
 box((Vec){-.65f,-.40f,.34f},(Vec){-.35f,.25f,.81f},seat);
 box((Vec){-.65f,.72f,.34f},(Vec){-.35f,right-.09f,.81f},seat);
 if(s->aircraft){
  /* Early Perspective Touch: two large screens, three lower controllers. */
  pfd(s,-.40f,.66f);mapScreen(s,.36f,.66f);
  controller(-.37f,.455f,.29f);controller(.03f,.455f,.29f);controller(.43f,.455f,.29f);
  dial(-.54f,.27f,.047f,0,1);dial(1.17f,.27f,.047f,0,1);
  controls(.40f,.64f,4,2,.032f);
  beam((Vec){1.40f,.34f,.095f},(Vec){1.08f,.34f,-.69f},.015f,dark);
  for(int i=0;i<4;i++)screenRect(.998f,1.08f,.35f+i*.045f,.085f,.022f,i==1?rgb(73,133,81):dark);
 }else{
  /* SR22 Perspective/G1000-era two-screen panel and center control stack. */
  pfd(s,-.40f,.50f);mapScreen(s,.25f,.50f);
  dial(-.53f,.27f,.045f,0,1);dial(.91f,.27f,.045f,0,1);
  dial(-.31f,.49f,.041f,s->roll,0);dial(-.18f,.49f,.041f,s->ias*.04f,0);dial(-.05f,.49f,.041f,s->pos.z*.003f,0);
  controls(.28f,.465f,5,2,.039f);controls(.40f,.61f,4,4,.034f);
  disc(.991f,.16f,.29f,.015f,8,rgb(131,135,139));disc(.991f,.16f,.37f,.015f,8,rgb(131,135,139));
 }
 if(s->aircraft){box((Vec){.94f,.78f,.46f},(Vec){1.0f,.83f,.53f},rgb(221,227,229));}
 return count;
}
