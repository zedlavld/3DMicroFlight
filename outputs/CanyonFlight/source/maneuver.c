/* v33.09 scripted dynamics manoeuvres; see include/maneuver.h. */
#include "maneuver.h"
#include "fbw.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#define PI 3.14159265359f
#define DEG (PI/180)
static const char *names[MAN_COUNT]={"NONE","STALL","TURN","PULL","ROLL","CONFIG","GOAROUND",
 "DIVE","SPIN","SPIRAL","MAXTURN","LOOP","IMMELMANN","SPLITS","INVERTED","VERTICAL"};
int maneuverIndex(const char *name){for(int k=1;k<MAN_COUNT;k++)if(!strcasecmp(name,names[k]))return k;return 0;}
const char *maneuverName(int kind){return kind>=0&&kind<MAN_COUNT?names[kind]:"?";}
void maneuverBegin(Maneuver *m,int kind,Flight *s){
 /* v34.06: room under the aircraft for the manoeuvres that lose height */
 s->pos.z+=kind==MAN_SPIN||kind==MAN_SPLITS?2500:kind==MAN_DIVE?2000:kind==MAN_SPIRAL?1500:0;
 memset(m,0,sizeof(*m));m->kind=kind;m->startAlt=s->pos.z;m->startCas=s->ias;m->startYaw=m->lastYaw=s->yaw;m->nzFiltered=s->loadFactor;
}
static int law(const Flight *s){return aircraft[s->aircraft].controlLaw;}
/* Stick for a load-factor target. The FBW laws take load factor directly
   (feed-forward through their stick gradient); direct-law aircraft get an
   integrator on the load-factor error, scaled with dynamic pressure. */
static float pitchFor(Maneuver *m,const Flight *s,float dt,float nzDes){
 float err=nzDes-s->loadFactor;
 if(law(s)==LAW_AIRBUS){
  float comp=1/cosf(clampf(s->roll,-33*DEG,33*DEG)),d=nzDes-comp;
  m->stick=clampf(m->stick+dt*.3f*err,-.3f,.3f);
  return clampf((d>0?d/1.5f:d)+m->stick,-1,1);
 }
 if(law(s)==LAW_F16){
  float comp=1/cosf(clampf(s->roll,-60*DEG,60*DEG)),d=nzDes-comp;
  m->stick=clampf(m->stick+dt*.05f*err,-.2f,.2f);
  return clampf((d>0?d/(9-comp):d/(comp+3))+m->stick,-1,1);
 }
 /* Direct law: the short period is quick and lightly damped, so the loop
    works on a 0.4 s filtered load factor with a gentle integrator. */
 if(!m->noGuard&&s->alpha>aircraftAlphaMax(s)-3*DEG)nzDes=fminf(nzDes,m->nzFiltered-.5f); /* a pilot eases off before the stall */
 m->nzFiltered+=(s->loadFactor-m->nzFiltered)*fminf(1,dt/.4f);err=nzDes-m->nzFiltered;
 float speed=fmaxf(s->ias,40),scale=clampf((110/speed)*(110/speed),.15f,3);
 m->stick=clampf(m->stick+dt*.15f*scale*err,-1,1);
 return clampf(m->stick+.02f*scale*err,-1,1);
}
/* Load factor that holds an altitude (and a pitch ceiling) in a bank. */
static float holdAltitude(const Flight *s,float alt){
 float bank=clampf(s->roll,-60*DEG,60*DEG);
 return fminf(2.2f,cosf(s->pitch)/cosf(bank)+clampf(.004f*(alt-s->pos.z)-.05f*s->vel.z,-.6f,.6f));
}
static float holdPitch(const Flight *s,float pitch){
 return cosf(s->pitch)/cosf(clampf(s->roll,-60*DEG,60*DEG))+clampf(2.5f*(pitch-s->pitch)-.8f*s->q,-.8f,1.2f);
}
static float rollFor(const Flight *s,float bankCmd){
 float rMax=law(s)==LAW_F16?60*DEG:law(s)==LAW_DIRECT?30*DEG:20*DEG,pDes=clampf(1.2f*remainderf(bankCmd-s->roll,2*PI),-rMax,rMax);
 if(law(s)==LAW_AIRBUS)return clampf((pDes+.5f*(pDes-s->p))/(15*DEG),-1,1);
 if(law(s)==LAW_F16)return clampf((pDes+(pDes-s->p))/(308*DEG),-1,1);
 return clampf(1.2f*(pDes-s->p),-1,1);
}
/* v34.06 helpers for the envelope manoeuvres */
static float limitLoad(const Flight *s){const Aircraft *a=&aircraft[s->aircraft];
 return a->controlLaw==LAW_F16?8.8f:a->engine==ENGINE_PISTON||a->mesh==1?3.8f:2.5f;}
/* 1 g along the vertical through a roll: -1 g when inverted. */
static float rollNz(const Flight *s){return cosf(s->roll)*cosf(s->pitch);}
static float holdInverted(const Flight *s,float alt){return -(1+clampf(.004f*(alt-s->pos.z)-.05f*s->vel.z,-.8f,.8f));}
static float headingChange(const Maneuver *m,const Flight *s){return remainderf(s->yaw-m->startYaw,2*PI)/DEG;}
static void holdSpeed(Flight *s,float dt,float cas){s->throttle=clampf(s->throttle+dt*.012f*(cas-s->ias),0,1);}
static void setGear(Flight *s,int down){if(aircraft[s->aircraft].retractableGear&&!s->onGround)s->gear=down;}
static void next(Maneuver *m){m->stage++;m->stageTime=0;}
void maneuverStep(Maneuver *m,Flight *s,float dt,float *pitchIn,float *rollIn,float *rudderIn){
 m->t+=dt;m->stageTime+=dt;*rudderIn=0;
 m->turned+=remainderf(s->yaw-m->lastYaw,2*PI);m->lastYaw=s->yaw;m->pitched+=s->q*dt;
 float alphaMax=aircraftAlphaMax(s);
 switch(m->kind){
 case MAN_STALL:
  /* 0 settle; 1 idle, hold altitude to the stall (or the law's limit); 2 recover; 3 level */
  if(m->stage==0){*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));*rollIn=rollFor(s,0);if(m->stageTime>2)next(m);}
  else if(m->stage==1){
   s->throttle=0;*rollIn=rollFor(s,0);m->noGuard=1;
   float nz=holdAltitude(s,m->startAlt);
   *pitchIn=law(s)==LAW_DIRECT?pitchFor(m,s,dt,nz):clampf(m->stageTime/4,0,1); /* FBW: stick walked to the stop */
   int stalled=s->alpha>alphaMax-.5f*DEG||s->stallWarning>.95f;
   if((stalled&&m->stageTime>3)||m->stageTime>18)next(m);
  }else if(m->stage==2){
   s->throttle=1;*rollIn=rollFor(s,0);m->noGuard=0;
   *pitchIn=pitchFor(m,s,dt,.6f);
   if((s->alpha<alphaMax-6*DEG&&m->stageTime>1)||m->stageTime>5){m->stick=0;next(m);}
  }else{
   if(m->stageTime<3)s->throttle=1;else holdSpeed(s,dt,m->startCas*1.3f);
   *rollIn=rollFor(s,0);
   *pitchIn=pitchFor(m,s,dt,holdPitch(s,s->vel.z<0?8*DEG:3*DEG));
  }
  break;
 case MAN_TURN:{
  /* 0 settle; 1 roll in and turn 360 deg; 2 roll out; 3 level */
  float bank=law(s)==LAW_F16?60*DEG:45*DEG;
  holdSpeed(s,dt,m->startCas);
  if(m->stage==0){m->bankCmd=0;if(m->stageTime>2){m->turned=0;next(m);}}
  else if(m->stage==1){m->bankCmd=bank;if(fabsf(m->turned)>2*PI-25*DEG||m->stageTime>60)next(m);}
  else m->bankCmd=0;
  *rollIn=rollFor(s,m->bankCmd);*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));
  break;}
 case MAN_PULL:
  /* 0 settle; 1 full aft stick 4 s (direct law: pull to the limit load
     and hold it); 2 level the nose 6 s; 3 full forward 1 s (direct law:
     push to 0 g); 4 level. */
  *rollIn=rollFor(s,0);
  {int direct=law(s)==LAW_DIRECT;float limit=aircraft[s->aircraft].engine==ENGINE_PISTON?3.8f:aircraft[s->aircraft].mesh==1?3.8f:2.5f;
  if(m->stage==0){*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));if(m->stageTime>2)next(m);}
  else if(m->stage==1){*pitchIn=direct?clampf(m->stick+=dt*(m->nzFiltered<limit&&s->alpha<alphaMax-2*DEG?.3f:-.3f),-1,1):1;
   m->nzFiltered+=(s->loadFactor-m->nzFiltered)*fminf(1,dt/.15f);if(m->stageTime>4){m->stick=0;next(m);}}
  else if(m->stage==2){*pitchIn=pitchFor(m,s,dt,holdPitch(s,2*DEG));if(m->stageTime>6)next(m);}
  else if(m->stage==3){*pitchIn=direct?clampf(m->stick-=dt*(m->nzFiltered>.3f?.6f:-.6f),-1,1):-1;
   m->nzFiltered+=(s->loadFactor-m->nzFiltered)*fminf(1,dt/.15f);if(m->stageTime>1.5f){m->stick=0;next(m);}}
  else *pitchIn=pitchFor(m,s,dt,holdPitch(s,2*DEG));}
  break;
 case MAN_ROLL:
  /* 0 settle; 1 full right stick 4 s (direct law: to 60 deg; F-16: one
     full roll); 2 neutral 4 s (bank hold / spiral); 3 level 4 s; 4 full
     left 2 s (direct law: to 45 deg; F-16: one roll); 5 level */
  *pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));
  m->rolled+=fabsf(s->p)*dt;
  if(m->stage==0){*rollIn=rollFor(s,0);if(m->stageTime>2){m->rolled=0;next(m);}}
  else if(m->stage==1){*rollIn=1;if(m->stageTime>4||(law(s)==LAW_DIRECT&&s->roll>60*DEG)||(law(s)==LAW_F16&&m->rolled>2*PI-20*DEG))next(m);}
  else if(m->stage==2){*rollIn=0;if(m->stageTime>4)next(m);}
  else if(m->stage==3){*rollIn=rollFor(s,0);if(m->stageTime>4){m->rolled=0;next(m);}}
  else if(m->stage==4){*rollIn=-1;if(m->stageTime>2||(law(s)==LAW_DIRECT&&s->roll<-45*DEG)||(law(s)==LAW_F16&&m->rolled>2*PI-20*DEG))next(m);}
  else *rollIn=rollFor(s,0);
  break;
 case MAN_CONFIG:{
  /* gear down 3 s, flaps one step 8 s, flaps full and slow to final speed
     14 s, accelerate with flaps one step 26 s, flaps up 34 s, gear up 42 s */
  const Aircraft *a=&aircraft[s->aircraft];float step=a->flapSteps>0?1.f/a->flapSteps:1;
  float target=m->startCas;
  if(m->t>3)setGear(s,1);
  if(m->t>8)s->flap=step;
  if(m->t>14&&m->t<26){s->flap=1;target=fmaxf(a->speeds.final,m->startCas*.85f);}
  if(m->t>34){s->flap=0;target=fmaxf(m->startCas,1.3f*stallSpeedCAS(s,0));}
  if(m->t>42)setGear(s,0);
  holdSpeed(s,dt,target);
  *rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));
  break;}
 case MAN_GOAROUND:{
  /* 0 on the glide path 4 s hands-off; 1 full power, pitch 12 deg; flaps one step up and gear up when climbing */
  const Aircraft *a=&aircraft[s->aircraft];float step=a->flapSteps>0?1.f/a->flapSteps:1;
  *rollIn=rollFor(s,0);
  if(m->stage==0){*pitchIn=0;if(m->stageTime>4)next(m);}
  else{
   if(m->stageTime<10)s->throttle=1;else holdSpeed(s,dt,m->startCas*1.25f);
   *pitchIn=pitchFor(m,s,dt,holdPitch(s,12*DEG));
   if(m->stageTime>3&&s->vel.z>1){s->flap=fmaxf(0,fminf(s->flap,1-step));setGear(s,0);}
   if(m->stageTime>15)s->flap=0;
  }
  break;}
 case MAN_DIVE:{
  /* 0 settle; 1 full power, dive past VMO + 6% (F-16: to its 700 kt limit, it loses 4 km); 2 idle, speedbrake, pull at 80% limit load; 3 level */
  const Aircraft *a=&aircraft[s->aircraft];
  float dive=law(s)==LAW_F16?-30*DEG:a->engine==ENGINE_PISTON?-25*DEG:a->mesh==1?-20*DEG:-12*DEG;
  *rollIn=rollFor(s,0);m->metric[0]=fmaxf(m->metric[0],s->ias);m->metric[1]=a->vmo;
  if(m->stage==0){*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));if(m->stageTime>2)next(m);}
  else if(m->stage==1){s->throttle=1;*pitchIn=pitchFor(m,s,dt,holdPitch(s,dive));if(s->ias>a->vmo*(law(s)==LAW_F16?1:1.06f)||m->stageTime>45){m->metric[2]=m->stageTime;next(m);}}
  else if(m->stage==2){s->throttle=0;s->speedbrake=1;*pitchIn=pitchFor(m,s,dt,.8f*limitLoad(s));if((s->pitch>0&&m->stageTime>1)||m->stageTime>20)next(m);}
  else{s->speedbrake=0;holdSpeed(s,dt,m->startCas);*pitchIn=pitchFor(m,s,dt,holdPitch(s,2*DEG));if(m->stageTime>8)m->done=1;}
  break;}
 case MAN_SPIN:
  /* 0 settle; 1 idle to the stall; 2 full aft stick + full right rudder 6 s;
     3 PARE recovery until the rotation stops; 4 pull out; 5 level */
  if(m->stage==0){*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));*rollIn=rollFor(s,0);if(m->stageTime>2)next(m);}
  else if(m->stage==1){
   /* idle and speedbrake at constant altitude to 1.3 VS first (bankCmd
      marks it), so the FBW stick walk does not zoom from cruise speed */
   s->throttle=0;*rollIn=rollFor(s,0);
   if(!m->bankCmd){s->speedbrake=1;*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));if(s->ias<1.3f*stallSpeedCAS(s,0)||m->stageTime>60){m->bankCmd=1;m->stageTime=0;s->speedbrake=0;}break;}
   m->noGuard=1;
   *pitchIn=law(s)==LAW_DIRECT?pitchFor(m,s,dt,holdAltitude(s,m->startAlt)):clampf(m->stageTime/4,0,1);
   if(((s->alpha>alphaMax-.5f*DEG||s->stallWarning>.95f)&&m->stageTime>3)||m->stageTime>18){m->turned=0;next(m);}
  }else if(m->stage==2){s->throttle=0;*pitchIn=1;*rollIn=0;*rudderIn=1;m->metric[0]=fabsf(m->turned)/(2*PI);if(m->stageTime>6)next(m);}
  else if(m->stage==3){
   s->throttle=0;*pitchIn=-.6f;*rollIn=0;*rudderIn=s->r>3*DEG?-1:0; /* opposite rudder while it still yaws, then neutral */m->metric[0]=fabsf(m->turned)/(2*PI);m->metric[1]=m->stageTime;
   if((fabsf(s->r)<8*DEG&&s->alpha<alphaMax-4*DEG&&m->stageTime>.5f)||m->stageTime>15){if(m->stageTime>15)m->failed=1;m->stick=0;m->noGuard=0;next(m);}
  }else if(m->stage==4){s->throttle=0;*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,fabsf(s->roll)>60*DEG?.5f:.8f*limitLoad(s));if((s->pitch>-3*DEG&&m->stageTime>1)||m->stageTime>20)next(m);}
  else{if(m->stageTime<3)s->throttle=1;else holdSpeed(s,dt,m->startCas);*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,holdPitch(s,3*DEG));if(m->stageTime>8)m->done=1;}
  break;
 case MAN_SPIRAL:
  /* 0 settle; 1 full right aileron, stick where it was trimmed, to 120 deg of bank;
     2 hands off 4 s; 3 recover: idle, speedbrake, wings level, pull; 4 level */
  if(m->stage==0){*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));*rollIn=rollFor(s,0);if(m->stageTime>2){m->metric[0]=law(s)==LAW_DIRECT?m->stick:0;next(m);}}
  else if(m->stage==1){*pitchIn=m->metric[0];*rollIn=1;if(fabsf(s->roll)>120*DEG||m->stageTime>4)next(m);}
  else if(m->stage==2){*pitchIn=m->metric[0];*rollIn=0;if(m->stageTime>4){m->metric[1]=fabsf(s->roll)/DEG;m->metric[2]=s->pitch/DEG;m->stick=m->metric[0];next(m);}}
  else if(m->stage==3){
   s->throttle=0;s->speedbrake=1;*rollIn=rollFor(s,0);
   *pitchIn=pitchFor(m,s,dt,fabsf(s->roll)>60*DEG?.5f:fabsf(s->roll)>20*DEG?1.5f:.8f*limitLoad(s));
   if((fabsf(s->roll)<10*DEG&&s->pitch>-2*DEG&&m->stageTime>1)||m->stageTime>25){if(m->stageTime>25)m->failed=1;next(m);}
  }else{s->speedbrake=0;holdSpeed(s,dt,m->startCas);*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,holdPitch(s,2*DEG));if(m->stageTime>8)m->done=1;}
  break;
 case MAN_MAXTURN:{
  /* 0 settle; 1 full power, limit load, bank for level flight, 360 deg; 2 roll out, level */
  if(m->stage==0){*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));*rollIn=rollFor(s,0);if(m->stageTime>2){m->turned=0;next(m);}}
  else if(m->stage==1){
   s->throttle=1;
   float level=acosf(clampf(cosf(s->pitch)/fmaxf(s->loadFactor,1.05f),0,1));
   float bank=clampf(level+clampf(.002f*(s->pos.z-m->startAlt)+.02f*s->vel.z,-.35f,.35f),0,85*DEG);
   *rollIn=rollFor(s,bank);
   if(law(s)==LAW_DIRECT){
    /* Direct law: fly the lower of the limit load and 3 deg under the
       stall angle (a pilot on the stall warning), stick integrator. */
    m->nzFiltered+=(s->loadFactor-m->nzFiltered)*fminf(1,dt/.4f);
    float err=fminf(alphaMax-3*DEG-s->alpha,(limitLoad(s)-m->nzFiltered)*1.5f*DEG);
    float k=clampf((110/fmaxf(s->ias,40))*(110/fmaxf(s->ias,40)),.15f,3); /* stick authority grows with q */
    m->stick=clampf(m->stick+dt*4*k*err,-1,1);*pitchIn=clampf(m->stick+k*(2*err-.15f*s->q),-1,1);
   }else *pitchIn=pitchFor(m,s,dt,limitLoad(s));
   m->metric[0]=m->stageTime;
   if(fabsf(m->turned)>=2*PI||m->stageTime>90){if(m->stageTime>90)m->failed=1;m->stick=0;next(m);}
  }else{holdSpeed(s,dt,m->startCas);*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));if(m->stageTime>8)m->done=1;}
  break;}
 case MAN_LOOP:case MAN_IMMELMANN:
  /* 0 settle; 1 5.5 g pull through 360 deg (Immelmann: 180 deg); 2 Immelmann: half roll upright; 3 level */
  s->throttle=1;
  if(m->stage==0){*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));*rollIn=rollFor(s,0);if(m->stageTime>2){m->pitched=0;m->stick=0;next(m);}}
  else if(m->stage==1){*rollIn=0;*pitchIn=pitchFor(m,s,dt,5.5f);m->metric[1]=m->stageTime;
   if(m->pitched>=(m->kind==MAN_LOOP?2*PI-12*DEG:PI-8*DEG)||m->stageTime>30){if(m->stageTime>30)m->failed=1;m->stick=0;next(m);if(m->kind==MAN_LOOP)next(m);}}
  else if(m->stage==2){*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,rollNz(s));if(fabsf(s->roll)<8*DEG||m->stageTime>6){if(m->stageTime>6)m->failed=1;next(m);}}
  else{holdSpeed(s,dt,m->startCas);*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,holdPitch(s,2*DEG));m->metric[0]=headingChange(m,s);if(m->stageTime>8)m->done=1;}
  break;
 case MAN_SPLITS:
  /* 0 idle and speedbrake to 300 kt; 1 roll inverted; 2 5.5 g pull through 180 deg; 3 level */
  if(m->stage==0){s->throttle=0;s->speedbrake=1;*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));*rollIn=rollFor(s,0);if((s->ias<300&&m->stageTime>2)||m->stageTime>25)next(m);}
  else if(m->stage==1){s->throttle=0;*rollIn=rollFor(s,PI);*pitchIn=pitchFor(m,s,dt,rollNz(s));if(fabsf(s->roll)>172*DEG||m->stageTime>5){m->pitched=0;m->stick=0;m->metric[2]=s->pos.z;next(m);}}
  else if(m->stage==2){s->throttle=0;*rollIn=0;*pitchIn=pitchFor(m,s,dt,5.5f);m->metric[1]=m->metric[2]-s->pos.z;
   if(m->pitched>=PI-8*DEG||m->stageTime>20){if(m->stageTime>20)m->failed=1;m->stick=0;next(m);}}
  else{s->speedbrake=0;holdSpeed(s,dt,m->startCas);*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,holdPitch(s,2*DEG));m->metric[0]=headingChange(m,s);if(m->stageTime>8)m->done=1;}
  break;
 case MAN_INVERTED:
  /* 0 settle; 1 roll inverted; 2 hold altitude inverted 10 s; 3 roll upright; 4 level */
  if(m->stage==0){*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));*rollIn=rollFor(s,0);if(m->stageTime>2)next(m);}
  else if(m->stage==1){*rollIn=rollFor(s,PI);*pitchIn=pitchFor(m,s,dt,rollNz(s));if(fabsf(s->roll)>175*DEG||m->stageTime>5){m->stick=0;next(m);}}
  else if(m->stage==2){holdSpeed(s,dt,m->startCas);*rollIn=rollFor(s,PI);*pitchIn=pitchFor(m,s,dt,holdInverted(s,m->startAlt));
   if(m->stageTime>2)m->metric[0]=fmaxf(m->metric[0],fabsf(s->pos.z-m->startAlt));
   if(m->stageTime>10){m->stick=0;next(m);}}
  else if(m->stage==3){*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,rollNz(s));if(fabsf(s->roll)<8*DEG||m->stageTime>5){if(m->stageTime>5)m->failed=1;m->stick=0;next(m);}}
  else{holdSpeed(s,dt,m->startCas);*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));if(m->stageTime>6)m->done=1;}
  break;
 case MAN_VERTICAL:
  /* 0 settle; 1 5 g pull to the vertical; 2 up an 85 deg line to 300 kt;
     3 4 g over the top to 20 deg nose down; 4 roll upright; 5 6 g pull-out; 6 level */
  s->throttle=1;
  if(m->stage==0){*pitchIn=pitchFor(m,s,dt,holdAltitude(s,m->startAlt));*rollIn=rollFor(s,0);if(m->stageTime>2){m->pitched=0;m->stick=0;next(m);}}
  else if(m->stage==1){*rollIn=0;*pitchIn=pitchFor(m,s,dt,5);if(m->pitched>=88*DEG||m->stageTime>15){m->stick=0;next(m);}}
  else if(m->stage==2){*rollIn=0;*pitchIn=pitchFor(m,s,dt,holdPitch(s,85*DEG));m->metric[0]=s->ias;m->metric[1]=s->pos.z-m->startAlt;if(s->ias<300||m->stageTime>25){m->stick=0;next(m);}}
  else if(m->stage==3){*rollIn=0;*pitchIn=pitchFor(m,s,dt,4);if(m->pitched>=PI+20*DEG||m->stageTime>40){if(m->stageTime>40)m->failed=1;m->stick=0;next(m);}}
  else if(m->stage==4){*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,rollNz(s));if(fabsf(s->roll)<8*DEG||m->stageTime>5){if(m->stageTime>5)m->failed=1;m->stick=0;next(m);}}
  else if(m->stage==5){*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,6);if(s->pitch>-2*DEG||m->stageTime>15){m->stick=0;next(m);}}
  else{holdSpeed(s,dt,m->startCas);*rollIn=rollFor(s,0);*pitchIn=pitchFor(m,s,dt,holdPitch(s,2*DEG));if(m->stageTime>6)m->done=1;}
  break;
 default:*pitchIn=*rollIn=0;
 }
}
void dynStatsBegin(DynStats *d,const Flight *s){
 memset(d,0,sizeof(*d));d->maxNz=d->minNz=s->loadFactor;d->minCas=d->maxCas=s->ias;
 d->minPitch=d->maxPitch=s->pitch;d->minAlt=d->maxAlt=d->startAlt=s->pos.z;d->minAgl=1e9f;
}
void dynStatsStep(DynStats *d,const Flight *s,float dt,float agl,int protActive){
 d->time+=dt;d->samples++;
 if(s->onGround){if(agl<d->minAgl)d->minAgl=agl;return;} /* ground roll: no flight extremes */
 d->maxNz=fmaxf(d->maxNz,s->loadFactor);d->minNz=fminf(d->minNz,s->loadFactor);
 d->maxAlpha=fmaxf(d->maxAlpha,s->alpha);d->minCas=fminf(d->minCas,s->ias);d->maxCas=fmaxf(d->maxCas,s->ias);
 d->maxBank=fmaxf(d->maxBank,fabsf(s->roll));d->maxPitch=fmaxf(d->maxPitch,s->pitch);d->minPitch=fminf(d->minPitch,s->pitch);
 d->maxRollRate=fmaxf(d->maxRollRate,fabsf(s->p));d->maxPitchRate=fmaxf(d->maxPitchRate,fabsf(s->q));
 d->minAlt=fminf(d->minAlt,s->pos.z);d->maxAlt=fmaxf(d->maxAlt,s->pos.z);d->minAgl=fminf(d->minAgl,agl);
 d->maxBeta=fmaxf(d->maxBeta,fabsf(s->beta));d->maxYawRate=fmaxf(d->maxYawRate,fabsf(s->r));
 if(s->stallWarning>.5f)d->stallTime+=dt;
 if(protActive)d->protTime+=dt;
}
const char *maneuverReport(const Maneuver *m){
 static char b[128];const float *v=m->metric;char bad[40];
 /* a hardware case can end before the manoeuvre does: say where it was */
 if(m->failed)snprintf(bad,sizeof(bad),", DID NOT COMPLETE");else if(!m->done)snprintf(bad,sizeof(bad),", unfinished at stage %d",m->stage);else bad[0]=0;
 switch(m->kind){
 case MAN_DIVE:snprintf(b,sizeof(b),"peak %.0f kt, VMO %.0f (%+.0f), %.0f s to reach it%s",v[0],v[1],v[0]-v[1],v[2],bad);break;
 case MAN_SPIN:snprintf(b,sizeof(b),"%.1f turns, recovery %.1f s%s",v[0],v[1],m->failed?", NOT RECOVERED":bad);break;
 case MAN_SPIRAL:snprintf(b,sizeof(b),"hands off to bank %.0f pitch %.0f%s",v[1],v[2],m->failed?", NOT RECOVERED":bad);break;
 case MAN_MAXTURN:snprintf(b,sizeof(b),"360 deg in %.1f s, %.1f deg/s%s",v[0],v[0]>0?360/v[0]:0,bad);break;
 case MAN_LOOP:case MAN_IMMELMANN:snprintf(b,sizeof(b),"pull %.1f s, heading change %.0f deg%s",v[1],v[0],bad);break;
 case MAN_SPLITS:snprintf(b,sizeof(b),"height used %.0f m, heading change %.0f deg%s",v[1],v[0],bad);break;
 case MAN_INVERTED:snprintf(b,sizeof(b),"inverted 10 s, height within %.0f m%s",v[0],bad);break;
 case MAN_VERTICAL:snprintf(b,sizeof(b),"vertical line to %.0f kt, %.0f m up%s",v[0],v[1],bad);break;
 default:b[0]=0;
 }
 return b;
}
