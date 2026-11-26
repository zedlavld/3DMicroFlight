/* Host test (v1.1p15): the test autopilot flies a route with the route options of
   autopilotRoute (cruise altitude, maximum speed, full landing) and must take off, cruise
   within the speed limit, land on the centreline and stop without crashing.
   usage: route_landing [from] [to] [aircraft] [cruise ft] [max 0/1] [weather] [trace s]
   build: gcc -O2 -Iinclude -Itests/stream_stubs source/{flight,fbw,f16_aero,ground,terrain,terrain_stream,
          terrain_data,airports,airport_index,autopilot}.c tests/stream_stubs/pfd_limit.c tests/route_landing_test.c -lm
   v1.1 release set (all PASS; CF_AIRPORTS_FLY for KDEN/KCOS): KLAS KTUS 2 35000 1 0 and weather 942,
   KCOS KDEN 2 15000 0 / 0 9000 0, KPHX KDEN 2 35000 1, KPHX KLAS 3 35000 1 / 1 25000 1, KPHX KTUS 4 20000 1
   / 3 20000 1, KSEZ KFLG 0 9000 0 / 1 9000 0, KDVT KPHX 2 8000 0, KLAS KPHX 4 30000 1, KFLG KSEZ 4 9000 1.
   East of the Rockies the host has no terrain data (KATL-KCLT flies into it): test those on the 3DS. */
#include "autopilot.h"
#include "instruments.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "airport_index.h"
/* v1.1p20: CF_AIRPORTS_FLY=<path to airports_fly.bin> adds every flyable US airport (KDEN, KORD...). */
static int primary(const char *id){
 static int loaded;if(!loaded){loaded=1;const char *p=getenv("CF_AIRPORTS_FLY");if(p)printf("%d card runway records from %s\n",airportsLoad(p),p);}
 return airportFind(id);
}
int main(int argc,char **argv){
 const char *from=argc>1?argv[1]:"KLAS",*to=argc>2?argv[2]:"KTUS";int craft=argc>3?atoi(argv[3]):2;
 float ft=argc>4?atof(argv[4]):35000;int maxSpeed=argc>5?atoi(argv[5]):1,weather=argc>6?atoi(argv[6]):0;float trace=argc>7?atof(argv[7]):120;
 int a=primary(from),b=primary(to);if(a<0||b<0){printf("unknown airport\n");return 2;}
 Flight s;resetFlightPhase(&s,craft,a,PHASE_TAKEOFF);setWeather(&s,weather);
 Autopilot ap;autopilotBegin(&ap,&s,b);autopilotRoute(&ap,&s,ft/3.28084f,maxSpeed,1);
 const float dt=1/120.f;float t=0,maxOver=-1e9,cruiseCas=0,cruiseN=0,stageAt[AP_STAGE_COUNT];for(int k=0;k<AP_STAGE_COUNT;k++)stageAt[k]=-1;
 float tdSink=-1,tdAlong=0,tdCross=0,tdCas=0,tdPitch=0,tdRoll=0,maxBank=0,minAgl=1e9,maxAlt=0,stopT=-1,maxMach=0;int last=-1;
 for(;t<6000&&!s.crashed;t+=dt){
  float e,r,y;s.brakeLeft=s.brakeRight=0;autopilotStep(&ap,&s,dt,&e,&r,&y);stepFlight(&s,dt,e,r,y);
  if(ap.stage!=last){stageAt[ap.stage]=t;last=ap.stage;printf("t %6.0f -> %s\n",t,autopilotStageName(ap.stage));}
  float agl=s.pos.z-terrain(s.pos.x,s.pos.y);maxAlt=fmaxf(maxAlt,s.pos.z);
  if(!s.onGround){maxBank=fmaxf(maxBank,fabsf(s.roll));if(ap.stage<AP_APPROACH&&t>60)minAgl=fminf(minAgl,agl);}
  if(ap.stage!=AP_TAKEOFF&&!s.onGround)maxOver=fmaxf(maxOver,s.ias-pfdAirspeedLimit(&s));
  Atmosphere at=atmosphere(s.pos.z,s.isaOffset);float tas=sqrtf(s.vel.x*s.vel.x+s.vel.y*s.vel.y+s.vel.z*s.vel.z);maxMach=fmaxf(maxMach,tas/at.sound);
  if(ap.stage==AP_CRUISE&&t-stageAt[AP_CRUISE]>120){cruiseCas+=s.ias;cruiseN++;}
  if(tdSink<0&&ap.stage==AP_ROLLOUT){float h=ap.runwayHeading,rx=s.pos.x-ap.aimX,ry=s.pos.y-ap.aimY;
   tdSink=s.touchdownSink;tdAlong=rx*sinf(h)+ry*cosf(h);tdCross=rx*cosf(h)-ry*sinf(h);tdCas=s.ias;tdPitch=s.pitch;tdRoll=s.roll;}
  if(ap.stage==AP_ROLLOUT&&hypotf(s.vel.x,s.vel.y)<1&&stopT<0){stopT=t;break;}
  int step=(int)(t/dt+.5f);if(step%(int)(trace*120)==0||(ap.stage>=AP_APPROACH&&step%(10*120)==0))
   printf("t %5.0f %-8s %6.1f km msl %5.0f agl %5.0f cas %4.0f lim %4.0f M%.2f vs %5.1f pitch %5.1f roll %5.1f thr %.2f gear %d flap %.2f spd %.1f wind %.0f/%.0f along %.0f cross %.0f trk %.1f\n",
    t,autopilotStageName(ap.stage),autopilotDistance(&ap,&s)/1000,s.pos.z,agl,s.ias,pfdAirspeedLimit(&s),tas/at.sound,s.vel.z,s.pitch*57.3f,s.roll*57.3f,s.throttle,s.gear,s.flap,s.speedbrake,s.wind.x,s.wind.y,-((s.pos.x-ap.aimX)*sinf(ap.runwayHeading)+(s.pos.y-ap.aimY)*cosf(ap.runwayHeading)),(s.pos.x-ap.aimX)*cosf(ap.runwayHeading)-(s.pos.y-ap.aimY)*sinf(ap.runwayHeading),atan2f(s.vel.x,s.vel.y)*57.3f);
  if(getenv("FLR")&&ap.stage>=AP_APPROACH&&s.pos.z-terrain(s.pos.x,s.pos.y)<40&&step%12==0)printf("  flr t %.1f %s agl %.1f vs %.2f pitch %.2f q %.2f elev %.2f alpha %.1f cas %.0f thr %.2f stallw %.2f bias %.2f\n",t,autopilotStageName(ap.stage),s.pos.z-terrain(s.pos.x,s.pos.y),s.vel.z,s.pitch*57.3f,s.q*57.3f,e,s.alpha*57.3f,s.ias,s.throttle,s.stallWarning,ap.pitchBias*57.3f);
  if(getenv("DBG")&&t>atof(getenv("DBG"))&&t<atof(getenv("DBG"))+40&&step%60==0)printf("  dbg t %.1f beta %.1f r %.2f p %.2f roll %.1f yaw %.1f trk %.1f ail %.2f rud %.2f flap %.2f/%.2f gear %.2f cas %.0f wind %.1f %.1f\n",t,s.beta*57.3f,s.r*57.3f,s.p*57.3f,s.roll*57.3f,s.yaw*57.3f,atan2f(s.vel.x,s.vel.y)*57.3f,r,y,s.flap,s.flapActual,s.gearActual,s.ias,s.wind.x,s.wind.y);
 }
 const Airport *rw=&airports[ap.runway];
 float rx=s.pos.x-rw->east,ry=s.pos.y-rw->north,h=ap.runwayHeading,along=rx*sinf(h)+ry*cosf(h),cross=rx*cosf(h)-ry*sinf(h);
 printf("%s -> %s runway %s/%s heading %.0f: max alt %.0f ft, cruise CAS %.0f kt, max over limit %+.1f kt, max Mach %.3f, max bank %.0f, min AGL en route %.0f m\n",
  from,to,rw->le,rw->he,h*57.3f,maxAlt*3.28084f,cruiseN?cruiseCas/cruiseN:0,maxOver,maxMach,maxBank*57.3f,minAgl);
 printf("touchdown sink %.2f m/s, %.0f m from aim (+ past), %.1f m right of centre, CAS %.0f, pitch %.1f roll %.1f; stopped %.0f s at %.0f m from runway centre, %.1f m off centreline (half length %.0f, half width %.0f); crashed %d; total %.0f s\n",
  tdSink,tdAlong,tdCross,tdCas,tdPitch*57.3f,tdRoll*57.3f,stopT,along,cross,rw->length/2,rw->width/2,s.crashed,t);
 int fail=s.crashed||stopT<0||maxOver>3||fabsf(along)>rw->length/2||fabsf(cross)>rw->width/2||tdSink>3;
 printf("%s\n",fail?"FAIL":"PASS");return fail;
}
