/* v33 dynamics harness: scripted flights per aircraft, reported as named
   metrics (tools/run_dynamics_test.py prints them with expected ranges).
   Built like the ARM physics test and run under unicorn. */
#include "flight.h"
#include "fbw.h"
#include "f16_aero.h"
#include <stdio.h>
#include <string.h>
#define PI 3.14159265359f
#define DEG (PI/180)
#define DT (1.f/120)
int checks,failures,firstFailure;
float metrics[256];int metricCount;char metricNames[8192];static int nameLen;
static void metric(const char *name,float v){
 if(metricCount>=256)return;metrics[metricCount++]=v;
 int n=snprintf(metricNames+nameLen,sizeof(metricNames)-nameLen,"%s;",name);if(n>0)nameLen+=n;
}
static int apIndex(const char *id){for(int i=0;i<AIRPORT_COUNT;i++)if(!strcmp(airports[i].id,id)&&airports[i].primary)return i;return 0;}
static void step(Flight *s,float e,float a,float r){stepFlight(s,DT,e,a,r);}
/* Pitch-attitude hold through the stick; works for direct and FBW laws. */
static float pitchStick(const Flight *s,float target){return clampf(3.0f*(target-s->pitch)-1.0f*s->q,-1,1);}
static float agl(const Flight *s){return s->pos.z-terrain(s->pos.x,s->pos.y);}
static void fly(int t,const char *tag){
 char n[64];int phx=apIndex("KPHX");Flight s;
 /* 1. Cruise, 60 s hands off. */
 resetFlightPhase(&s,t,phx,PHASE_CRUISE);
 float z0=s.pos.z,c0=s.ias,maxRoll=0;
 for(int i=0;i<60*120;i++){step(&s,0,0,0);maxRoll=fmaxf(maxRoll,fabsf(s.roll));}
 snprintf(n,64,"%s cruise60 dAlt m",tag);metric(n,s.pos.z-z0);
 snprintf(n,64,"%s cruise60 dCAS kt",tag);metric(n,s.ias-c0);
 snprintf(n,64,"%s cruise60 maxRoll deg",tag);metric(n,maxRoll/DEG);
 snprintf(n,64,"%s cruise60 crashed",tag);metric(n,s.crashed);
 /* 2. Takeoff, full thrust; rotate at VR to 8 deg (SR22/SF50 12 deg). */
 resetFlightPhase(&s,t,phx,PHASE_TAKEOFF);
 const Aircraft *a=&aircraft[t];float x0=s.pos.x,y0=s.pos.y,liftDist=-1,liftIas=0,vs1000=-1,pitchT=(a->engine==ENGINE_PISTON?7:8)*DEG;
 s.throttle=1;
 for(int i=0;i<120*120&&!s.crashed;i++){
  float e=s.ias>a->speeds.rotate?pitchStick(&s,pitchT):0;
  if(!s.onGround&&agl(&s)>15&&a->retractableGear)s.gear=0;
  if(!s.onGround&&agl(&s)>150)s.flap=0;
  step(&s,e,0,0);
  if(liftDist<0&&!s.onGround&&agl(&s)>a->wheelHeight+1){liftDist=hypotf(s.pos.x-x0,s.pos.y-y0);liftIas=s.ias;}
  if(vs1000<0&&agl(&s)>305)vs1000=s.vel.z*196.85f;
 }
 snprintf(n,64,"%s takeoff roll m",tag);metric(n,liftDist);
 snprintf(n,64,"%s liftoff CAS kt",tag);metric(n,liftIas);
 snprintf(n,64,"%s climb at 1000ft fpm",tag);metric(n,vs1000);
 snprintf(n,64,"%s takeoff crashed",tag);metric(n,s.crashed);
 /* 3. Climb at the climb speed, full thrust, 40 s; average of the last 20 s. */
 resetFlightPhase(&s,t,phx,PHASE_CLIMB);s.throttle=1;
 float target=a->speeds.climb,tp=s.pitch,sumVs=0;int nVs=0;
 for(int i=0;i<40*120;i++){
  tp=clampf(tp+DT*.0012f*(s.ias-target),0,20*DEG);
  step(&s,pitchStick(&s,tp),0,0);
  if(i>20*120){sumVs+=s.vel.z*196.85f;nVs++;}
 }
 snprintf(n,64,"%s climb %.0fkt fpm",tag,target);metric(n,nVs?sumVs/nVs:0);
 snprintf(n,64,"%s climb CAS kt",tag);metric(n,s.ias);
 /* 4. Full back stick 4 s from cruise, then release 6 s. */
 resetFlightPhase(&s,t,phx,PHASE_CRUISE);float peak=0;
 for(int i=0;i<4*120;i++){step(&s,1,0,0);peak=fmaxf(peak,s.loadFactor);}
 for(int i=0;i<6*120;i++)step(&s,0,0,0);
 snprintf(n,64,"%s pull peak g",tag);metric(n,peak);
 snprintf(n,64,"%s release q deg/s",tag);metric(n,s.q/DEG);
 snprintf(n,64,"%s release g",tag);metric(n,s.loadFactor);
 /* 5. Full roll stick 2 s, release 8 s; then 4 s more, release 12 s. */
 resetFlightPhase(&s,t,phx,PHASE_CRUISE);
 float peakP=0;for(int i=0;i<2*120;i++){step(&s,0,1,0);peakP=fmaxf(peakP,s.p);}
 snprintf(n,64,"%s roll rate deg/s",tag);metric(n,peakP/DEG);
 float rel=s.roll;for(int i=0;i<8*120;i++)step(&s,0,0,0);
 snprintf(n,64,"%s bank after release deg",tag);metric(n,s.roll/DEG);
 snprintf(n,64,"%s bank at release deg",tag);metric(n,rel/DEG);
 for(int i=0;i<4*120;i++)step(&s,0,1,0);
 snprintf(n,64,"%s bank stick held deg",tag);metric(n,s.roll/DEG);
 for(int i=0;i<12*120;i++)step(&s,0,0,0);
 snprintf(n,64,"%s bank 12s after release deg",tag);metric(n,s.roll/DEG);
 /* 6. Final configuration, idle, full back stick 25 s. */
 resetFlightPhase(&s,t,phx,PHASE_FINAL);s.pos.z+=900;s.throttle=0;float maxAlpha=0;
 for(int i=0;i<25*120&&!s.crashed;i++){step(&s,1,0,0);maxAlpha=fmaxf(maxAlpha,s.alpha);}
 snprintf(n,64,"%s stall maxAlpha deg",tag);metric(n,maxAlpha/DEG);
 snprintf(n,64,"%s stall alphaMax deg",tag);metric(n,aircraftAlphaMax(&s)/DEG);
 /* 7. Final, hands off 20 s: approach stability. */
 resetFlightPhase(&s,t,phx,PHASE_FINAL);s.pos.z+=600;float c1=s.ias;
 for(int i=0;i<20*120;i++)step(&s,0,0,0);
 snprintf(n,64,"%s final20 dCAS kt",tag);metric(n,s.ias-c1);
 snprintf(n,64,"%s final20 vs fpm",tag);metric(n,s.vel.z*196.85f);
}
/* Stevens & Lewis published trim: 502 ft/s, sea level, cg .35 ->
   alpha 2.11 deg, elevator -0.756 deg (TE down +), throttle .1385. */
static void f16PublishedTrim(void){
 const float V=502*.3048f,qS=.5f*1.225f*V*V*F16_AREA,W=9298.6f*9.80665f;
 float alpha=0,el=0;
 for(int it=0;it<60;it++){
  float lo=-1,hi=1;for(int k=0;k<40;k++){el=(lo+hi)/2;F16Coefficients c=f16Coefficients(alpha,0,V,0,0,0,el,0,0);if(c.cm>0)hi=el;else lo=el;}
  F16Coefficients c=f16Coefficients(alpha,0,V,0,0,0,el,0,0);
  float f=qS*c.cz+W*cosf(alpha);alpha+=f/(qS*3.6f);
 }
 F16Coefficients c=f16Coefficients(alpha,0,V,0,0,0,el,0,0);
 float need=W*sinf(alpha)-qS*c.cx,lo=0,hi=1,thr=0;
 for(int k=0;k<40;k++){thr=(lo+hi)/2;if(f16ThrustNewtons(thr,V/340.294f,0)<need)lo=thr;else hi=thr;}
 metric("F16 S&L trim alpha deg (2.11)",alpha/DEG);
 metric("F16 S&L trim elevator deg (-0.756)",-el*25);
 metric("F16 S&L trim throttle (.1385)",thr);
}
int main(void){
 f16PublishedTrim();
 static const char *tags[]={"SR22","SF50","B737","A321","F16"};
 for(int t=0;t<AIRCRAFT_COUNT;t++)fly(t,tags[t]);
 return 0;
}
