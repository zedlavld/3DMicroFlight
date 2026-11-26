/* Test autopilot; see include/autopilot.h. */
#include "autopilot.h"
#include "instruments.h"
#include "airport_index.h"
#include <string.h>
#include <math.h>
#define AP_PI 3.14159265f
#define DEG (AP_PI/180)
static float wrapAngle(float a){while(a>AP_PI)a-=2*AP_PI;while(a<-AP_PI)a+=2*AP_PI;return a;}
const char *autopilotStageName(int stage){
 static const char *names[AP_STAGE_COUNT]={"TAKEOFF","CLIMB","CRUISE","DESCENT","ORBIT","APPROACH","FLARE","ROLLOUT"};
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
 ap->groundAgl=s->onGround?s->pos.z-terrain(s->pos.x,s->pos.y):3;
}
#define AP_FINAL 18000.f   /* straight-in final from this far before the touchdown point */
#ifndef AP_BETA_GAIN
#define AP_BETA_GAIN 2.f
#endif
#ifndef AP_FLARE_GAIN
#define AP_FLARE_GAIN .03f
#endif
#define AP_GLIDE .0524f     /* tan 3 degrees */
/* Landing runway: of the destination's runways long enough for the aircraft, the end with
   the most headwind in the weather at that time. */
static void chooseRunway(Autopilot *ap,const Flight *s){
 const Airport *d=&airports[ap->destination];int jet=aircraft[s->aircraft].mass>8000;
 Vec w=sampleWeatherWind(s->weather,(Vec){d->east,d->north,d->elevation+50},s->weatherTime);
 float best=-1e9f,inbound=atan2f(d->east-s->pos.x,d->north-s->pos.y); /* straight-in counts as 3 m/s of headwind */
 int first,count;airportGroup(ap->destination,&first,&count); /* v1.1p20: the airport's records */
 for(int i=first;i<first+count;i++){const Airport *r=&airports[i];
  for(int end=0;end<2;end++){
   float h=r->heading*DEG+(end?AP_PI:0),sx=sinf(h),cy=cosf(h);
   float score=-(w.x*sx+w.y*cy)-(jet&&r->length<2400?1000:0)+(r->primary?.5f:0)+3*cosf(h-inbound);
   if(score>best){best=score;ap->runway=i;ap->runwayHeading=h;
    float half=r->length*.5f;ap->aimX=r->east-sx*(half-300);ap->aimY=r->north-cy*(half-300);
    ap->aimZ=terrain(ap->aimX,ap->aimY);}
  }
 }
}
/* Distance still to fly to the touchdown point: to the start of the final, then down it. */
static void finalStart(const Autopilot *ap,float *x,float *y){*x=ap->aimX-sinf(ap->runwayHeading)*AP_FINAL;*y=ap->aimY-cosf(ap->runwayHeading)*AP_FINAL;}
/* Arriving from beyond the final start, the route first flies a downwind leg to a point 6 km
   further out and 5 km to the side it is on, then turns in to the final start. */
static void downwindPoint(const Autopilot *ap,float *x,float *y){
 float h=ap->runwayHeading,sx=sinf(h),cy=cosf(h),fx,fy;finalStart(ap,&fx,&fy);
 *x=fx-sx*6000+cy*ap->side*5000;*y=fy-cy*6000-sx*ap->side*5000;
}
static float pathToGo(const Autopilot *ap,const Flight *s){
 float fx,fy;finalStart(ap,&fx,&fy);
 if(ap->leg==0){float bx,by;downwindPoint(ap,&bx,&by);return hypotf(bx-s->pos.x,by-s->pos.y)+hypotf(fx-bx,fy-by)+AP_FINAL;}
 return hypotf(fx-s->pos.x,fy-s->pos.y)+AP_FINAL;
}
/* Leg for the current position: downwind first unless already behind the final start. */
static void planArrival(Autopilot *ap,const Flight *s){
 float h=ap->runwayHeading,rx=s->pos.x-ap->aimX,ry=s->pos.y-ap->aimY;
 float along=-(rx*sinf(h)+ry*cosf(h)),cross=rx*cosf(h)-ry*sinf(h);
 ap->side=cross>=0?1:-1;ap->leg=along>AP_FINAL+3000?1:0;
}
/* Vertical speed command -> elevator, as the route pitch loop. */
static float pitchToVs(Autopilot *ap,const Flight *s,float dt,float vsCmd,float pitchMax,float gain){
 float speed=sqrtf(s->vel.x*s->vel.x+s->vel.y*s->vel.y+s->vel.z*s->vel.z),vsErr=vsCmd-s->vel.z;
 ap->pitchBias=clampf(ap->pitchBias+dt*.004f*vsErr,-8*DEG,12*DEG);
 float pitchCmd=clampf(ap->pitchBias+asinf(clampf(vsCmd/fmaxf(speed,30),-.3f,.3f))+clampf(gain*vsErr,-4*DEG,6*DEG),-10*DEG,pitchMax);
 if(s->stallWarning>.2f)pitchCmd=fminf(pitchCmd,s->pitch-2*DEG);
 return clampf(2.5f*(pitchCmd-s->pitch)-.8f*s->q,-1,1);
}
/* v1.1p15 descent, final approach, flare and rollout (route option "land"). */
static void landingStep(Autopilot *ap,Flight *s,float dt,float *elevator,float *aileron,float *rudder){
 const Aircraft *craft=&aircraft[s->aircraft];const AircraftSpeeds *v=&craft->speeds;
 ap->time+=dt;
 float h=ap->runwayHeading,sx=sinf(h),cy=cosf(h);
 float rx=s->pos.x-ap->aimX,ry=s->pos.y-ap->aimY;
 float along=-(rx*sx+ry*cy),cross=rx*cy-ry*sx;   /* metres before the aim point; right of the centreline */
 float ground=terrain(s->pos.x,s->pos.y),agl=s->pos.z-ground,wheels=agl-ap->groundAgl; /* height of the wheels */
 float gs=hypotf(s->vel.x,s->vel.y),groundTrack=gs>5?atan2f(s->vel.x,s->vel.y):s->yaw;
 float toGo=pathToGo(ap,s);
 if(ap->stage==AP_DESCENT){
  float fx=ap->aimX-sx*AP_FINAL,fy=ap->aimY-cy*AP_FINAL;
  if(ap->leg==0){float bx,by;downwindPoint(ap,&bx,&by);if(hypotf(bx-s->pos.x,by-s->pos.y)<2500||along>AP_FINAL+5000)ap->leg=1;}
  float lined=fabsf(wrapAngle(groundTrack-h))<50*DEG&&fabsf(cross)<3000&&along<AP_FINAL+4000&&along>4000;
  if(ap->leg==1&&(lined||(hypotf(fx-s->pos.x,fy-s->pos.y)<2500&&fabsf(wrapAngle(groundTrack-h))<90*DEG)))ap->stage=AP_APPROACH;
 }
 if(ap->stage==AP_APPROACH){
  float flareAt=clampf(-s->vel.z*3,4,15);
  if(wheels<flareAt&&along<1500){ap->stage=AP_FLARE;ap->flareSink=-s->vel.z;}
 }
 if(ap->stage==AP_FLARE&&s->onGround)ap->stage=AP_ROLLOUT;
 /* Lateral: to the start of the final, then a 2.5 km look-ahead onto the centreline. */
 float track;
 if(ap->stage==AP_DESCENT){float fx,fy;if(ap->leg==0)downwindPoint(ap,&fx,&fy);else finalStart(ap,&fx,&fy);track=atan2f(fx-s->pos.x,fy-s->pos.y);}
 else track=h-atanf(cross/(ap->stage==AP_ROLLOUT?400.f:clampf(along*.25f,800,2500)));  /* look-ahead shortens near the runway */
 float trackErr=wrapAngle(track-(ap->stage==AP_ROLLOUT?s->yaw:groundTrack));
 float bankCmd=s->onGround?0:clampf(trackErr*1.4f,-25*DEG,25*DEG);
 if(wheels<60)bankCmd=clampf(bankCmd,-8*DEG,8*DEG);
 if(ap->stage==AP_FLARE)bankCmd=clampf(bankCmd,-3*DEG,3*DEG);
 *aileron=clampf(1.5f*(bankCmd-s->roll)-.4f*s->p,-1,1);
 /* Rudder damps yaw beyond a coordinated turn (damping all of it skidded the airliners
    sideways on final: banked 4 degrees, track unchanged). */
 float coordinated=9.81f*sinf(s->roll)/fmaxf(gs,30);
 /* plus an integral on sideslip: a steady yaw push (start trim at approach speeds) otherwise
    held the airliners 5-8 degrees sideways */
 if(!s->onGround)ap->rudderI=clampf(ap->rudderI+dt*1.5f*s->beta,-.4f,.4f);
 *rudder=s->onGround?clampf(3*trackErr,-1,1):clampf(-.5f*(s->r-coordinated)+AP_BETA_GAIN*s->beta+ap->rudderI,-1,1);
 /* Configuration by distance and speed: half flap below its limit inside 22 km, gear inside
    13 km, full flap inside 11 km. */
 float targetCas=v->descent;
 if(ap->stage!=AP_DESCENT){
  /* v1.1: the F-16 flies its final 15 kt fast: at 150 kt it sat at 12.5 deg alpha, already at
     the flare's pitch cap, and landed at 4.5 m/s */
  float final=v->final+(craft->controlLaw==LAW_F16?15:0);
  targetCas=along>16000?v->approach:along>9000?fminf(v->approach,craft->flapLimit[0]-15):final;
  if(along<22000&&s->ias<craft->flapLimit[0]-5&&s->flap<.5f)s->flap=.5f;
  if(along<13000&&craft->retractableGear&&s->ias<craft->gearLimit-10)s->gear=1;
  if(along<11000&&s->ias<craft->flapLimit[1]-5)s->flap=1;
 }
 targetCas=fminf(targetCas,pfdAirspeedLimit(s)-12);
 /* Vertical. */
 float vsCmd,pitchMax=15*DEG;
 if(ap->stage==AP_DESCENT){
  float target=fminf(ap->cruiseMsl,ap->aimZ+ap->groundAgl+toGo*AP_GLIDE);
  target=fmaxf(target,terrainAhead(s,s->yaw)+450);
  ap->targetMsl+=clampf(target-ap->targetMsl,-16*dt,8*dt);
  vsCmd=clampf((ap->targetMsl-s->pos.z)*.08f-gs*AP_GLIDE*(ap->targetMsl<ap->cruiseMsl-1),-15,12);
 }else if(ap->stage==AP_APPROACH){
  float path=ap->aimZ+ap->groundAgl+fmaxf(along,0)*AP_GLIDE;   /* wheels reach the runway at the aim point */
  vsCmd=clampf(-gs*AP_GLIDE+.12f*(path-s->pos.z),-12,path-s->pos.z>30?0:4); /* v1.1: well below the path (short route, low cruise): level until it meets the path */
  if(wheels<150)vsCmd=fmaxf(vsCmd,-gs*AP_GLIDE-.8f);   /* no steepening late: the flare can only take out ~4 m/s */
  ap->targetMsl=path;
 }else if(ap->stage==AP_FLARE){
  /* sink eases from the approach rate to about 0.6 m/s at the wheels */
  vsCmd=-fmaxf(.5f,fminf(ap->flareSink,wheels*.3f+.3f));pitchMax=craft->tailStrike-(craft->controlLaw==LAW_F16?1.5f:3)*DEG; /* v1.1: the F-16 flares near 11 deg alpha; 3 deg short of the tail it ran out of pitch (KSEZ 3.9 m/s) */
  /* v1.1: relative to the runway: KLAS 26 rises ~1% and the A321 touched 0.7 m/s harder than it sank */
  {float ahead=terrain(s->pos.x+sinf(groundTrack)*60,s->pos.y+cosf(groundTrack)*60);vsCmd+=clampf((ahead-ground)/60,-.03f,.03f)*gs;}
 }else vsCmd=0;
 if(ap->stage==AP_ROLLOUT){
  /* Nose down, spoilers, brakes once the nose wheel is down. */
  *elevator=clampf(2.5f*(-1*DEG-s->pitch)-.8f*s->q,-1,0);
  s->speedbrake=1;s->throttle=0;
  float b=s->pitch<2*DEG?1:0;s->brakeLeft=s->brakeRight=b;
  if(!s->onGround&&wheels>1)ap->stage=AP_FLARE;
  return;
 }
 if(ap->stage!=AP_FLARE){float vProt=targetCas*.85f;if(s->ias<vProt)vsCmd=fminf(vsCmd,(s->ias-vProt*.8235f)*.5f);}
 /* v1.1: the last 150 m of the approach track the sink command closer (.02, was .006): the A321
    reached KLAS 0.6 m/s steeper than commanded and touched at 3.4 m/s */
 *elevator=pitchToVs(ap,s,dt,vsCmd,pitchMax,ap->stage==AP_FLARE?AP_FLARE_GAIN:ap->stage==AP_APPROACH&&wheels<150?.02f:.006f);
 /* Speed: autothrottle; spoilers out in the descent when well over the target. */
 float casErr=targetCas-s->ias;
 ap->throttleBias=clampf(ap->throttleBias+dt*.004f*casErr,0,1);
 s->throttle=clampf(ap->throttleBias+.03f*casErr,0,1);
 s->speedbrake=ap->stage==AP_DESCENT&&casErr<-8?1:ap->stage==AP_APPROACH&&casErr<-8&&along>6000?1:0;
 if(ap->stage==AP_FLARE)s->throttle=fmaxf(0,s->throttle-dt*.5f)*(wheels>2);
}
void autopilotRoute(Autopilot *ap,const Flight *s,float cruiseMsl,int maxSpeed,int land){
 if(cruiseMsl>0)ap->cruiseMsl=cruiseMsl;
 ap->maxSpeed=maxSpeed;ap->land=land;
 if(land){chooseRunway(ap,s);planArrival(ap,s);}
}
void autopilotStep(Autopilot *ap,Flight *s,float dt,float *elevator,float *aileron,float *rudder){
 if(ap->land&&ap->stage>=AP_DESCENT&&ap->stage!=AP_ORBIT){landingStep(ap,s,dt,elevator,aileron,rudder);return;}
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
 if(ap->stage==AP_CRUISE&&ap->maxSpeed)targetCas=pfdAirspeedLimit(s)-5;
 /* v1.1p15 landing: descend on a 3 degree path measured along the way to the final. */
 /* v1.1: the path to go always includes the 18 km final, so from a low cruise the old test
    (path < height / glide + 6 km) never passed (KSEZ-KFLG cruised on for ever); and a short
    route can reach that point still climbing, or pass the final start without ever being that
    close to it: inside 33 km of the destination the arrival begins anyway. */
 if(ap->land){if((ap->stage==AP_CRUISE||(ap->stage==AP_CLIMB&&agl>300))&&(pathToGo(ap,s)<fmaxf((ap->cruiseMsl-ap->aimZ)/AP_GLIDE,AP_FINAL)+6000||autopilotDistance(ap,s)<AP_FINAL+15000)){chooseRunway(ap,s);planArrival(ap,s);ap->stage=AP_DESCENT;}distance=1e9f;}
 if(ap->stage==AP_CLIMB&&s->pos.z>ap->cruiseMsl-50)ap->stage=AP_CRUISE;
 /* v33: climb out to 300 m first. Airliners reach the liftoff speed still on
    the runway, and a short route (KDVT-KPHX) would start the descent there. */
 if((ap->stage==AP_CLIMB||ap->stage==AP_CRUISE)&&distance<descentStart&&agl>300)ap->stage=AP_DESCENT;
 if(ap->stage==AP_DESCENT&&distance<4000)ap->stage=AP_ORBIT;
 if(ap->stage==AP_CLIMB)targetCas=v->climb;
 if(ap->stage!=AP_TAKEOFF)targetCas=fminf(targetCas,pfdAirspeedLimit(s)-10); /* v1.1: gear and flap limits in the climb */
 if(ap->stage==AP_DESCENT){targetMsl=fminf(ap->cruiseMsl,ap->arrivalMsl+(distance-4000)*.05f);targetCas=v->descent;}
 if(ap->stage==AP_ORBIT){targetMsl=ap->arrivalMsl;targetCas=v->orbit;}
 float floor=terrainAhead(s,s->yaw)+450;if(ap->stage!=AP_TAKEOFF)targetMsl=fmaxf(targetMsl,floor);
 /* Rate-limit the target so terrain steps under a turn don't yank the nose. */
 if(ap->stage==AP_TAKEOFF)ap->targetMsl=s->pos.z;
 else ap->targetMsl+=clampf(targetMsl-ap->targetMsl,-10*dt,(ap->stage==AP_CLIMB?25:8)*dt);
 targetMsl=ap->targetMsl;
 /* Configuration. */
 if(ap->stage==AP_TAKEOFF){
  /* v1.1: the F-16 takes off at military power (0.77, f16Power): in afterburner it was at
     230 kt before it left the runway and passed its 300 kt gear limit with the gear in transit */
  s->throttle=craft->aeroModel==AERO_F16?.77f:1;
  if(s->ias>v->liftoff){
   ap->stage=AP_CLIMB;
   /* v33.06: hold the rotation attitude. A zero bias commanded ~4 deg and
      the airliners' elevator pitched them back onto the runway. */
   ap->pitchBias=clampf(s->pitch-asinf(clampf(6/fmaxf(speed,30),-.3f,.3f)),-8*DEG,12*DEG);
  }
 }
 if(ap->stage!=AP_TAKEOFF){
  if(craft->retractableGear&&(agl>60||(agl>15&&s->vel.z>1))&&s->gear&&!s->onGround)s->gear=0; /* v1.1: positive climb: the F-16 passed its 300 kt gear limit by 60 m */
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
  /* v1.1p15: high climbs (route options) keep 85% of the climb speed once 1000 m up: the
     737 slowed to 163 kt (clean stall 145) near FL330 at full power. */
  if(ap->stage==AP_CLIMB&&(ap->land||ap->maxSpeed)&&agl>1000)vProt=targetCas*.85f;
  if(s->ias<vProt)vsCmd=fminf(vsCmd,(s->ias-vProt*.8235f)*.5f);
  /* v1.1: near the limit speed, climb it off (the F-16 at full afterburner outran its throttle) */
  {float limit=pfdAirspeedLimit(s);if(s->ias>limit-6)vsCmd=fmaxf(vsCmd,fminf((s->ias-limit+6)*.8f,40));}
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
  if(ap->stage==AP_CLIMB)s->throttle=clampf(1+.03f*casErr,casErr<-8?0:.3f,craft->aeroModel==AERO_F16&&s->gearActual>.05f?.77f:1); /* v1.1: idle allowed when well over; F-16 no afterburner until the gear is up */
 }
}
