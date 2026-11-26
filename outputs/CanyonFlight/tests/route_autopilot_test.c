/* Host test: the v31.8 test autopilot flies a whole route (default Tucson to
   Phoenix in the SF50) at 120 Hz like the game loop, and must take off, reach
   cruise, arrive and hold the orbit without crashing or stalling.
   usage: route [from ICAO] [to ICAO] [aircraft 0/1] [trace seconds] */
#include "autopilot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "airport_index.h"
/* v1.1p20: CF_AIRPORTS_FLY=<path to airports_fly.bin> adds every flyable US airport (KDEN, KORD...). */
static int primary(const char *id){
 static int loaded;if(!loaded){loaded=1;const char *p=getenv("CF_AIRPORTS_FLY");if(p)printf("%d card runway records from %s\n",airportsLoad(p),p);}
 return airportFind(id);
}
int main(int argc,char **argv){
 const char *from=argc>1?argv[1]:"KTUS",*to=argc>2?argv[2]:"KPHX";int craft=argc>3?atoi(argv[3]):1;float trace=argc>4?atof(argv[4]):60;
 int a=primary(from),b=primary(to);if(a<0||b<0){printf("unknown airport\n");return 2;}
 Flight s;resetFlightPhase(&s,craft,a,PHASE_TAKEOFF);setWeather(&s,0);
 Autopilot ap;autopilotBegin(&ap,&s,b);
 const float dt=1/120.f;float t=0,minAgl=1e9,maxBank=0,minCas=1e9,maxWarn=0,arrive=-1,cruiseAt=-1;int failures=0,airborne=0;
 for(;t<4000&&!s.crashed;t+=dt){
  float e,r,y;autopilotStep(&ap,&s,dt,&e,&r,&y);stepFlight(&s,dt,e,r,y);
  float agl=s.pos.z-terrain(s.pos.x,s.pos.y);
  if(agl>300)airborne=1;
  if(airborne){minAgl=fminf(minAgl,agl);maxBank=fmaxf(maxBank,fabsf(s.roll));minCas=fminf(minCas,s.ias);maxWarn=fmaxf(maxWarn,s.stallWarning);}
  if(cruiseAt<0&&ap.stage==AP_CRUISE)cruiseAt=t;
  if(arrive<0&&ap.stage==AP_ORBIT)arrive=t;
  if(arrive>0&&t>arrive+300)break;
  int step=(int)(t/dt+.5f);if(step%(int)(trace*120)==0)
   printf("t %5.0f %-8s dist %6.1f km  msl %5.0f agl %5.0f  cas %4.0f  vs %5.1f  pitch %5.1f roll %5.1f hdg %5.1f thr %.2f gear %d flap %.1f\n",
    t,autopilotStageName(ap.stage),autopilotDistance(&ap,&s)/1000,s.pos.z,agl,s.ias,s.vel.z,s.pitch*57.3f,s.roll*57.3f,s.yaw*57.3f,s.throttle,s.gear,s.flap);
 }
 printf("%s -> %s: cruise reached %.0f s, arrived %.0f s, crashed %d, min AGL %.0f m, max bank %.0f deg, min CAS %.0f kt, max stall warning %.2f, final distance %.1f km\n",
  from,to,cruiseAt,arrive,s.crashed,minAgl,maxBank*57.3f,minCas,maxWarn,autopilotDistance(&ap,&s)/1000);
 if(s.crashed||arrive<0||cruiseAt<0)failures++;
 if(minAgl<150||maxBank>40/57.3f||maxWarn>.5f||autopilotDistance(&ap,&s)>6000)failures++;
 printf("%s\n",failures?"FAIL":"PASS");
 return failures!=0;
}
