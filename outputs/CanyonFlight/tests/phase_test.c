#include "flight.h"
#include <string.h>
#define PI 3.14159265359f
#define G 9.80665f
#define DT (1.f/120)
#define KNOT .514444444f
volatile int checks,failures,firstFailure,caseCount;
/* One diagnostic per scenario, independent of the scenery/mesh suite. */
float results[64][12];int caseTypes[64];
static float surface,slope;
float terrain(float e,float n){return surface+slope*(n-airports[0].north);}
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
static float speedCAS(float cas,float height){float lo=0,hi=250;for(int i=0;i<25;i++){float m=(lo+hi)/2;if(calibratedAirspeed(m,atmosphere(height,0))<cas)lo=m;else hi=m;}return (lo+hi)/2;}
static void sideTrim(Flight *s){
 const Aircraft *a=&aircraft[s->aircraft];s->aileronTrim=s->rudderTrim=s->aileron=s->rudder=0;
 Aero f=evaluateAero(s);float tas=sqrtf(s->vel.x*s->vel.x+s->vel.y*s->vel.y+s->vel.z*s->vel.z),scale=.5f*atmosphere(s->pos.z,s->isaOffset).density*tas*tas*a->area*a->span;
 if(scale>1){s->aileronTrim=clampf(-f.moment.x/(scale*a->clda),-.2f,.2f);s->rudderTrim=clampf(-(f.moment.z/scale+a->cnda*s->aileronTrim)/a->cndr,-.2f,.2f);s->aileron=s->aileronTrim;s->rudder=s->rudderTrim;}
}
/* Trim force balance at a prescribed path angle. Engine mode: -1 solve power,
   0 engine out, 1 maximum thrust. This is a test fixture, not product autopilot. */
static void trim(Flight *s,int type,float altitude,float cas,float flap,float gamma,int mode){
 resetFlight(s,type,0);s->pos=(Vec){0,0,altitude};s->vel=(Vec){0};s->flap=s->flapActual=flap;s->gear=s->gearActual=type?(flap>0):1;s->engineCutoff=mode==0;if(type&&flap>0)s->mass=2517.4377f;
 float v=speedCAS(cas,altitude);s->vel=(Vec){0,v*cosf(gamma),v*sinf(gamma)};
 float powerLo=0,powerHi=1;
 for(int outer=0;outer<24;outer++){
  s->engine=mode<0?(powerLo+powerHi)/2:(mode==0?0:1);
  float lo=-.12f,hi=.5f;
  for(int i=0;i<24;i++){
   float alpha=(lo+hi)/2;setAttitude(s,0,alpha+gamma,0);Aero a=evaluateAero(s);Vec f=bodyWorld(s,a.force);
   float normal=f.z*cosf(gamma)-f.y*sinf(gamma);
   if(normal>s->mass*G*cosf(gamma))hi=alpha;else lo=alpha;
  }
  Aero a=evaluateAero(s);Vec f=bodyWorld(s,a.force);float residual=f.y*cosf(gamma)+f.z*sinf(gamma)-s->mass*G*sinf(gamma);
  if(residual>0)powerHi=s->engine;else powerLo=s->engine;
 }
 const Aircraft *a=&aircraft[type];Aero f=evaluateAero(s);s->trim=clampf(-(a->cm0+a->cma*f.alpha-.025f*flap)/a->cme,-.3f,.3f);s->throttle=s->engine;sideTrim(s);f=evaluateAero(s);s->ias=f.ias;s->alpha=f.alpha;
}
static float equilibriumPath(int type,float altitude,float cas,int mode){
 float lo=mode==0?-.35f:0,hi=mode==0?0:.35f;
 for(int i=0;i<22;i++){
  float gamma=(lo+hi)/2;Flight s;trim(&s,type,altitude,cas,0,gamma,mode);Vec f=bodyWorld(&s,evaluateAero(&s).force);
  float residual=f.y*cosf(gamma)+f.z*sinf(gamma)-s.mass*G*sinf(gamma);
  if(residual>0)lo=gamma;else hi=gamma;
 }return (lo+hi)/2;
}
static void pilot(Flight *s,float pitch,float bank){
 float e=clampf(3.2f*(pitch-s->pitch)-.7f*s->q,-1,1);
 float a=clampf(3*(bank-s->roll)-.6f*s->p,-1,1);
 float rudder=clampf(2*s->beta-.6f*s->r,-1,1);
 stepFlight(s,DT,e,a,rudder);
}
static void record(int type,int phase,float a,float b,float c,float d,float e){int i=caseCount++;caseTypes[i]=phase;results[i][0]=type;results[i][1]=a;results[i][2]=b;results[i][3]=c;results[i][4]=d;results[i][5]=e;}
int main(void){
 surface=-1000; /* Keep airborne numerical scenarios above the test surface. */
 /* Full stick and trim must not create travel beyond the elevator stop.
    Check both directions, both aircraft and unsaturated trim authority. */
 for(int type=0;type<2;type++){
  Flight s;resetFlight(&s,type,0);s.trim=0;
  for(int sign=-1;sign<=1;sign+=2){
   s.elevator=sign;s.trim=0;float stop=evaluateAero(&s).moment.y;
   s.trim=sign*.3f;CHECK(fabsf(evaluateAero(&s).moment.y-stop)<.01f);
   s.elevator=sign*.4f;float withTrim=evaluateAero(&s).moment.y;
   s.elevator=sign*.7f;s.trim=0;CHECK(fabsf(evaluateAero(&s).moment.y-withTrim)<.01f);
  }
 }
 /* Static reference-condition matrix, distinct from dynamic phase runs.
    Include the installed SR22's 3600-lb maximum as a sensitivity case;
    preserve the product's 3400-lb default and published stall anchors. */
 for(int type=0;type<2;type++)for(int load=0;load<2;load++)for(int level=0;level<3;level++){
  float altitude=level==0?0:level==1?609.6f:1828.8f;
  Flight s;trim(&s,type,altitude,type?140:100,0,0,1);
  s.mass=type?(load?2721.554f:2517.4377f):(load?1632.9325f:1542.214f);
  float stall=stallSpeedCAS(&s,0),coldThrust=evaluateAero(&s).thrust;
  for(int hot=0;hot<2;hot++){
   s.isaOffset=hot?20:0;Aero a=evaluateAero(&s);
   record(type,13,altitude,s.isaOffset,s.mass,stallSpeedCAS(&s,0),a.thrust);
   CHECK(isfinite(a.force.x)&&isfinite(a.moment.y)&&a.thrust>0);
   CHECK(fabsf(stallSpeedCAS(&s,0)-stall)<.001f);
   CHECK(fabsf(stall/aircraft[type].stallCAS[0]-sqrtf(s.mass/aircraft[type].mass))<.0001f);
   CHECK(fabsf(stallSpeedCAS(&s,PI/3)/stall-sqrtf(2))<.0001f);
   if(hot)CHECK(a.thrust<coldThrust);
  }
 }
 for(int type=0;type<2;type++){
  for(int level=0;level<3;level++){
   float altitude=level==0?0:level==1?609.6f:1828.8f,cas=type?140:101-level*(level==2?1.5f:1.f);
   float gamma=equilibriumPath(type,altitude,cas,1);Flight s;trim(&s,type,altitude,cas,0,gamma,1);float z=s.pos.z,pitch=s.pitch;
   for(int i=0;i<2400;i++){float target=pitch+clampf((s.ias-cas)*.006f,-.1f,.1f);pilot(&s,target,0);}
   float climb=(s.pos.z-z)/20*196.8504f;record(type,1,altitude,cas,climb,s.ias,s.roll);
   CHECK(!s.crashed&&isfinite(climb)&&climb>300&&climb<3500);CHECK(fabsf(s.ias-cas)<8);
   if(type==0){float ref=level==0?1398:level==1?1279:1041;CHECK(fabsf(climb/ref-1)<.12f);}
  }
  for(int mode=0;mode<2;mode++){
   Flight s;float cas=type?180:135,gamma=mode?-.06f:0;trim(&s,type,3500,cas,0,gamma,-1);float z=s.pos.z,pitch=s.pitch;
   for(int i=0;i<7200;i++)pilot(&s,pitch+clampf((s.ias-cas)*.004f,-.1f,.1f),0);
   record(type,mode?3:2,cas,(s.pos.z-z)/60*196.8504f,s.ias,s.throttle,s.roll);
   CHECK(!s.crashed&&isfinite(s.ias));CHECK(fabsf(s.ias-cas)<7);
   if(!mode)CHECK(fabsf(s.pos.z-z)<80);else CHECK(s.pos.z<z-100);
  }
  float cas=type?120:88,gamma=equilibriumPath(type,5000,cas,0);Flight s;trim(&s,type,5000,cas,0,gamma,0);float z=s.pos.z,n=s.pos.y,pitch=s.pitch;
  for(int i=0;i<7200;i++)pilot(&s,pitch+clampf((s.ias-cas)*.005f,-.08f,.08f),0);
  float ratio=(s.pos.y-n)/(z-s.pos.z);record(type,7,cas,ratio,(z-s.pos.z)/60*196.8504f,s.ias,gamma);
  CHECK(!s.crashed&&s.thrust==0&&ratio>4&&ratio<20);if(type==0)CHECK(fabsf(ratio-9.6f)<.5f);
  /* Stabilized approach and a go-around from the same trimmed configuration. */
  trim(&s,type,2500,type?90:80,1,-.05236f,-1);z=s.pos.z;pitch=s.pitch;
  for(int i=0;i<3600;i++)pilot(&s,pitch+clampf((s.ias-(type?90:80))*.004f,-.08f,.08f),0);
  record(type,4,s.ias,(s.pos.z-z)/30*196.8504f,s.flapActual,s.gearActual,s.mass);
  CHECK(!s.crashed&&s.pos.z<z-40&&fabsf(s.ias-(type?90:80))<5);
  z=s.pos.z;s.throttle=1;s.flap=.5f;if(type)s.gear=0;
  for(int i=0;i<5400;i++){if(i>1800)s.flap=0;pilot(&s,.12f+clampf((s.ias-(type?120:95))*.005f,-.05f,.08f),0);}
  record(type,11,s.pos.z-z,s.ias,s.flapActual,s.gearActual,s.stallWarning);
  CHECK(!s.crashed&&s.pos.z>z+50&&s.stallWarning<.2f);
  /* A controlled 30-degree banked turn: altitude response is test-pilot
     feedback, not an autopilot included in the handheld application. */
  trim(&s,type,3500,type?180:135,0,0,-1);z=s.pos.z;pitch=s.pitch;float startYaw=s.yaw;
  for(int i=0;i<5400;i++){
   float bank=fminf(i*DT/5,1)*PI/6;
   float target=pitch+clampf((z-s.pos.z)*.0006f-s.vel.z*.014f,-.12f,.18f);
   pilot(&s,target,bank);
  }
  float yawChange=atan2f(sinf(s.yaw-startYaw),cosf(s.yaw-startYaw));
  record(type,12,s.pos.z-z,s.roll,yawChange,s.ias,s.beta);
  CHECK(!s.crashed&&fabsf(s.roll-PI/6)<.05f&&fabsf(s.pos.z-z)<100&&yawChange>.3f);
  /* Dynamic slow deceleration through stall, then unload and add power. */
  trim(&s,type,5000,type?100:80,0,0,-1);s.throttle=0;float maxWarning=0,maxAlpha=0,minIAS=s.ias;
  for(int i=0;i<3600;i++){pilot(&s,.28f,0);maxWarning=fmaxf(maxWarning,s.stallWarning);maxAlpha=fmaxf(maxAlpha,s.alpha);minIAS=fminf(minIAS,s.ias);}
  float stalledIAS=minIAS;s.throttle=1;
  for(int i=0;i<3600;i++)pilot(&s,-.06f,0);
  record(type,6,maxWarning,maxAlpha,stalledIAS,s.ias,s.stallWarning);
  CHECK(!s.crashed&&isfinite(s.ias));CHECK(maxWarning>.9f);CHECK(s.ias>stalledIAS+10&&s.stallWarning<.2f);
 }
 /* AFM MCT comparison probes deliberately report a required model setting,
    rather than pretending full takeoff lever equals maximum continuous thrust.
    ISA OAT interpolation of AFM 5-31..33, 6000 lb, clean, anti-ice off. */
 const float sfAlt[3]={0,609.6f,1828.8f},sfCas[3]={176.5f,172.86f,176.95f},sfGrad[3]={632.5f,626.44f,589.14f};
 for(int i=0;i<3;i++){
  Flight probe;float angle=atanf(sfGrad[i]*.3048f/1852);trim(&probe,1,sfAlt[i],sfCas[i],0,angle,-1);
  float required=probe.throttle;float full=equilibriumPath(1,sfAlt[i],sfCas[i],1);
  record(1,9,sfAlt[i],sfCas[i],sfGrad[i],tanf(full)*1852/.3048f,required);
  CHECK(required>0&&required<1);
 }
 /* Ground scenarios share actual airport plan coordinates but a flat surface,
    isolating wheel physics from the mesh and terrain conditioning. */
 for(int type=0;type<2;type++){
  surface=airports[0].elevation;slope=0;Flight s;resetFlightPhase(&s,type,0,PHASE_TAKEOFF);s.brakeLeft=s.brakeRight=1;
  for(int i=0;i<1200;i++)stepFlight(&s,DT,0,0,0);
  float z=s.pos.z,velocity=hypotf(s.vel.x,s.vel.y);record(type,8,z-surface,velocity,s.roll,s.pitch,s.groundLoad);
  CHECK(!s.crashed&&s.onGround);CHECK(fabsf(s.vel.z)<.1f&&fabsf(s.roll)<.03f);
  s.throttle=1;Vec start=s.pos;
  for(int i=0;i<600;i++)stepFlight(&s,DT,0,0,0);
  CHECK(hypotf(s.pos.x-start.x,s.pos.y-start.y)<1);
  s.brakeLeft=s.brakeRight=0;float distance=0,liftoffCAS=0;
  for(int i=0;i<9600;i++){
   float pitch=s.ias>(type?90:68)?.14f:0;pilot(&s,pitch,0);
   if(!s.onGround&&s.pos.z>surface+8){distance=hypotf(s.pos.x-start.x,s.pos.y-start.y);liftoffCAS=s.ias;break;}
  }
  record(type,0,distance,liftoffCAS,s.pos.z-surface,s.ias,s.crashed);
  CHECK(!s.crashed&&distance>100&&distance<2000&&liftoffCAS>60);
  /* Normal touchdown: gear down, small sink, then braking on dry pavement. */
  resetFlightPhase(&s,type,0,PHASE_TAKEOFF);s.pos.z=surface+1.32f;if(type)s.mass=2517.4377f;s.flap=s.flapActual=1;
  float speed=speedCAS(type?82:72,surface);float h=airports[0].heading*PI/180;
  s.vel=(Vec){sinf(h)*speed,cosf(h)*speed,-.7f};setAttitude(&s,0,.04f,h);s.trim=0;s.engine=s.throttle=0;s.brakeLeft=s.brakeRight=1;start=s.pos;
  int touched=0;for(int i=0;i<7200&&!s.crashed;i++){pilot(&s,0,0);touched|=s.onGround;if(touched&&hypotf(s.vel.x,s.vel.y)<.2f)break;}
  distance=hypotf(s.pos.x-start.x,s.pos.y-start.y);record(type,5,distance,s.touchdownSink,hypotf(s.vel.x,s.vel.y),s.pitch,s.crashed);
  CHECK(touched&&!s.crashed&&s.onGround);CHECK(distance>20&&distance<1500&&hypotf(s.vel.x,s.vel.y)<.3f);
  /* Gear-up and excessive sink contacts must still be recognized as crashes. */
  resetFlightPhase(&s,type,0,PHASE_TAKEOFF);s.pos.z=surface+1.24f;s.vel.z=-8;stepFlight(&s,DT,0,0,0);CHECK(s.crashed);
  resetFlightPhase(&s,type,0,PHASE_TAKEOFF);s.pos.z=surface+.55f;s.gearActual=0;stepFlight(&s,DT,0,0,0);CHECK(s.crashed);
  resetFlightPhase(&s,type,0,PHASE_TAKEOFF);s.pos.z=surface+3;setAttitude(&s,.75f,0,0);stepFlight(&s,DT,0,0,0);CHECK(s.crashed);
  /* Differential main-wheel braking must turn left without a fictional
     linked nose-wheel steering mechanism. */
  resetFlightPhase(&s,type,0,PHASE_TAKEOFF);s.brakeLeft=s.brakeRight=1;
  for(int i=0;i<600;i++)stepFlight(&s,DT,0,0,0);
  float initialYaw=s.yaw;s.brakeRight=0;s.vel=(Vec){sinf(h)*5,cosf(h)*5,0};
  float turn=0,lastYaw=initialYaw;
  for(int i=0;i<600;i++){stepFlight(&s,DT,0,0,0);turn+=atan2f(sinf(s.yaw-lastYaw),cosf(s.yaw-lastYaw));lastYaw=s.yaw;}
  record(type,10,turn,hypotf(s.vel.x,s.vel.y),s.roll,s.pitch,s.crashed);
  CHECK(!s.crashed&&turn<-.03f);
  slope=.02f;resetFlightPhase(&s,type,0,PHASE_TAKEOFF);s.brakeLeft=s.brakeRight=1;start=s.pos;
  for(int i=0;i<2400;i++)stepFlight(&s,DT,0,0,0);
  CHECK(!s.crashed&&hypotf(s.pos.x-start.x,s.pos.y-start.y)<.5f);slope=0;
  /* Ground wind is not tire velocity. A stationary crosswind must not act
     as a sideways conveyor belt under the tires. */
  resetFlightPhase(&s,type,0,PHASE_TAKEOFF);s.wind=(Vec){5,0,0};s.brakeLeft=s.brakeRight=1;
  for(int i=0;i<1200;i++)stepFlight(&s,DT,0,0,0);CHECK(!s.crashed&&hypotf(s.vel.x,s.vel.y)<.3f);
 }
 return failures;
}
