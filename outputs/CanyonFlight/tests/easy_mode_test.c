/* v1.1p21 easy mode host test: the same impacts crash in normal mode and are survived in easy
   mode, for every aircraft, and each difficulty level crashes where flight.h says: a dive into
   the ground at cruise speed, an 8 m/s touchdown, a gear-up touchdown at approach speed, a
   40 degree bank touchdown and a 3.8 m/s straight drop.
   gcc -O2 -Iinclude -Itests/stream_stubs source/{flight,fbw,f16_aero,ground,terrain,terrain_stream,
       terrain_data,airports,airport_index}.c tests/easy_mode_test.c -lm */
#include "flight.h"
#include <stdio.h>
#include <math.h>
static int failures;
#define CHECK(x) do{if(!(x)){failures++;printf("FAIL %s:%d %s (craft %d case %d)\n",__FILE__,__LINE__,#x,craft,kind);}}while(0)
static void setup(Flight *s,int craft,int kind){
 resetFlightPhase(s,craft,0,kind==0?PHASE_CRUISE:PHASE_FINAL);
 /* touchdowns on the runway (the final approach start is over sloping ground) */
 if(kind){s->pos.x=airports[0].east;s->pos.y=airports[0].north;}
 const Aircraft *a=&aircraft[craft];float g=terrain(s->pos.x,s->pos.y);
 if(kind==0){s->pos.z=g+40;s->vel.z=-60;}                          /* dive into the ground */
 else{s->pos.z=g+a->gearArmZ+.4f;s->gear=1;s->gearActual=1;s->vel.z=kind==1?-8:kind==4?-3.8f:-1;
  if(kind==4){s->pos.z=g+a->gearArmZ+.03f;s->vel.x=s->vel.y=0;setAttitude(s,0,.03f,s->yaw);} /* level straight drop: 3.8 m/s at the wheels on any slope */
  if(kind==2){s->gear=0;s->gearActual=0;s->pos.z=g+a->bellyClear+.5f;} /* gear up */
  if(kind==3)setAttitude(s,.7f,s->pitch,s->yaw);}                 /* 40 degree bank */
}
/* kinds: 0 dive, 1 8 m/s touchdown, 2 gear up, 3 40 deg bank, 4 3.8 m/s drop.
   -1: either (on Forgiving the dive can pull out into a gear-down contact at landing speed, which is forgiven).
   expect[level][kind]: 1 = crashes. */
static const int expect[FLIGHT_LEVELS][5]={{0,0,0,0,0},{-1,0,1,0,0},{1,1,1,1,0},{1,1,1,1,1},{1,1,1,1,1}};
int main(void){
 int craft=0,kind=0,level=0;
 for(level=0;level<FLIGHT_LEVELS;level++)for(craft=0;craft<AIRCRAFT_COUNT;craft++)for(kind=0;kind<5;kind++){
  Flight s;flightLevel=level;flightEasyRescues=0;setup(&s,craft,kind);float low=1e9f;
  for(int i=0;i<60*20&&!s.crashed;i++){stepFlight(&s,1/60.f,0,0,0);low=fminf(low,s.pos.z-terrain(s.pos.x,s.pos.y));}
  if(expect[level][kind]>=0&&s.crashed!=expect[level][kind]){failures++;printf("FAIL level %d craft %d case %d: %s, expected %s\n",level,craft,kind,s.crashed?"crashed":"survived",expect[level][kind]?"a crash":"no crash");}
  if(!s.crashed){CHECK(low>-1);CHECK(isfinite(s.pos.z)&&isfinite(s.vel.z));if(level<2&&kind!=4)CHECK(flightEasyRescues>0);if(level==0&&kind==2)CHECK(s.gearActual>.95f);}
 }
 printf("difficulty: %d levels x %d aircraft x 5 impacts, %d failures\n",FLIGHT_LEVELS,AIRCRAFT_COUNT,failures);
 return failures!=0;
}
