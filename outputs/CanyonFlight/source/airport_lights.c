#include <math.h>
#include "airport_lights.h"
#include "terrain.h"
#include "scenery.h"
#include "daylight.h"
#include "airport_index.h"
#define PI 3.14159265359f
/* Runway frame: along = metres from the centre toward the heading end,
   across = metres to the right of the heading. */
typedef struct {const Airport *ap;float se,ce;} Frame;
static Vec at(const Frame *f,float along,float across,float z){
 return (Vec){f->ap->east+along*f->se+across*f->ce,f->ap->north+along*f->ce-across*f->se,z};
}
/* Approach-light ground heights: terrain() is too slow to call for every lamp
   every frame, so each station is cached and refreshed now and then (the
   elevation stream may still be loading finer tiles). */
/* v1.1p20: thousands of runways, so the heights live in a small cache of runway ends
   (least recently used slot reused), not a table per record. */
#define ALS_STATIONS 25
#define ALS_SLOTS 128
static float alsHeight[ALS_SLOTS][ALS_STATIONS];static unsigned char alsAge[ALS_SLOTS];
static int alsTag[ALS_SLOTS]={[0 ... ALS_SLOTS-1]=-1};static unsigned alsUsed[ALS_SLOTS],alsClock;
static int alsSlot(int index,int endIndex){
 int tag=index*2+endIndex,oldest=0;alsClock++;
 for(int k=0;k<ALS_SLOTS;k++){
  if(alsTag[k]==tag){alsUsed[k]=alsClock;return k;}
  if(alsUsed[k]<alsUsed[oldest])oldest=k;
 }
 alsTag[oldest]=tag;alsUsed[oldest]=alsClock;alsAge[oldest]=0;   /* new end: measure on first use */
 return oldest;
}
static float stationHeight(const Frame *f,int slot,int station,float along){
 (void)f;(void)along;return alsHeight[slot][station<0?0:station>=ALS_STATIONS?ALS_STATIONS-1:station];
}
static void refreshStations(const Frame *f,int slot,float side){
 if(alsAge[slot]--)return;
 alsAge[slot]=120;
 float half=f->ap->length*.5f;
 for(int k=0;k<ALS_STATIONS;k++){
  float along=side*(half+30*(k+1));Vec p=at(f,along,0,0);
  float g=sceneryGroundHeight(p.x,p.y),r=runwayElevation(f->ap,side*half);
  alsHeight[slot][k]=fmaxf(g,r-30); /* poles never sink far below the threshold */
 }
}
/* v34.03 glideslope lights, drawn every frame (their colour depends on the
   eye angle) and in daylight too, like the real ones:
   - PAPI on runways of 2000 m or more: four boxes left of the landing
     direction, 300 m in; on a 3 degree path two white, two red.
   - VASI (2-bar) on shorter runways: a downwind bar 150 m in and an
     upwind bar 365 m in, two boxes each side of the runway (one side on
     runways under 30 m wide). Downwind bar white above 2.5 degrees, upwind
     bar white above 3.5: on the path red over white.
   None on runways under 700 m. */
static void glideSlope(const Frame *f,float side,Vec eye,float eyeAlong,float k,LightEmit emit){
 const Airport *ap=f->ap;float half=ap->length*.5f,w=ap->width*.5f,end=side*half,in=-side;
 if(ap->length<700||(eyeAlong-end)*side<=0)return;
 float size=2.0f+(1-k)*.6f;   /* a little larger in daylight */
 float left=-in;               /* left of travel is -across*in */
 if(ap->length>=2000){
  static const float slope[4]={3.5f,3.17f,2.83f,2.5f};  /* inner .. outer */
  for(int i=0;i<4;i++){
   float across=left*(w+15+9*i);Vec p=at(f,end+in*300,across,runwayElevation(ap,end+in*300)+.8f);
   float dx=eye.x-p.x,dy=eye.y-p.y,angle=atan2f(eye.z-p.z,sqrtf(dx*dx+dy*dy))*180/PI;
   int white=angle>slope[i];
   emit(p,1,white?.95f:.08f,white?.85f:.05f,.7f,size);
  }
  return;
 }
 static const float bar[2]={150,365},slope[2]={2.5f,3.5f};
 int sides=ap->width>=30?2:1;
 for(int b=0;b<2;b++){
  float along=end+in*bar[b],z=runwayElevation(ap,along)+.7f;
  for(int s=0;s<sides;s++){float dir=s?-left:left;
   for(int box=0;box<2;box++){
    Vec p=at(f,along,dir*(w+12+7*box),z);
    float dx=eye.x-p.x,dy=eye.y-p.y,angle=atan2f(eye.z-p.z,sqrtf(dx*dx+dy*dy))*180/PI;
    int white=angle>slope[b];
    emit(p,1,white?.95f:.08f,white?.85f:.05f,.7f,size);
   }
  }
 }
}
static void runwayEnd(const Frame *f,int index,float side,Vec eye,float eyeAlong,float time,float k,LightEmit emit,float range,int animated){
 /* side -1: threshold at along = -half, landing toward +along; +1 mirrored. */
 const Airport *ap=f->ap;float half=ap->length*.5f,w=ap->width*.5f,end=side*half,z=runwayElevation(ap,end)+.35f;
 int approachSide=(eyeAlong-end)*side>0;   /* viewer beyond this end, looking at it */
 int slot=alsSlot(index,side>0);
 /* Threshold (green, from the approach) / runway end (red, from the runway). */
 if(!animated)for(int i=0;i<=8;i++){float across=-w+2*w*i/8;
  if(approachSide)emit(at(f,end,across,z),.15f*k,1*k,.30f*k,.45f,1.6f);
  else emit(at(f,end,across,z),1*k,.12f*k,.08f*k,.45f,1.4f);
 }
 if(!approachSide)return;
 float in=-side; /* direction from the threshold into the runway */
 /* Approach lights by runway length. */
 if(!animated)refreshStations(f,slot,side);
 if(ap->length>=2700&&ap->width>=44){
  /* ALSF-2: barrettes every 30 m to 720 m. */
  if(!animated)for(int s=1;s<=24;s++){float along=end+side*30*s,h=stationHeight(f,slot,s-1,along)+1.5f;
   for(int j=-2;j<=2;j++)emit(at(f,along,j*1.05f,h),1*k,.95f*k,.85f*k,.5f,1.8f);
   if(s<=10){for(int j=0;j<3;j++){emit(at(f,along,-11-j*1.5f,h),1*k,.1f*k,.06f*k,.5f,1.5f);emit(at(f,along,11+j*1.5f,h),1*k,.1f*k,.06f*k,.5f,1.5f);}}
   if(s==5)for(int j=3;j<=6;j++){emit(at(f,along,-j*1.5f-2,h),1*k,.95f*k,.85f*k,.5f,1.6f);emit(at(f,along,j*1.5f+2,h),1*k,.95f*k,.85f*k,.5f,1.6f);}
   if(s==10)for(int j=3;j<=10;j++){emit(at(f,along,-j*1.5f,h),1*k,.95f*k,.85f*k,.5f,1.8f);emit(at(f,along,j*1.5f,h),1*k,.95f*k,.85f*k,.5f,1.8f);}
  }
  /* Sequenced flashers 330..720 m, running toward the runway twice a second. */
  float phase=fmodf(time*2,1);int lit=(int)((1-phase)*14);
  if(animated)for(int s=11;s<=24;s++)if(s-11==lit){float along=end+side*30*s;
   emit(at(f,along,0,stationHeight(f,slot,s-1,along)+2.2f),1,1,1,1.2f,3.5f);}
 }else if(ap->length>=1800){
  /* MALSR: barrettes every 60 m to 420 m, crossbar at 300 m, flashers 480..720 m. */
  if(!animated)for(int s=2;s<=14;s+=2){float along=end+side*30*s,h=stationHeight(f,slot,s-1,along)+1.5f;
   for(int j=-2;j<=2;j++)emit(at(f,along,j*1.05f,h),1*k,.95f*k,.85f*k,.5f,1.7f);
   if(s==10)for(int j=3;j<=8;j++){emit(at(f,along,-j*1.5f,h),1*k,.95f*k,.85f*k,.5f,1.7f);emit(at(f,along,j*1.5f,h),1*k,.95f*k,.85f*k,.5f,1.7f);}
  }
  float phase=fmodf(time*2,1);int lit=(int)((1-phase)*5);
  if(animated)for(int s=16;s<=24;s+=2)if((s-16)/2==lit){float along=end+side*30*s;
   emit(at(f,along,0,stationHeight(f,slot,s-1,along)+2.2f),1,1,1,1.1f,3.2f);}
 }else{
  /* REIL: a pair of strobes at the threshold, twice a second. */
  if(animated&&fmodf(time*2,1)<.12f){emit(at(f,end,-w-4,z+.5f),1,1,1,.9f,3);emit(at(f,end,w+4,z+.5f),1,1,1,.9f,3);}
 }
 /* Touchdown zone: bars of three either side of the centreline, 900 m. */
 if(!animated&&ap->length>=2400&&range<6000)
  for(int s=1;s<=30;s++){float along=end+in*30*s,zz=runwayElevation(ap,along)+.15f;
   for(int j=0;j<3;j++){emit(at(f,along,-11-j*1.5f,zz),1*k,.95f*k,.85f*k,.3f,1.2f);emit(at(f,along,11+j*1.5f,zz),1*k,.95f*k,.85f*k,.3f,1.2f);}
  }
}
static void taxiways(const Frame *f,float k,LightEmit emit){
 const Airport *ap=f->ap;if(!ap->primary||!ap->apronSide)return;
 float m=(float)-ap->apronSide,half=ap->length*.5f,w=ap->width*.5f;
 const float blue[3]={.15f*k,.35f*k,1*k},green[3]={.1f*k,1*k,.3f*k};
 /* Parallel taxiway, scenery.c: across -64..-47 (times m), along -half+65..half-65. */
 for(float t=-half+65;t<=half-65;t+=30){
  float z=runwayElevation(ap,t)+.3f;
  emit(at(f,t,-65*m,z),blue[0],blue[1],blue[2],.35f,1.2f);emit(at(f,t,-46*m,z),blue[0],blue[1],blue[2],.35f,1.2f);
 }
 /* Connectors (18 m wide at t = -570, -220, 130): blue edges, green centreline. */
 for(int i=0;i<3;i++){float t=-570+i*350;
  for(float a=47;a<=w+1;a+=12){float z=runwayElevation(ap,t+9)+.3f;
   emit(at(f,t-1,-a*m,z),blue[0],blue[1],blue[2],.35f,1.2f);emit(at(f,t+19,-a*m,z),blue[0],blue[1],blue[2],.35f,1.2f);
   emit(at(f,t+9,-a*m,z+.02f),green[0],green[1],green[2],.3f,1.1f);}
 }
 /* Apron floodlights on masts along the apron edge (apron -650..250, -245..-50). */
 for(float t=-640;t<=240;t+=110){float z=runwayElevation(ap,t)+14;
  emit(at(f,t,-238*m,z),1*k,.78f*k,.48f*k,2.4f,2.4f);emit(at(f,t,-60*m,z),1*k,.78f*k,.48f*k,2.0f,2.0f);}
}
/* v1.1p20: the nearest LIGHT_RUNWAYS runways within 32 km (nearest first, so a full lamp
   buffer drops the far ones), listed again once the eye has moved 250 m. */
#define LIGHT_RUNWAYS 48
static int lightNear[LIGHT_RUNWAYS],lightCount,lightValid;static Vec lightEye;
void airportLights(Vec eye,float time,float night,LightEmit emit,int animated){
 float k=daylightLampLevel(night);
 if(!lightValid||hypotf(eye.x-lightEye.x,eye.y-lightEye.y)>250||airportCount!=lightValid){
  lightCount=airportsNear(eye.x,eye.y,32000,lightNear,LIGHT_RUNWAYS);lightEye=eye;lightValid=airportCount;
 }
 for(int n=0;n<lightCount;n++){int i=lightNear[n];
  const Airport *ap=&airports[i];float dx=ap->east-eye.x,dy=ap->north-eye.y,range=sqrtf(dx*dx+dy*dy);
  if(range>32000)continue;
  float angle=ap->heading*PI/180;Frame f={ap,sinf(angle),cosf(angle)};
  float half=ap->length*.5f,w=ap->width*.5f+1.5f;
  float eyeAlong=(eye.x-ap->east)*f.se+(eye.y-ap->north)*f.ce;
  if(animated&&range<12000){glideSlope(&f,-1,eye,eyeAlong,k,emit);glideSlope(&f,1,eye,eyeAlong,k,emit);}
  if(k<=0)continue;
  /* Edge lights every 60 m; the last 600 m ahead of the viewer are yellow. */
  if(!animated){
  int n=(int)(ap->length/60);if(n<2)n=2;
  int ahead=eyeAlong<0?1:-1; /* runway end the viewer is looking toward */
  for(int j=1;j<n;j++){
   float along=-half+ap->length*j/n,z=runwayElevation(ap,along)+.6f;
   int caution=ap->length>=1800&&(ahead*along>half-600);
   for(int s=-1;s<=1;s+=2)emit(at(&f,along,s*w,z),1*k,caution?.78f*k:.92f*k,caution?.25f*k:.72f*k,.5f,1.5f);
  }
  /* Centreline every 15 m on long, wide runways (close range only). */
  if(ap->length>=2400&&ap->width>=44&&range<7000)
   for(float along=-half+7.5f;along<half;along+=15){
    float toEnd=ahead>0?half-along:along+half,z=runwayElevation(ap,along)+.12f;
    int idx=(int)((along+half)/15),red=toEnd<300||(toEnd<900&&(idx&1));
    emit(at(&f,along,0,z),1*k,red?.12f*k:.95f*k,red?.08f*k:.85f*k,.3f,1.1f);
   }
  if(range<9000)taxiways(&f,k,emit);
  }
  runwayEnd(&f,i,-1,eye,eyeAlong,time,k,emit,range,animated);
  runwayEnd(&f,i,1,eye,eyeAlong,time,k,emit,range,animated);
 }
}
