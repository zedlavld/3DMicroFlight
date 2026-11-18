/* v33.09: fly every autotest manoeuvre (include/maneuver.h) on all five
   aircraft with the production flight model and control laws, and check
   that each finishes its script without a crash and inside the control-law
   limits. Run by tools/test_maneuver.py (unicorn, no 3DS OS). */
#include "flight.h"
#include "maneuver.h"
#include "fbw.h"
#include <stdio.h>
#include <string.h>
volatile int checks,failures,firstFailure;static int runFail;
char logbuf[32768];int logLen;
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!runFail)runFail=__LINE__;if(!firstFailure)firstFailure=__LINE__;}}while(0)
#define DEG 57.29578f
float pfdAirspeedLimit(const Flight *s){(void)s;return 999;}
static int apIndex(const char *id){for(int i=0;i<AIRPORT_COUNT;i++)if(!strcmp(airports[i].id,id)&&airports[i].primary)return i;return 0;}
static void run(int type,const char *from,int phase,int kind,float seconds,int lastStage){
 Flight s;Maneuver m;DynStats d;runFail=0;
 resetFlightPhase(&s,type,apIndex(from),phase);fbwReset();maneuverBegin(&m,kind,&s);dynStatsBegin(&d,&s);
 const float dt=1.f/120;
 for(int i=0;i<(int)(seconds*120)&&!s.crashed;i++){
  float e,a,r;maneuverStep(&m,&s,dt,&e,&a,&r);stepFlight(&s,dt,e,a,r);
  const char *st=fbwStatus(&s);
  dynStatsStep(&d,&s,dt,s.pos.z-terrain(s.pos.x,s.pos.y),st&&strcmp(st,"NORMAL LAW")&&strcmp(st,"FLCS")&&strcmp(st,"GROUND LAW"));
 }
 const Aircraft *craft=&aircraft[type];
 CHECK(!s.crashed);
 CHECK(m.stage>=lastStage);
 CHECK(!m.failed);CHECK(m.kind<MAN_DIVE||m.done);
 CHECK(isfinite(s.pos.x)&&isfinite(s.pos.z)&&isfinite(s.qw));
 CHECK(phase==PHASE_FINAL?d.minAgl>15:d.minAgl>100);
 /* The 67 deg bank limit overshoots by up to 2 deg on a full-stick roll-in
    (v34.06 MAXTURN), so the bank check is 70. */
 /* SPIN holds full rudder at the stall: the Airbus law, like the real one,
    does not protect against that, so its bank and alpha limits are not
    checked there; the F-16 limiter may overshoot to 30 deg. */
 if(craft->controlLaw==LAW_AIRBUS){CHECK(d.maxNz<2.7f);CHECK(d.minNz>-1.1f);if(kind!=MAN_SPIN){CHECK(d.maxBank<70/DEG);CHECK(d.maxAlpha<aircraftAlphaMax(&s)+1/DEG);}}
 if(craft->controlLaw==LAW_F16){CHECK(d.maxNz<9.5f);CHECK(d.minNz>-3.5f);CHECK(d.maxAlpha<(kind==MAN_SPIN?30:26.5f)/DEG);}
 logLen+=snprintf(logbuf+logLen,sizeof(logbuf)-logLen,"%s %s %s: stage %d nz %.2f/%.2f alpha %.1f cas %.0f-%.0f bank %.0f p %.0f alt %+.0f/%+.0f end %+.0f stall %.1f prot %.1f yaw %.0f %s%s%s\n",
  craft->name,from,maneuverName(kind),m.stage,d.minNz,d.maxNz,d.maxAlpha*DEG,d.minCas,d.maxCas,d.maxBank*DEG,d.maxRollRate*DEG,
  d.minAlt-d.startAlt,d.maxAlt-d.startAlt,s.pos.z-d.startAlt,d.stallTime,d.protTime,d.maxYawRate*DEG,maneuverReport(&m),s.crashed?" CRASHED":"",runFail?" <<FAIL":"");
}
int main(void){
 static const char *home[AIRCRAFT_COUNT]={"KSEZ","KFLG","KPHX","KLAS","KTUS"};
 for(int t=0;t<AIRCRAFT_COUNT;t++){
  run(t,home[t],PHASE_STALL,MAN_STALL,35,3);
  run(t,home[t],PHASE_CRUISE,MAN_TURN,70,2);
  run(t,home[t],PHASE_CRUISE,MAN_PULL,20,4);
  run(t,home[t],PHASE_CRUISE,MAN_ROLL,24,5);
  run(t,home[t],PHASE_APPROACH,MAN_CONFIG,48,0);
  run(t,home[t],PHASE_FINAL,MAN_GOAROUND,30,1);
 }
 /* v34.06 extreme envelope: every aircraft; each must reach its level-flight end (m.done) */
 for(int t=0;t<AIRCRAFT_COUNT;t++){
  run(t,home[t],PHASE_CRUISE,MAN_DIVE,120,0);
  run(t,home[t],PHASE_CRUISE,MAN_SPIN,160,0);
  run(t,home[t],PHASE_CRUISE,MAN_SPIRAL,60,0);
  run(t,home[t],PHASE_CRUISE,MAN_MAXTURN,110,0);
 }
 /* F-16 aerobatics */
 run(4,"KTUS",PHASE_CRUISE,MAN_LOOP,60,0);
 run(4,"KTUS",PHASE_CRUISE,MAN_IMMELMANN,50,0);
 run(4,"KTUS",PHASE_CRUISE,MAN_SPLITS,70,0);
 run(4,"KTUS",PHASE_CRUISE,MAN_INVERTED,40,0);
 run(4,"KTUS",PHASE_CRUISE,MAN_VERTICAL,130,0);
 return 0;
}
