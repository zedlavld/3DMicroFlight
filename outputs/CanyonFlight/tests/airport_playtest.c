#include "flight.h"
#include "terrain.h"
#include "camera.h"
#include "scenery.h"
#include "airport_index.h"
int checks,failures,firstFailure;
/* v31: every start runway (AIRPORT_START_COUNT) for both aircraft. */
int startCount=AIRPORT_START_COUNT;
float results[2*AIRPORT_START_COUNT][10];
float impact[2*AIRPORT_START_COUNT][8];
int airportVertices[AIRPORT_START_COUNT],asphaltVertices[AIRPORT_START_COUNT];
static WorldVertex vertices[SCENERY_PART_CAPACITY];
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
int main(void){
 for(int type=0;type<2;type++)for(int k=0;k<AIRPORT_START_COUNT;k++){int ap=airportStarts[k];
  Flight s;resetFlightPhase(&s,type,ap,PHASE_TAKEOFF);Vec start=s.pos;
  s.throttle=1;float distance=0;int lifted=0;
  for(int i=0;i<120*70&&!s.crashed;i++){
   float target=s.ias>(type?90:68)?.12f:0;
   float e=clampf(3.2f*(target-s.pitch)-.7f*s.q,-1,1);
   float a=clampf(-3*s.roll-.6f*s.p,-1,1),r=clampf(2*s.beta-.6f*s.r,-1,1);
   Flight before=s;stepFlight(&s,1.f/120,e,a,r);
   if(s.crashed){float *p=impact[type*AIRPORT_START_COUNT+k];p[0]=before.vel.z;p[1]=before.q;p[2]=before.p;p[3]=before.pitch;p[4]=before.roll;p[5]=before.onGround;p[6]=before.groundLoad;p[7]=before.touchdownSink;}
   if(!s.onGround&&s.pos.z>terrain(s.pos.x,s.pos.y)+8){lifted=1;break;}
  }
  distance=hypotf(s.pos.x-start.x,s.pos.y-start.y);
  float *v=results[type*AIRPORT_START_COUNT+k];v[0]=type;v[1]=ap;v[2]=s.crashed;v[3]=lifted;v[4]=distance;v[5]=s.ias;v[6]=s.pitch;v[7]=s.roll;v[8]=s.pos.z-terrain(s.pos.x,s.pos.y);v[9]=s.groundLoad;
  CHECK(!s.crashed&&lifted&&distance<airports[ap].length-80);
  resetFlightPhase(&s,type,ap,PHASE_TAKEOFF);
  View view={.distance=32};CameraPose camera=makeCamera(&s,&view);
  CHECK(camera.eye.z-terrain(camera.eye.x,camera.eye.y)>1.7f);
  view.exterior=1;resetView(&view);camera=makeCamera(&s,&view);
  /* Full exterior bounding box fits the normal camera, including gear. */
  for(int x=-1;x<=1;x+=2)for(int y=-1;y<=1;y+=2)for(int z=-1;z<=1;z+=2){
   Vec p=bodyWorld(&s,(Vec){x*aircraft[type].length*.5f,y*aircraft[type].span*.5f,z*2});
   p.x+=s.pos.x;p.y+=s.pos.y;p.z+=s.pos.z;p=projectCamera(&camera,p);
   CHECK(p.z>2&&fabsf(p.x)*220/p.z<195&&fabsf(p.y)*220/p.z<95);
  }
  /* Geometry at the runway must agree with the contact surface to centimetres,
     rather than putting the camera/airframe below metre-high pavement. */
  /* v1.1p20: the runways within 45 km, nearest first, go round robin over object parts
     32..35 (v31: record i in part 32 + i % 4). */
  int near[64],nearCount=airportsNear(s.pos.x,s.pos.y,45000,near,64),slot=0;
  for(int q=0;q<nearCount;q++)if(near[q]==ap)slot=q;
  int n=buildSceneryPart(&s,vertices,SCENERY_PART_CAPACITY,32+slot%4);
  CHECK(n>0&&n<SCENERY_PART_CAPACITY-3&&n%3==0);
  airportVertices[k]=n;int asphalt=0;
  /* The four shoulder bands must leave the asphalt footprint open. Check
     triangle interiors; checking vertices alone missed the old spanning quad. */
  const Airport *shoulder=&airports[near[slot%4]];   /* the part's first (and nearest) runway: full detail */
  for(int i=0;i<24;i+=3){
   Vec p={0,0,0};for(int j=0;j<3;j++){p.x+=vertices[i+j].p.x/3;p.y+=vertices[i+j].p.y/3;p.z+=vertices[i+j].p.z/3;}
   float angle=shoulder->heading*.01745329252f,dx=p.x-shoulder->east,dy=p.y-shoulder->north;
   float along=dx*sinf(angle)+dy*cosf(angle),across=dx*cosf(angle)-dy*sinf(angle);
   CHECK(fabsf(along)>=shoulder->length*.5f||fabsf(across)>=shoulder->width*.5f);
   CHECK(fabsf(p.z-runwayElevation(shoulder,along)-.01f)<.003f);
  }
  for(int i=0;i<n;i++){
   int material=sceneryVertexMaterial(i);asphalt+=material==-2;
   CHECK(material==-1||material==-2);
   if(i%3)CHECK(material==sceneryVertexMaterial(i-i%3));
   Vec p=vertices[i].p;float h=terrain(p.x,p.y);
   float angle=airports[ap].heading*.01745329252f,dx=p.x-airports[ap].east,dy=p.y-airports[ap].north;
   float along=dx*sinf(angle)+dy*cosf(angle),across=dx*cosf(angle)-dy*sinf(angle);
   if(fabsf(along)<airports[ap].length*.5f-10&&fabsf(across)<airports[ap].width*.5f)
    CHECK(p.z-h<.15f);
  }
  asphaltVertices[k]=asphalt;CHECK(asphalt>100);
  float h=airports[ap].heading*.01745329252f;
  for(float along=-airports[ap].length*.5f+10;along<airports[ap].length*.5f-10;along+=10)
   for(int side=-1;side<=1;side++){
    float across=side*airports[ap].width*.25f;
    float east=airports[ap].east+along*sinf(h)+across*cosf(h),north=airports[ap].north+along*cosf(h)-across*sinf(h);
    CHECK(fabsf(terrain(east,north)-runwayElevation(&airports[ap],along))<.01f);
   }
 }
 return failures;
}
