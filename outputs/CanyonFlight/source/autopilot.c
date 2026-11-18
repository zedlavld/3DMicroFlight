/* Test autopilot; see include/autopilot.h. */
#include "autopilot.h"
#include "instruments.h"
#define AP_PI 3.14159265f
#define DEG (AP_PI/180)
static float wrapAngle(float a){while(a>AP_PI)a-=2*AP_PI;while(a<-AP_PI)a+=2*AP_PI;return a;}
const char *autopilotStageName(int stage){
 static const char *names[AP_STAGE_COUNT]={"TAKEOFF","CLIMB","CRUISE","DESCENT","ORBIT"};
 return stage>=0&&stage<AP_STAGE_COUNT?names[stage]:"?";
}
float autopilotDistance(const Autopilot *ap,const Flight *s){
 const Airport *d=&airports[ap->destination];
 return sqrtf((d->east-s->pos.x)*(d->east-s->pos.x)+(d->north-s->pos.y)*(d->north-s->pos.y));
}
/* Highest terrain within 10 km ahead along the track and 1 km either side. */
static float terrainAhead(const Flight *s,float heading){
 float top=terrain(s->pos.x,s->pos.y),sx=sinf(heading),cy=cosf(heading);
 for(float d=500;d<=10000;d+=500)for(int side=-1;side<=1;side++){
  float x=s->pos.x+sx*d+cy*side*1000,y=s->pos.y+cy*d-sx*side*1000;top=fmaxf(top,terrain(x,y));
 }
 return top;
}
void autopilotHoldBegin(Autopilot *ap,const Flight *s){
 *ap=(Autopilot){0};ap->destination=-1;ap->stage=AP_CRUISE;ap->targetMsl=s->pos.z;ap->pitchBias=s->pitch;
}
void autopilotHold(Autopilot *ap,Flight *s,float dt,float heading,float altitudeMsl,float *elevator,float *aileron,float *rudder){
 float speed=sqrtf(s->vel.x*s->vel.x+s->vel.y*s->vel.y+s->vel.z*s->vel.z);
 ap->time+=dt;ap->targetMsl+=clampf(altitudeMsl-ap->targetMsl,-8*dt,8*dt);
 float vsCmd=clampf((ap->targetMsl-s->pos.z)*.08f,-8,8);
 float stall=stallSpeedCAS(s,s->roll);
 if(s->ias<stall*1.3f)vsCmd=fminf(vsCmd,(s->ias-stall*1.15f)*.5f);
 float vsErr=vsCmd-s->vel.z;
 ap->pitchBias=clampf(ap->pitchBias+dt*.004f*vsErr,-8*DEG,12*DEG);
 float pitchCmd=clampf(ap->pitchBias+asinf(clampf(vsCmd/fmaxf(speed,30),-.3f,.3f))+clampf(.006f*vsErr,-4*DEG,4*DEG),-10*DEG,15*DEG);
 if(s->stallWarning>.2f)pitchCmd=fminf(pitchCmd,s->pitch-2*DEG);
 if(s->onGround)pitchCmd=fminf(pitchCmd,aircraft[s->aircraft].tailStrike-2*DEG); /* v33: no tail strike on the runway */
 *elevator=clampf(2.5f*(pitchCmd-s->pitch)-.8f*s->q,-1,1);
 float bankCmd=clampf(wrapAngle(heading-s->yaw)*1.2f,-25*DEG,25*DEG);
 *aileron=clampf(1.5f*(bankCmd-s->roll)-.4f*s->p,-1,1);
 *rudder=clampf(-.5f*s->r,-1,1);
}
void autopilotPanelBegin(Autopilot *ap,const Flight *s){
 *ap=(Autopilot){0};ap->destination=-1;ap->stage=AP_CRUISE;ap->targetMsl=ap->holdMsl=s->pos.z;ap->holdHeading=s->yaw;
 ap->pitchBias=s->pitch;ap->throttleBias=s->throttle;
}
void autopilotPanelStep(Autopilot *ap,Flight *s,const ApPanel *p,float dt,float *elevator,float *aileron,float *rudder){
 ap->time+=dt;
 if(p->athr){
  float casErr=p->speed-s->ias;
  ap->throttleBias=clampf(ap->throttleBias+dt*.004f*casErr,0,1);
  s->throttle=clampf(ap->throttleBias+.03f*casErr,0,1);
 }
 if(!p->ap)return;
 float speed=sqrtf(s->vel.x*s->vel.x+s->vel.y*s->vel.y+s->vel.z*s->vel.z),vsCmd;
 if(p->vs&&p->alt){float limit=fmaxf(fabsf(p->verticalSpeed),1);vsCmd=clampf((p->altitude-s->pos.z)*.08f,-limit,limit);}
 else if(p->vs)vsCmd=p->verticalSpeed;
 else{
  float target=p->alt?p->altitude:ap->holdMsl;
  ap->targetMsl+=clampf(target-ap->targetMsl,-8*dt,8*dt);
  vsCmd=clampf((ap->targetMsl-s->pos.z)*.08f,-8,8);
 }
 vsCmd=clampf(vsCmd,-30,30);
 /* Climbs give way to airspeed near the stall; dives to the limit speed. */
 float stall=stallSpeedCAS(s,s->roll);
 if(s->ias<stall*1.3f)vsCmd=fminf(vsCmd,(s->ias-stall*1.15f)*.5f);
 float limit=pfdAirspeedLimit(s);if(s->ias>limit-3)vsCmd=fmaxf(vsCmd,(s->ias-limit+3)*.5f);
 float vsErr=vsCmd-s->vel.z;
 ap->pitchBias=clampf(ap->pitchBias+dt*.004f*vsErr,-8*DEG,12*DEG);
 float pitchCmd=clampf(ap->pitchBias+asinf(clampf(vsCmd/fmaxf(speed,30),-.3f,.3f))+clampf(.006f*vsErr,-4*DEG,4*DEG),-10*DEG,15*DEG);
 if(s->stallWarning>.2f)pitchCmd=fminf(pitchCmd,s->pitch-2*DEG);
 if(s->onGround)pitchCmd=fminf(pitchCmd,aircraft[s->aircraft].tailStrike-2*DEG); /* v33: no tail strike on the runway */
 *elevator=clampf(2.5f*(pitchCmd-s->pitch)-.8f*s->q,-1,1);
 float track=ap->holdHeading;
 if(p->hdg)track=p->heading;
 else if(p->nav&&p->destination>=0){const Airport *d=&airports[p->destination];track=atan2f(d->east-s->pos.x,d->north-s->pos.y);}
 float bankCmd=s->onGround?0:clampf(wrapAngle(track-s->yaw)*1.2f,-25*DEG,25*DEG);
 *aileron=clampf(1.5f*(bankCmd-s->roll)-.4f*s->p,-1,1);
 *rudder=s->onGround?*rudder:clampf(-.5f*s->r,-1,1);
}
void autopilotBegin(Autopilot *ap,const Flight *s,int destination){
 const Airport *d=&airports[destination];
 *ap=(Autopilot){0};ap->destination=destination;ap->stage=s->onGround?AP_TAKEOFF:AP_CLIMB;
 ap->arrivalMsl=d->elevation+600;
 ap->cruiseMsl=fmaxf(s->pos.z,d->elevation)+2000;
 ap->throttleBias=s->throttle;ap->targetMsl=s->pos.z;
}
void autopilotStep(Autopilot *ap,Flight *s,float dt,float *elevator,float *aileron,float *rudder){
 const Airport *d=&airports[ap->destination];
 const Aircraft *craft=&aircraft[s->aircraft];const AircraftSpeeds *v=&craft->speeds;
 ap->time+=dt;
 float dx=d->east-s->pos.x,dy=d->north-s->pos.y,distance=sqrtf(dx*dx+dy*dy);
 float agl=s->pos.z-terrain(s->pos.x,s->pos.y),speed=sqrtf(s->vel.x*s->vel.x+s->vel.y*s->vel.y+s->vel.z*s->vel.z);
 /* Lateral target: the runway heading until 150 m up, then straight to the
    destination; within 4 km, a 3 km clockwise orbit around it. */
 float track=atan2f(dx,dy);
 if(ap->stage==AP_ORBIT){float radial=atan2f(-dx,-dy);track=radial+AP_PI/2+clampf((distance-3000)/1500,-1,1)*(AP_PI/4);}
 if(ap->stage==AP_TAKEOFF||(ap->stage==AP_CLIMB&&agl<150))track=airports[s->airport].heading*DEG;
 /* Vertical target. */
 float targetMsl=ap->cruiseMsl,targetCas=v->cruise;
 float descentStart=(ap->cruiseMsl-ap->arrivalMsl)/.05f+4000;   /* ~3 degree path */
 if(ap->stage==AP_CLIMB&&s->pos.z>ap->cruiseMsl-50)ap->stage=AP_CRUISE;
 /* v33: climb out to 300 m first. Airliners reach the liftoff speed still on
    the runway, and a short route (KDVT-KPHX) would start the descent there. */
 if((ap->stage==AP_CLIMB||ap->stage==AP_CRUISE)&&distance<descentStart&&agl>300)ap->stage=AP_DESCENT;
 if(ap->stage==AP_DESCENT&&distance<4000)ap->stage=AP_ORBIT;
 if(ap->stage==AP_CLIMB)targetCas=v->climb;
 if(ap->stage==AP_DESCENT){targetMsl=fminf(ap->cruiseMsl,ap->arrivalMsl+(distance-4000)*.05f);targetCas=v->descent;}
 if(ap->stage==AP_ORBIT){targetMsl=ap->arrivalMsl;targetCas=v->orbit;}
 float floor=terrainAhead(s,s->yaw)+450;if(ap->stage!=AP_TAKEOFF)targetMsl=fmaxf(targetMsl,floor);
 /* Rate-limit the target so terrain steps under a turn don't yank the nose. */
 if(ap->stage==AP_TAKEOFF)ap->targetMsl=s->pos.z;
 else ap->targetMsl+=clampf(targetMsl-ap->targetMsl,-10*dt,(ap->stage==AP_CLIMB?25:8)*dt);
 targetMsl=ap->targetMsl;
 /* Configuration. */
 if(ap->stage==AP_TAKEOFF){
  s->throttle=1;
  if(s->ias>v->liftoff){
   ap->stage=AP_CLIMB;
   /* v33.06: hold the rotation attitude. A zero bias commanded ~4 deg and
      the airliners' elevator pitched them back onto the runway. */
   ap->pitchBias=clampf(s->pitch-asinf(clampf(6/fmaxf(speed,30),-.3f,.3f)),-8*DEG,12*DEG);
  }
 }
 if(ap->stage!=AP_TAKEOFF){
  if(craft->retractableGear&&agl>60&&s->gear&&!s->onGround)s->gear=0;
  if(agl>250&&s->flap>0)s->flap=0;
 }
 /* Pitch: altitude error -> vertical speed -> pitch attitude -> elevator. */
 float pitchCmd;
 if(ap->stage==AP_TAKEOFF)pitchCmd=0;
 else{
  float vsCmd=clampf((targetMsl-s->pos.z)*.08f,-12,ap->stage==AP_CLIMB?25:12);
  if(ap->stage==AP_CLIMB&&agl<150)vsCmd=fmaxf(vsCmd,6);
  /* Speed protection: trade climb for airspeed below 85% of the target. */
  /* v33.06: in the climb, near the liftoff speed: an airliner lifts off at
     155 kt against a 250 kt climb target, and 85% of that dived it back. */
  float vProt=ap->stage==AP_CLIMB?fminf(targetCas*.85f,v->liftoff*1.1f):targetCas*.85f;
  if(s->ias<vProt)vsCmd=fminf(vsCmd,(s->ias-vProt*.8235f)*.5f);
  float vsErr=vsCmd-s->vel.z;
  ap->pitchBias=clampf(ap->pitchBias+dt*.004f*vsErr,-8*DEG,12*DEG);
  pitchCmd=clampf(ap->pitchBias+asinf(clampf(vsCmd/fmaxf(speed,30),-.3f,.3f))+clampf(.006f*vsErr,-4*DEG,4*DEG),-10*DEG,15*DEG);
  if(s->stallWarning>.2f)pitchCmd=fminf(pitchCmd,s->pitch-2*DEG);
  if(s->onGround)pitchCmd=fminf(pitchCmd,aircraft[s->aircraft].tailStrike-2*DEG); /* v33: no tail strike on the runway */
 }
 *elevator=clampf(2.5f*(pitchCmd-s->pitch)-.8f*s->q,-1,1);
 if(ap->stage==AP_TAKEOFF&&s->ias>v->rotate)*elevator=clampf(2.5f*(fminf(8*DEG,craft->tailStrike-2*DEG)-s->pitch)-.8f*s->q,-1,1);
 /* Roll: heading error -> bank -> aileron; rudder damps yaw. */
 float headingErr=wrapAngle(track-s->yaw);
 float bankCmd=s->onGround?0:clampf(headingErr*1.2f,-25*DEG,25*DEG);
 if(ap->stage==AP_TAKEOFF||agl<60)bankCmd=clampf(bankCmd,-5*DEG,5*DEG);
 *aileron=clampf(1.5f*(bankCmd-s->roll)-.4f*s->p,-1,1);
 *rudder=s->onGround?clampf(3*headingErr,-1,1):clampf(-.5f*s->r,-1,1);
 /* Speed: throttle bias tracks the trim power, plus proportional. */
 if(ap->stage!=AP_TAKEOFF){
  float casErr=targetCas-s->ias;
  ap->throttleBias=clampf(ap->throttleBias+dt*.004f*casErr,0,1);
  s->throttle=clampf(ap->throttleBias+.03f*casErr,0,1);
  /* Climb at full power, eased off above the climb speed (F-16: out of
     afterburner instead of 690 kt at 3 deg nose up). */
  if(ap->stage==AP_CLIMB)s->throttle=clampf(1+.03f*casErr,.3f,1);
 }
}
