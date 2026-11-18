/* v33 flight control laws.

   LAW_AIRBUS, after the A320-family normal law (simplified, not certified
   logic):
   - Pitch: the stick commands load factor, 1 g at neutral (path hold) up
     to +2.5 g / 0 g at the stops (clean), with bank compensation up to
     33 deg. Auto-trim is the integrator. Protections: high angle of attack
     (above alpha-prot the stick commands alpha, at most alpha-max), high
     speed (nose-up demand above VMO + 6 kt), pitch attitude (+30 / -15 deg).
   - Roll: the stick commands roll rate, up to 15 deg/s; neutral stick holds
     the bank up to 33 deg and rolls back to 33 deg above it; 67 deg limit.
   - Yaw: turn coordination and yaw damping on the rudder.
   On the ground the law is direct (stick to surfaces), as Airbus ground mode.
   Gains are scaled with dynamic pressure (reference 250 kt).

   LAW_DIRECT with yawDamper (737): stick to surfaces, plus a yaw damper. */
#include "fbw.h"
#include <string.h>
#define PI 3.14159265359f
#define DEG (PI/180)
#define KNOT .514444444f
#define G 9.80665f
static struct {int active,aircraft;float pitchInt,rollInt,bankHold;const char *status;} st;
void fbwReset(void){memset(&st,0,sizeof(st));}
static float sq(float x){return x*x;}
/* v34.06: rate of the flight path in the pitch plane, and alpha rate as
   pitch rate minus that. The protections used body pitch rate as the alpha
   lead, but in a steep turn most of the pitch rate is the turn: alpha
   protection fired at 6 deg alpha in a 70 deg bank (A321 held to 1.8 g)
   and the F-16 AoA limiter capped it at 7.6 g. */
static float trueAirspeed(const Flight *s){float vx=s->vel.x-s->wind.x,vy=s->vel.y-s->wind.y,vz=s->vel.z-s->wind.z;return fmaxf(sqrtf(vx*vx+vy*vy+vz*vz),30);}
static float pathRate(const Flight *s){return G*(s->loadFactor-cosf(s->pitch)*cosf(s->roll))/trueAirspeed(s);}
const char *fbwStatus(const Flight *s){
 const Aircraft *a=&aircraft[s->aircraft];
 if(a->controlLaw==LAW_DIRECT)return NULL;
 if(s->onGround)return "GROUND LAW";
 return st.status?st.status:(a->controlLaw==LAW_F16?"FLCS":"NORMAL LAW");
}
static void yawDamper(const Flight *s,float scale,float gain,float limit,float *rudder){
 /* v34.06: turn rate from true airspeed (was CAS: at altitude it asked for
    too much yaw rate; 10 deg of sideslip in a 67 deg A321 bank) */
 float rCoord=G*tanf(clampf(s->roll,-1.2f,1.2f))/trueAirspeed(s)*cosf(s->pitch);
 *rudder=clampf(*rudder+clampf(scale*(gain*(rCoord-s->r)-1.5f*s->beta),-limit,limit),-1,1);
}
/* LAW_F16, after the F-16 FLCS (simplified): the stick commands load
   factor, +9 / -3 g at the stops, blended toward pitch rate at low speed;
   an angle-of-attack limiter holds 25 deg; the airframe is neutrally
   stable in pitch (cg 0.35), so the pitch loops also stabilise it. Roll
   stick commands roll rate, 308 deg/s at low AoA, reduced above 15 deg;
   neutral stick holds the bank. Yaw damper and aileron-rudder
   interconnect on the rudder. Gains scaled by dynamic pressure (400 kt). */
static void f16Law(Flight *s,float dt,float *elevator,float *aileron,float *rudder){
 float qbar=fmaxf(.5f*1.225f*sq(s->ias*KNOT),600),qref=.5f*1.225f*sq(400*KNOT);
 float scale=clampf(qref/qbar,.5f,6);
 if(!st.active){st.active=1;st.pitchInt=clampf(s->trim+s->elevator,-.6f,.6f);st.rollInt=0;st.bankHold=s->roll;}
 st.status=NULL;
 float stick=clampf(*elevator,-1,1),v=fmaxf(s->ias*KNOT,40);
 float comp=1/cosf(clampf(s->roll,-60*DEG,60*DEG));
 /* v34.06: +9 / -3 g absolute at the stops (was comp + 8 / comp - 3, so
    -2 g wings level and 10 g in a 60 deg bank). */
 float nzCmd=comp+(stick>0?(9-comp)*stick:(comp+3)*stick);
 float qCmd=clampf(1.5f*G*(nzCmd-s->loadFactor)/v+G*(nzCmd-comp)/v,-1,1);
 const float alphaLimit=25*DEG;
 float qPath=pathRate(s),alphaRate=s->q-qPath;
 if(s->alpha+.35f*alphaRate>alphaLimit-6*DEG){
  float qAlpha=qPath+2.5f*(alphaLimit-1*DEG-s->alpha-.35f*alphaRate);
  if(qAlpha<qCmd){qCmd=qAlpha;st.status="AOA LIMIT";}
 }
 if(s->loadFactor>9){st.status="G LIMIT";}
 float qErr=qCmd-s->q,surface=st.pitchInt+.9f*scale*qErr;
 /* Anti-windup: no integration further into a saturated surface. */
 if(!((surface>=1&&qErr>0)||(surface<=-1&&qErr<0)))st.pitchInt=clampf(st.pitchInt+dt*.8f*scale*qErr,-.8f,.8f);
 s->trim=st.pitchInt;
 *elevator=clampf(.9f*scale*qErr,-1,1);
 /* Roll */
 float rollStick=clampf(*aileron,-1,1),pMax=308*DEG*(1-.8f*clampf((s->alpha-15*DEG)/(10*DEG),0,1)),pCmd;
 if(fabsf(rollStick)>.06f){pCmd=rollStick*pMax;st.bankHold=s->roll;}
 /* v34.06: no bank hold above 60 deg of pitch. The Euler bank flips by
    180 deg through the vertical, so holding it rolled the jet half over in
    every loop; there the law holds zero roll rate and re-latches the bank. */
 else if(fabsf(s->pitch)>60*DEG){pCmd=0;st.bankHold=s->roll;}
 else pCmd=clampf(1.5f*remainderf(st.bankHold-s->roll,2*PI),-30*DEG,30*DEG);
 float pErr=pCmd-s->p;st.rollInt=clampf(st.rollInt+dt*1.5f*scale*pErr,-.5f,.5f);
 *aileron=clampf(st.rollInt+.6f*scale*pErr,-1,1);
 s->aileronTrim=0;
 /* Yaw: damper, turn coordination and aileron-rudder interconnect.
    v34.06: pedal authority fades out from 15 to 25 deg alpha, as the real
    FLCS ignores pedals at high AoA; full pedal at the alpha limit departed
    the jet into a spin (alpha 156 deg). */
 *rudder*=1-clampf((s->alpha-15*DEG)/(10*DEG),0,1);
 float rCoord=G*tanf(clampf(s->roll,-1.3f,1.3f))/trueAirspeed(s)*cosf(s->pitch);
 *rudder=clampf(*rudder+clampf(scale*(.8f*(rCoord-s->r)-1.0f*s->beta)+.1f*(*aileron)*sinf(s->alpha),-.5f,.5f),-1,1);
}
void fbwControl(Flight *s,float dt,float *elevator,float *aileron,float *rudder){
 const Aircraft *a=&aircraft[s->aircraft];
 if(st.aircraft!=s->aircraft){fbwReset();st.aircraft=s->aircraft;}
 float qbar=fmaxf(.5f*1.225f*sq(s->ias*KNOT),400),qref=.5f*1.225f*sq(250*KNOT);
 float scale=clampf(qref/qbar,.35f,4);
 if(a->controlLaw==LAW_DIRECT){
  if(a->yawDamper&&!s->onGround)yawDamper(s,scale,1.2f,.25f,rudder);
  return;
 }
 if(s->onGround){st.active=0;st.status=NULL;return;}
 if(a->controlLaw==LAW_F16){f16Law(s,dt,elevator,aileron,rudder);return;}
 if(!st.active){st.active=1;st.pitchInt=clampf(s->trim+s->elevator,-.8f,.8f);st.rollInt=0;st.bankHold=s->roll;}
 st.status=NULL;
 /* ---- pitch: load-factor demand */
 float stick=clampf(*elevator,-1,1);
 float comp=1/cosf(clampf(s->roll,-33*DEG,33*DEG));
 float nzCmd=comp+(stick>0?1.5f*stick:1.0f*stick);
 /* v34.06: the load-factor limits are absolute: +2.5 / -1 g clean, +2 / 0 g
    with flaps. In a bank comp + 1.5 reached 2.69 g. */
 float nzMax=s->flapActual>.01f?2:2.5f;
 nzCmd=s->flapActual>.01f?clampf(nzCmd,0,2):clampf(nzCmd,-1,2.5f);
 if(s->pitch>25*DEG)nzCmd-=2.0f*clampf((s->pitch-25*DEG)/(5*DEG),0,1.5f);
 if(s->pitch<-10*DEG)nzCmd+=1.5f*clampf((-10*DEG-s->pitch)/(5*DEG),0,1.5f);
 if(s->ias>a->vmo+6){nzCmd+=clampf((s->ias-a->vmo-6)*.06f,0,1);st.status="HIGH SPEED PROT";}
 /* Outer loop: load-factor error -> pitch-rate demand (rad/s). */
 float v=fmaxf(s->ias*KNOT,40);
 float qCmd=clampf(1.6f*G*(nzCmd-s->loadFactor)/v+G*(nzCmd-comp)/v,-.25f,.25f);
 /* v34.06: load-factor guard; a steep max-g turn overshot to 2.84 g */
 if(s->loadFactor>nzMax-.1f)qCmd=fminf(qCmd,s->q-3*G*(s->loadFactor-nzMax+.1f)/v);
 /* v34.04: alpha prot 3 deg and the alpha limit 2.5 deg under the stall angle
    (prot from about 1.11 VS1g; with the overshoot of a full pull the A321
    peaks about 1 deg under the stall). It was 4 and 2
    deg: prot from ~1.17 VS, so the A321 sat in ALPHA PROT at 170 kt clean. */
 float alMax=aircraftAlphaMax(s),alProt=alMax-3*DEG,alLimit=alMax-2.5f*DEG;
 float qPath=pathRate(s),alphaRate=s->q-qPath;
 if(s->alpha+.5f*alphaRate>alProt-1*DEG){
  float alphaCmd=alProt+fmaxf(stick,0)*(alLimit-alProt);
  float qAlpha=qPath+1.8f*(alphaCmd-s->alpha-.5f*alphaRate);
  if(qAlpha<qCmd){qCmd=qAlpha;st.status="ALPHA PROT";}
 }
 /* Inner loop: pitch rate with integrator (the integrator is the auto-trim). */
 float qErr=qCmd-s->q,surface=st.pitchInt+1.8f*scale*qErr;
 if(!((surface>=1&&qErr>0)||(surface<=-1&&qErr<0)))st.pitchInt=clampf(st.pitchInt+dt*1.2f*scale*qErr,-.9f,.9f);
 s->trim=st.pitchInt;
 *elevator=clampf(1.8f*scale*qErr,-1,1);
 /* ---- roll: rate demand, bank hold to 33 deg, 67 deg limit */
 float rollStick=clampf(*aileron,-1,1),pCmd;
 if(fabsf(rollStick)>.06f){pCmd=rollStick*15*DEG;st.bankHold=s->roll;}
 else{
  if(fabsf(st.bankHold)>33*DEG)st.bankHold=copysignf(33*DEG,st.bankHold);
  pCmd=clampf(.9f*(st.bankHold-s->roll),-15*DEG,15*DEG);
 }
 /* 67 deg bank limit: the rate demand tapers to zero at the limit. */
 if(pCmd*s->roll>0)pCmd=copysignf(fminf(fabsf(pCmd),fmaxf(0,(67*DEG-fabsf(s->roll)-.25f*fabsf(s->p))*2.5f)),pCmd); /* v34.06: roll-rate lead, gain 2.5 (was 1.2): it overshot to 71 deg */
 if(fabsf(s->roll)>67*DEG)pCmd=-3*(s->roll-copysignf(67*DEG,s->roll)); /* v34.06: was 1x, it sat at 71 deg */
 float pErr=pCmd-s->p;st.rollInt=clampf(st.rollInt+dt*5.0f*scale*pErr,-.8f,.8f);
 *aileron=clampf(st.rollInt+3.5f*scale*pErr,-1,1);
 /* Trim inputs are not used in normal law. */
 s->aileronTrim=0;
 /* ---- yaw */
 yawDamper(s,scale,1.6f,.35f,rudder);
}
