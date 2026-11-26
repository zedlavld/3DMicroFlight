#include "flight.h"
#include "fbw.h"
#include "f16_aero.h"
#include <string.h>
#define PI 3.14159265359f
#define G 9.80665f
#define KNOT .514444444f
/* SR22 G3 normally aspirated / early SF50. Dimensions and idle stall anchors
   are manual-derived; stability derivatives and engine maps remain estimates.
   See FLIGHT_MODEL.md for sources, conditions and outstanding validation. */
const Aircraft aircraft[AIRCRAFT_COUNT]={
 {.name="SR22",.mass=1542.214f,.area=13.46165f,.span=11.684f,.chord=1.152f,
  .power=231167,.ix=2100,.iy=2800,.iz=4000,.length=7.9248f,
  .cl0=.24f,.cla=5.05f,.cd0=.0258f,.efficiency=.82f,
  .cm0=.035f,.cma=-.85f,.cmq=-12,.cme=.95f,
  .clb=-.09f,.clp=-.55f,.clr=.12f,.clda=.068f, /* v34.04: was .15 (175 deg/s full aileron); ~65 deg/s */
  .cnb=.13f,.cnr=-.22f,.cnda=-.025f,.cndr=.085f,.cyb=-.7f,.cydr=-.14f,
  .stallCAS={70,64,60},.flapLift={0,.28f,.50f},.flapDrag={0,.018f,.065f},
  .engine=ENGINE_PISTON,.retractableGear=0,.mesh=0,.cockpitStyle=0,.spoolUp=.45f,.spoolDown=.45f,.gearDrag=0,
  .noseGearX=1.94f,.mainGearX=-.45f,.gearTrack=1.40f,.gearArmZ=1.27f,.wheelHeight=1.22f,.cockpitRight=1.22f,
  .vmo=200,.mmo=0,.vno=177,.gearLimit=0,.flapLimit={119,104},.flapArc={62,104},
  .resetTAS=65,.finalMass=0,.glideGamma=-.104f,
  .speeds={.approach=120,.climb=100,.cruise=125,.descent=115,.final=80,.glide=88,.orbit=100,.rotate=65,.liftoff=70},
  .controlLaw=LAW_DIRECT,.yawDamper=0,.flapSteps=2,.flapNames={0},.mdd=.52f,.bellyClear=.45f,.tailStrike=.32f,.takeoffFlap=.5f,
  .spoilerLift=0,.spoilerDrag=0,.thrustLapse={.32f,0}},
 {.name="SF50",.mass=2721.554f,.area=18.1819f,.span=11.79576f,.chord=1.541f,
  .power=8211.03f,.ix=6500,.iy=8000,.iz=11500,.length=9.35736f,
  .cl0=.22f,.cla=4.9f,.cd0=.024f,.efficiency=.80f,
  .cm0=.025f,.cma=-1.05f,.cmq=-16,.cme=1.10f,
  .clb=-.11f,.clp=-.60f,.clr=.15f,.clda=.046f, /* v34.04: was .12 (204 deg/s); ~60 deg/s */
  .cnb=.15f,.cnr=-.28f,.cnda=-.018f,.cndr=.075f,.cyb=-.8f,.cydr=-.13f,
  .stallCAS={86,77,67},.flapLift={0,.34f,.72f},.flapDrag={0,.022f,.075f},
  .engine=ENGINE_TURBOFAN,.retractableGear=1,.mesh=1,.cockpitStyle=1,.spoolUp=3.2f,.spoolDown=1.8f,.gearDrag=.025f,
  .noseGearX=2.8f,.mainGearX=-.45f,.gearTrack=1.70f,.gearArmZ=1.27f,.wheelHeight=1.22f,.cockpitRight=1.40f,
  .vmo=250,.mmo=.53f,.vno=0,.gearLimit=210,.flapLimit={190,150},.flapArc={0,0},
  .resetTAS=95,.finalMass=2517.4377f,.glideGamma=-.077f,
  .speeds={.approach=160,.climb=140,.cruise=200,.descent=180,.final=90,.glide=120,.orbit=150,.rotate=80,.liftoff=85},
  .controlLaw=LAW_DIRECT,.yawDamper=0,.flapSteps=2,.flapNames={0},.mdd=.52f,.bellyClear=.45f,.tailStrike=.32f,.takeoffFlap=.5f,
  .spoilerLift=0,.spoilerDrag=0,.thrustLapse={.32f,0}},
 /* v33.01 Boeing 737-800 (CFM56-7B26, winglets) at a 65 t operating mass.
    Boeing D6-58325-7 Rev C: MTOW 79,015 kg, MLW 66,360 kg, OEW 41,412 kg.
    Wing 124.6 m2, span 35.8 m with winglets, MAC 3.96 m, 2 x 117 kN.
    Stability derivatives are typical swept-wing transport estimates;
    stall/flap/speed anchors from published 737NG reference speeds. */
 {.name="B737",.mass=65000,.area=124.58f,.span=35.8f,.chord=3.96f,
  .power=234000,.ix=1.2e6f,.iy=3.4e6f,.iz=4.4e6f,.length=39.47f,
  .cl0=.20f,.cla=5.6f,.cd0=.0215f,.efficiency=.78f,
  .cm0=.04f,.cma=-1.30f,.cmq=-25,.cme=1.40f,
  .clb=-.10f,.clp=-.55f,.clr=.12f,.clda=.030f,
  .cnb=.15f,.cnr=-.25f,.cnda=-.003f,.cndr=.075f,.cyb=-.9f,.cydr=-.15f,
  .stallCAS={145,124,115},.flapLift={0,.55f,1.0f},.flapDrag={0,.035f,.10f},
  .engine=ENGINE_TURBOFAN,.retractableGear=1,.mesh=2,.cockpitStyle=2,.spoolUp=6.5f,.spoolDown=4.5f,.gearDrag=.015f,
  .noseGearX=14.0f,.mainGearX=-1.0f,.gearTrack=2.86f,.gearArmZ=2.6f,.wheelHeight=2.55f,.cockpitRight=1.40f,
  .vmo=340,.mmo=.82f,.vno=0,.gearLimit=270,.flapLimit={200,162},.flapArc={0,0},
  .resetTAS=105,.finalMass=60000,.glideGamma=-.058f,
  .speeds={.approach=210,.climb=250,.cruise=280,.descent=260,.final=145,.glide=220,.orbit=210,.rotate=145,.liftoff=155},
  .controlLaw=LAW_DIRECT,.yawDamper=1,.flapSteps=4,.flapNames={"UP","5","15","30","40"},.mdd=.78f,.bellyClear=1.6f,.tailStrike=.19f,.takeoffFlap=.25f,
  .spoilerLift=.40f,.spoilerDrag=.08f,.thrustLapse={.80f,.45f}},
 /* v33.01 Airbus A321-200 (CFM56-5B3, sharklets) at 72 t. Published:
    MTOW 93.5 t, MLW 77.8 t, wing 122.6 m2, span 35.8 m, MAC 4.29 m,
    2 x 147 kN. The Airbus planning document could not be downloaded
    (bot protection); figures are the published ones as known, not
    re-checked against it. Normal-law flight controls (source/fbw.c). */
 {.name="A321",.mass=72000,.area=122.6f,.span=35.8f,.chord=4.29f,
  .power=294000,.ix=1.4e6f,.iy=4.6e6f,.iz=5.8e6f,.length=44.51f,
  .cl0=.22f,.cla=5.7f,.cd0=.022f,.efficiency=.78f,
  .cm0=.04f,.cma=-1.20f,.cmq=-28,.cme=1.35f,
  .clb=-.10f,.clp=-.55f,.clr=.12f,.clda=.035f,
  .cnb=.16f,.cnr=-.26f,.cnda=-.003f,.cndr=.08f,.cyb=-.95f,.cydr=-.16f,
  .stallCAS={154,128,115},.flapLift={0,.60f,1.05f},.flapDrag={0,.035f,.10f},
  .engine=ENGINE_TURBOFAN,.retractableGear=1,.mesh=4,.cockpitStyle=3,.spoolUp=6.5f,.spoolDown=4.5f,.gearDrag=.015f,
  .noseGearX=15.9f,.mainGearX=-1.0f,.gearTrack=3.8f,.gearArmZ=2.7f,.wheelHeight=2.65f,.cockpitRight=1.40f,
  .vmo=350,.mmo=.82f,.vno=0,.gearLimit=280,.flapLimit={215,190},.flapArc={0,0},
  .resetTAS=108,.finalMass=68000,.glideGamma=-.058f,
  .speeds={.approach=210,.climb=250,.cruise=290,.descent=270,.final=140,.glide=220,.orbit=210,.rotate=145,.liftoff=155},
  .controlLaw=LAW_AIRBUS,.yawDamper=1,.flapSteps=4,.flapNames={"0","1","2","3","FULL"},.mdd=.78f,.bellyClear=1.7f,.tailStrike=.17f,.takeoffFlap=.25f,
  .spoilerLift=.40f,.spoilerDrag=.08f,.thrustLapse={.80f,.45f}},
 /* v33.03 F-16 (Stevens & Lewis model of the NASA TP-1538 F-16, 20,500 lb,
    cg 0.35 cbar). Aerodynamics and thrust are tables (f16_aero.c); the
    linear fields below are only for trim estimates and the PFD stall band.
    Flight control: g / AoA-limited pitch, roll-rate command (fbw.c). */
 {.name="F16",.mass=9298.6f,.area=F16_AREA,.span=F16_SPAN,.chord=F16_CHORD,
  .power=128000,.ix=12875,.iy=75674,.iz=85552,.length=15.06f,
  .cl0=.10f,.cla=3.6f,.cd0=.02f,.efficiency=.8f,
  .cm0=0,.cma=-.10f,.cmq=-6,.cme=.45f,
  .clb=-.05f,.clp=-.4f,.clr=.1f,.clda=.05f,
  .cnb=.1f,.cnr=-.4f,.cnda=-.01f,.cndr=.045f,.cyb=-1.15f,.cydr=-.086f,
  .stallCAS={125,125,125},.flapLift={0,0,0},.flapDrag={0,0,0},
  .engine=ENGINE_TURBOFAN,.retractableGear=1,.mesh=3,.cockpitStyle=4,.spoolUp=2.0f,.spoolDown=1.5f,.gearDrag=.02f,
  .noseGearX=3.6f,.mainGearX=-.35f,.gearTrack=1.18f,.gearArmZ=1.6f,.wheelHeight=1.55f,.cockpitRight=.62f,
  .vmo=700,.mmo=0,.vno=0,.gearLimit=300,.flapLimit={700,700},.flapArc={0,0},
  .resetTAS=130,.finalMass=0,.glideGamma=-.12f,
  .speeds={.approach=250,.climb=350,.cruise=420,.descent=350,.final=150,.glide=200,.orbit=300,.rotate=145,.liftoff=160},
  .controlLaw=LAW_F16,.yawDamper=1,.flapSteps=1,.flapNames={"AUTO","DN"},.mdd=.90f,.bellyClear=.9f,.tailStrike=.22f,.takeoffFlap=0,
  .spoilerLift=0,.spoilerDrag=.06f,.thrustLapse={0,0},.aeroModel=AERO_F16}
};
float clampf(float x,float a,float b){return fmaxf(a,fminf(b,x));}
static float sq(float x){return x*x;}

/* Body x forward, y right, z down. Quaternion maps body to NED;
   the public world coordinate frame is ENU. No Euler-angle integration. */
Vec bodyWorld(const Flight *s,Vec v){
 float w=s->qw,x=s->qx,y=s->qy,z=s->qz;
 Vec n={(1-2*(y*y+z*z))*v.x+2*(x*y-w*z)*v.y+2*(x*z+w*y)*v.z,
        2*(x*y+w*z)*v.x+(1-2*(x*x+z*z))*v.y+2*(y*z-w*x)*v.z,
        2*(x*z-w*y)*v.x+2*(y*z+w*x)*v.y+(1-2*(x*x+y*y))*v.z};
 return (Vec){n.y,n.x,-n.z};
}
static Vec worldBody(const Flight *s,Vec v){
 Vec a=bodyWorld(s,(Vec){1,0,0}),b=bodyWorld(s,(Vec){0,1,0}),c=bodyWorld(s,(Vec){0,0,1});
 return (Vec){a.x*v.x+a.y*v.y+a.z*v.z,b.x*v.x+b.y*v.y+b.z*v.z,c.x*v.x+c.y*v.y+c.z*v.z};
}
static void syncEuler(Flight *s){
 float w=s->qw,x=s->qx,y=s->qy,z=s->qz;
 s->roll=atan2f(2*(w*x+y*z),1-2*(x*x+y*y));
 s->pitch=asinf(clampf(2*(w*y-z*x),-1,1));
 s->yaw=fmodf(atan2f(2*(w*z+x*y),1-2*(y*y+z*z))+2*PI,2*PI);
}
void setAttitude(Flight *s,float roll,float pitch,float yaw){
 float cr=cosf(roll/2),sr=sinf(roll/2),cp=cosf(pitch/2),sp=sinf(pitch/2),cy=cosf(yaw/2),sy=sinf(yaw/2);
 s->qw=cr*cp*cy+sr*sp*sy;s->qx=sr*cp*cy-cr*sp*sy;
 s->qy=cr*sp*cy+sr*cp*sy;s->qz=cr*cp*sy-sr*sp*cy;syncEuler(s);
}
Atmosphere atmosphere(float height,float offset){
 /* Geopotential ISA troposphere, isothermal above 11km; temperature deviation
    changes density at a fixed ISA pressure altitude. No weather pressure yet. */
 float h=clampf(height,-500,20000),t=288.15f-.0065f*fminf(h,11000);
 float pressure=101325*powf(t/288.15f,5.25588f);
 if(h>11000)pressure*=expf(-G*(h-11000)/(287.05287f*t));
 t=clampf(t+offset,160,340);
 return (Atmosphere){pressure/(287.05287f*t),pressure,t,sqrtf(1.4f*287.05287f*t)};
}
static float schedule(const float table[3],float flap){
 float x=clampf(flap,0,1)*2;int i=x<1?0:1;return table[i]+(table[i+1]-table[i])*(x-i);
}
float calibratedAirspeed(float speed,Atmosphere atm){
 /* Subsonic isentropic pitot impact pressure, calibrated at ISA sea level.
    Position/instrument errors are not modeled, so displayed IAS equals CAS. */
 float mach=fmaxf(speed,0)/atm.sound;
 float impact=atm.pressure*expm1f(3.5f*log1pf(.2f*mach*mach));
 return 340.294f*sqrtf(fmaxf(5*expm1f((2.f/7)*log1pf(impact/101325)),0))/KNOT;
}
int weatherVaried(int weather){return weather>=WEATHER_VARIED;}
static unsigned wxHash(unsigned a,unsigned b){unsigned h=a*2654435761u^(b+0x9e3779b9u)*2246822519u;h^=h>>15;h*=2246822519u;h^=h>>13;h*=3266489917u;return h^(h>>16);}
static float wxUnit(unsigned h){return (h&0xffffffu)/16777216.f;}
typedef struct {float level,windEast,windNorth;} WxSample;
/* One station per start airport: 40% clear, 35% scattered, 25% storm at
   its base, swinging +-0.45 with a 15-45 minute period; wind from 200-300
   degrees true at 2-10 m/s, up to 6 m/s more in storms. */
static WxSample wxStation(unsigned seed,int k,float time){
 float u0=wxUnit(wxHash(seed,k*4+1)),u1=wxUnit(wxHash(seed,k*4+2)),u2=wxUnit(wxHash(seed,k*4+3)),u3=wxUnit(wxHash(seed,k*4+4));
 float base=u0<.4f?.1f+.3f*u1:u0<.75f?.8f+.4f*u1:1.55f+.45f*u1,period=900+1800*u2;
 float level=clampf(base+.45f*sinf(2*PI*time/period+2*PI*u3),0,2);
 float from=(200+100*u3+25*sinf(2*PI*time/(period*1.7f)+2*PI*u1))*PI/180,speed=2+8*u2+6*clampf(level-1,0,1);
 return (WxSample){level,-sinf(from)*speed,-cosf(from)*speed};
}
/* Inverse-square distance blend, softened at 15 km, so each airport shows
   its own weather and the en-route weather changes smoothly. */
static WxSample wxBlend(unsigned seed,Vec p,float time){
 float sum=0,level=0,east=0,north=0;
 for(int k=0;k<AIRPORT_START_COUNT;k++){const Airport *ap=&airports[airportStarts[k]];
  float dx=ap->east-p.x,dy=ap->north-p.y,w=1/(dx*dx+dy*dy+2.25e8f);WxSample s=wxStation(seed,k,time);
  sum+=w;level+=w*s.level;east+=w*s.windEast;north+=w*s.windNorth;}
 return (WxSample){level/sum,east/sum,north/sum};
}
float weatherLevelAt(int weather,Vec p,float time){return weatherVaried(weather)?wxBlend((unsigned)(weather-WEATHER_VARIED),p,time).level:(float)(weather<0?0:weather>2?2:weather);}
float weatherLevel(const Flight *s){return weatherLevelAt(s->weather,s->pos,s->weatherTime);}
Vec sampleWeatherWind(int preset,Vec position,float time){
 if(preset<=0)return (Vec){0,0,0};
 if(weatherVaried(preset)){
  WxSample w=wxBlend((unsigned)(preset-WEATHER_VARIED),position,time);
  float agl=fmaxf(position.z-terrain(position.x,position.y),0),shear=.55f+.45f*clampf(agl/800,0,1);
  float strength=.4f+2.1f*clampf((w.level-.6f)/1.4f,0,1),phase=position.x*.00031f+position.y*.00017f;
  return (Vec){w.windEast*shear+strength*sinf(time*.67f+phase),w.windNorth*shear+strength*.6f*sinf(time*.43f-phase),strength*.3f*sinf(time*1.13f+phase*.7f)};
 }
 /* Deterministic illustrative weather, not a measured turbulence spectrum.
    Wind FROM 270 true: eastward mean flow, with bounded shear and gusts. */
 float agl=fmaxf(position.z-terrain(position.x,position.y),0);
 float shear=.55f+.45f*clampf(agl/800,0,1),strength=preset==1?.7f:2.5f;
 float phase=position.x*.00031f+position.y*.00017f;
 return (Vec){(preset==1?5.f:9.f)*shear+strength*sinf(time*.67f+phase),
  strength*.6f*sinf(time*.43f-phase),strength*.3f*sinf(time*1.13f+phase*.7f)};
}
void setWeather(Flight *s,int preset){
 s->weather=preset<0?0:preset>2&&preset<WEATHER_VARIED?2:preset>=WEATHER_VARIED+WEATHER_SEEDS?WEATHER_VARIED:preset;s->weatherTime=0;
 s->wind=sampleWeatherWind(s->weather,s->pos,0);
 Aero a=evaluateAero(s);s->ias=a.ias;s->alpha=a.alpha;s->beta=a.beta;
}
float aircraftAlphaMax(const Flight *s){
 const Aircraft *a=&aircraft[s->aircraft];
 float limits[3];for(int i=0;i<3;i++)limits[i]=2*a->mass*G/(1.225f*a->area*sq(a->stallCAS[i]*KNOT));
 float clmax=schedule(limits,s->flapActual),cl0=a->cl0+schedule(a->flapLift,s->flapActual);
 return (clmax-cl0)/a->cla;
}
float stallSpeedCAS(const Flight *s,float bank){
 const Aircraft *a=&aircraft[s->aircraft];
 /* Interpolate CLmax, rather than stall speed, between flap detents. */
 float limits[3];for(int i=0;i<3;i++)limits[i]=2*a->mass*G/(1.225f*a->area*sq(a->stallCAS[i]*KNOT));
 float clmax=schedule(limits,s->flapActual);
 return sqrtf(2*s->mass*G/(1.225f*a->area*clmax*fmaxf(cosf(bank),.05f)))/KNOT;
}
Aero evaluateAero(const Flight *s){
 const Aircraft *a=&aircraft[s->aircraft];Atmosphere atm=atmosphere(s->pos.z,s->isaOffset);
 Vec air={s->vel.x-s->wind.x,s->vel.y-s->wind.y,s->vel.z-s->wind.z};
 Vec v=worldBody(s,air);float speed=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z),safe=fmaxf(speed,1);
 float alpha=atan2f(v.z,v.x),beta=asinf(clampf(v.y/safe,-1,1));
 float pressure=.5f*atm.density*speed*speed;
 if(a->aeroModel==AERO_F16){
  F16Coefficients c=f16Coefficients(alpha,beta,speed,s->p,s->q,s->r,clampf(s->elevator+s->trim,-1,1),s->aileron,s->rudder);
  float mach=speed/atm.sound;
  /* Speedbrake, gear and a wave-drag rise past Mach 0.9 (the tables are subsonic). */
  c.cx-=a->spoilerDrag*s->spoilerActual+a->gearDrag*s->gearActual+.5f*sq(fmaxf(mach-a->mdd,0));
  float thrust=s->engineCutoff?0:f16ThrustNewtons(s->engine,mach,s->pos.z);
  Vec force={thrust+pressure*a->area*c.cx,pressure*a->area*c.cy,pressure*a->area*c.cz};
  Vec moment={pressure*a->area*a->span*c.cl,pressure*a->area*a->chord*c.cm,pressure*a->area*a->span*c.cn};
  float cl=-c.cz*cosf(alpha)+c.cx*sinf(alpha),cd=-c.cx*cosf(alpha)-c.cz*sinf(alpha);
  return (Aero){force,moment,cl,cd,alpha,beta,calibratedAirspeed(speed,atm),thrust,clampf((alpha-.40f)/.08f,0,1)};
 }
 float limits[3];for(int i=0;i<3;i++)limits[i]=2*a->mass*G/(1.225f*a->area*sq(a->stallCAS[i]*KNOT));
 float clmax=schedule(limits,s->flapActual),cl0=a->cl0+schedule(a->flapLift,s->flapActual);
 float alMax=(clmax-cl0)/a->cla,alMin=(-1.1f-cl0)/a->cla;
 float cl=cl0+a->cla*alpha-a->spoilerLift*s->spoilerActual,stall=0;
 if(alpha>alMax){float excess=alpha-alMax;stall=clampf(excess/.5f,0,1);cl=clmax*expf(-2*excess)*(1-stall)+.8f*sinf(2*alpha)*stall;}
 if(alpha<alMin){float excess=alMin-alpha;stall=clampf(excess/.5f,0,1);cl=-1.1f*expf(-2*excess)*(1-stall)+.8f*sinf(2*alpha)*stall;}
 float aspect=a->span*a->span/a->area;
 float cd=a->cd0+cl*cl/(PI*a->efficiency*aspect)+schedule(a->flapDrag,s->flapActual);
 cd+=a->gearDrag*s->gearActual+1.7f*sq(sinf(alpha))*stall+.15f*sq(sinf(beta));
 /* Effective engine-off SR22 polar fitted to the POH windmilling glide
    anchor (~88 KIAS, ~9.6:1 at 3400 lb). It is an empirical polar correction,
    not a blade-element propeller or slipstream simulation. */
 float idleBlend=s->engineCutoff?1:clampf((.16f-s->engine)/.16f,0,1);
 cd+=a->engine!=ENGINE_PISTON?(s->engineCutoff?.004f:0):idleBlend*(.021f+.020f*cl*cl);
 cd+=a->spoilerDrag*s->spoilerActual;
 float mach=speed/atm.sound;cd+=.5f*sq(fmaxf(mach-a->mdd,0));
 float sigma=atm.density/1.225f,thrust;
 if(a->engine==ENGINE_TURBOFAN){
  /* Empirical estimate until an FJ33 installed thrust deck is available. */
  thrust=a->power*(.055f+.945f*s->engine*s->engine)*powf(sigma,.72f)*clampf(1-a->thrustLapse[0]*mach+a->thrustLapse[1]*mach*mach,.5f,1.1f);
 }else{
  float shaft=a->power*s->engine*clampf(1.132f*sigma-.132f,0,1.1f);
  /* Smooth blend of actuator-disc static thrust and propulsive power.
     Low-speed efficiency estimated; constant-speed governor is simplified. */
  float eta=.85f-.18f*expf(-sq(speed/38));
  float staticT=powf(fmaxf(shaft,0),2.f/3)*powf(2*atm.density*3.14f,1.f/3)*.78f;
  float dynamicT=shaft*eta/fmaxf(speed,1);
  thrust=shaft>0?1/sqrtf(1/fmaxf(staticT*staticT,1)+1/fmaxf(dynamicT*dynamicT,1)):0;
 }
 if(s->engineCutoff)thrust=0;
 float lift=pressure*a->area*cl,drag=pressure*a->area*cd;
 float ph=s->p*a->span/(2*safe),qh=s->q*a->chord/(2*safe),rh=s->r*a->span/(2*safe);
 float effectiveness=1-.65f*stall;
 float cy=a->cyb*beta+a->cydr*s->rudder;
 Vec force={thrust-drag*v.x/safe+lift*sinf(alpha),pressure*a->area*cy-drag*v.y/safe,-drag*v.z/safe-lift*cosf(alpha)};
 float lcoef=a->clb*beta+a->clp*ph+a->clr*rh+a->clda*s->aileron*effectiveness;
 /* Trim and stick share the finite elevator travel. Without this limit,
    full stick plus trim could demand more than a full control deflection. */
 float elevator=clampf(s->elevator+s->trim,-1,1);
 float mcoef=a->cm0+a->cma*alpha+a->cmq*qh+a->cme*elevator*effectiveness-.025f*s->flapActual;
 float ncoef=a->cnb*beta+a->cnr*rh+a->cnda*s->aileron+a->cndr*s->rudder*effectiveness;
 /* Modest estimated propeller torque and alpha-sensitive yaw for the SR22. */
 int piston=a->engine==ENGINE_PISTON;
 float torque=!piston||s->engineCutoff?0:-a->power*s->engine*clampf(sigma,0,1.1f)/282.7f;
 Vec moment={pressure*a->area*a->span*lcoef+torque,pressure*a->area*a->chord*mcoef,pressure*a->area*a->span*ncoef-(piston?thrust*.035f*sinf(alpha):0)};
 return (Aero){force,moment,cl,cd,alpha,beta,calibratedAirspeed(speed,atm),thrust,clampf((alpha-(alMax-.04f))/.04f,0,1)};
}
static void integrateAttitude(Flight *s,float dt,float p,float q,float r){
 float rate=sqrtf(p*p+q*q+r*r),angle=rate*dt*.5f,k=rate>1e-6f?sinf(angle)/rate:dt*.5f;
 float dw=cosf(angle),dx=p*k,dy=q*k,dz=r*k,w=s->qw,x=s->qx,y=s->qy,z=s->qz;
 s->qw=w*dw-x*dx-y*dy-z*dz;s->qx=w*dx+x*dw+y*dz-z*dy;
 s->qy=w*dy-x*dz+y*dw+z*dx;s->qz=w*dz+x*dy-y*dx+z*dw;
 float norm=sqrtf(sq(s->qw)+sq(s->qx)+sq(s->qy)+sq(s->qz));
 s->qw/=norm;s->qx/=norm;s->qy/=norm;s->qz/=norm;syncEuler(s);
}
void resetFlight(Flight *s,int type,int ap){
 if(type<0||type>=AIRCRAFT_COUNT)type=0;
 fbwReset();
 memset(s,0,sizeof(*s));s->startPhase=PHASE_APPROACH;s->aircraft=type;s->airport=ap;s->gear=aircraft[type].retractableGear?0:1;s->gearActual=s->gear;
 s->mass=aircraft[type].mass;
 float heading=airports[ap].heading*PI/180;
 s->pos=(Vec){airports[ap].east-sinf(heading)*8000,airports[ap].north-cosf(heading)*8000,airports[ap].elevation+950};
 float speed=aircraft[type].resetTAS;
 float rho=atmosphere(s->pos.z,0).density;
 float cl=2*s->mass*G/(rho*speed*speed*aircraft[type].area);
 float alpha=(cl-aircraft[type].cl0)/aircraft[type].cla;
 float f16Elevator=0;if(aircraft[type].aeroModel==AERO_F16)f16Trim(speed,rho,s->mass,0,&alpha,&f16Elevator);
 setAttitude(s,0,alpha,heading);
 s->vel=(Vec){sinf(s->yaw)*speed,cosf(s->yaw)*speed,0};
 s->trim=clampf(-(aircraft[type].cm0+aircraft[type].cma*alpha)/aircraft[type].cme,-.3f,.3f);
 if(aircraft[type].aeroModel==AERO_F16)s->trim=f16Elevator;
 /* Solve an initial drag/thrust balance so starts are approximately trimmed. */
 float low=0,high=1;
 for(int j=0;j<20;j++){
  s->engine=(low+high)/2;Aero a=evaluateAero(s);
  if(a.force.x*cosf(alpha)+a.force.z*sinf(alpha)>0)high=s->engine;else low=s->engine;
 }
 s->throttle=(low+high)/2;
 if(aircraft[type].engine==ENGINE_PISTON){
  s->engine=s->throttle;Aero a=evaluateAero(s);
  float scale=.5f*rho*speed*speed*aircraft[type].area*aircraft[type].span;
  s->aileronTrim=clampf(-a.moment.x/(scale*aircraft[type].clda),-.1f,.1f);
  s->rudderTrim=clampf((a.thrust*.035f*sinf(alpha)/scale-aircraft[type].cnda*s->aileronTrim)/aircraft[type].cndr,-.1f,.1f);
  s->aileron=s->aileronTrim;s->rudder=s->rudderTrim;
 }

 s->engine=s->throttle;Aero a=evaluateAero(s);s->ias=a.ias;s->alpha=a.alpha;s->loadFactor=1;
}
/* Instant aircraft substitution for the prototype. Preserve the flight path,
   pose, environment and selected airport; use the new airframe's own mass and
   trim. It is intentionally independent of approach restart and camera state. */
void switchAircraft(Flight *s,int type){
 if(type<0||type>=AIRCRAFT_COUNT||type==s->aircraft)return;
 s->aircraft=type;s->mass=aircraft[type].mass;
 if(!aircraft[type].retractableGear){s->gear=1;s->gearActual=1;}
 s->p=s->q=s->r=0;
 s->elevator=s->aileron=s->rudder=0;
 s->aileronTrim=s->rudderTrim=0;s->trim=0;fbwReset();
 if(!aircraft[type].spoilerLift&&!aircraft[type].spoilerDrag)s->speedbrake=s->spoilerActual=0;
 Aero a=evaluateAero(s);
 s->trim=clampf(-(aircraft[type].cm0+aircraft[type].cma*a.alpha)/aircraft[type].cme,-.3f,.3f);
 a=evaluateAero(s);s->ias=a.ias;s->alpha=a.alpha;s->beta=a.beta;s->thrust=a.thrust;
 s->loadFactor=-a.force.z/(s->mass*G);s->stallWarning=a.stallRatio;
}
/* v35.10: positions are float metres from Sedona; near Maine (4,000 km east) a float steps 0.25 m,
   and a 1/120 s step moves a taxiing aircraft only ~4 cm, which would round away (and bias cruise
   ground speed ~6%). Kahan summation carries each step's rounding into the next. The carry is
   always below one float step, so it is kept here rather than in Flight (saved games keep their size). */
static Vec posCarry;
static float compensatedAdd(float sum,float add,float *carry){
 volatile float y=add-*carry,t=sum+y;*carry=(t-sum)-y;return t;
}
int flightLevel=FLIGHT_LEVEL_NORMAL,flightEasyRescues;
void flightEasyRescue(Flight *s){
 const Aircraft *a=&aircraft[s->aircraft];flightEasyRescues++;
 float reach=a->span*.5f+1,ground=terrain(s->pos.x,s->pos.y);
 for(int k=0;k<4;k++)ground=fmaxf(ground,terrain(s->pos.x+(k==0?reach:k==1?-reach:0),s->pos.y+(k==2?reach:k==3?-reach:0)));
 s->p=s->q=s->r=0;
 if(s->ias<stallSpeedCAS(s,0)*1.6f){ /* landing speed: on the wheels */
  s->gear=1;s->gearActual=1;
  setAttitude(s,0,clampf(s->pitch,0,a->tailStrike*.5f),s->yaw);
  s->pos.z=ground+a->gearArmZ-.05f;if(s->vel.z<0)s->vel.z=0;
 }else{                                 /* flying: clear of the ground and climbing */
  setAttitude(s,0,fmaxf(s->pitch,.10f),s->yaw);
  s->pos.z=ground+fmaxf(a->gearArmZ,a->bellyClear)+reach+3;if(s->vel.z<3)s->vel.z=3;
 }
}
int flightForgive(const Flight *s){
 return flightLevel<=0||(flightLevel==1&&s->gearActual>.95f&&s->ias<stallSpeedCAS(s,0)*1.6f&&s->vel.z>-10);
}
void stepFlight(Flight *s,float dt,float elevator,float aileron,float rudder){
 if(s->crashed||dt<=0)return;
 const Aircraft *a=&aircraft[s->aircraft];dt=fminf(dt,1/60.f);
 if(s->weather){s->weatherTime+=dt;s->wind=sampleWeatherWind(s->weather,s->pos,s->weatherTime);}
 fbwControl(s,dt,&elevator,&aileron,&rudder);
 /* Spoilers: the speedbrake lever in flight; on the ground at idle above
    40 kt they deploy fully (ground spoilers). */
 if(a->spoilerLift>0||a->spoilerDrag>0){
  float target=clampf(s->speedbrake,0,1);
  if(a->spoilerLift>0&&s->onGround&&s->throttle<.05f&&s->ias>40)target=1;
  s->spoilerActual+=clampf(target-s->spoilerActual,-dt,dt);
 }else s->spoilerActual=0;
 s->elevator+=(clampf(elevator,-1,1)-s->elevator)*(1-expf(-dt/.12f));
 s->aileron+=(clampf(aileron+s->aileronTrim,-1,1)-s->aileron)*(1-expf(-dt/.10f));
 s->rudder+=(clampf(rudder+s->rudderTrim,-1,1)-s->rudder)*(1-expf(-dt/.18f));
 s->flapActual+=clampf(s->flap-s->flapActual,-dt/5,dt/5);
 s->gearActual+=clampf(s->gear-s->gearActual,-dt/6,dt/6);
 float tau=s->throttle>s->engine?a->spoolUp:a->spoolDown;
 s->engine+=((s->engineCutoff?0:clampf(s->throttle,0,1))-s->engine)*(1-expf(-dt/tau));
 Aero aero=evaluateAero(s);Vec fw=bodyWorld(s,aero.force),groundForce,groundMoment;
 groundReaction(s,dt,fw,&groundForce,&groundMoment);
 fw.x+=groundForce.x;fw.y+=groundForce.y;fw.z+=groundForce.z;
 aero.moment.x+=groundMoment.x;aero.moment.y+=groundMoment.y;aero.moment.z+=groundMoment.z;
 float mass=fmaxf(s->mass,100);
 s->vel.x+=dt*fw.x/mass;s->vel.y+=dt*fw.y/mass;s->vel.z+=dt*(fw.z/mass-G);
 float p=s->p,q=s->q,r=s->r;
 s->p+=dt*(aero.moment.x+(a->iy-a->iz)*q*r)/a->ix;
 s->q+=dt*(aero.moment.y+(a->iz-a->ix)*p*r)/a->iy;
 s->r+=dt*(aero.moment.z+(a->ix-a->iy)*p*q)/a->iz;
 integrateAttitude(s,dt,(p+s->p)/2,(q+s->q)/2,(r+s->r)/2);
 s->pos.x=compensatedAdd(s->pos.x,dt*s->vel.x,&posCarry.x);s->pos.y=compensatedAdd(s->pos.y,dt*s->vel.y,&posCarry.y);
 s->pos.z=compensatedAdd(s->pos.z,dt*s->vel.z,&posCarry.z);
 s->ias=aero.ias;s->alpha=aero.alpha;s->beta=aero.beta;s->thrust=aero.thrust;
 s->loadFactor=-aero.force.z/(mass*G);s->stallWarning=aero.stallRatio;
 /* Ground reactions precede integration; belly penetration is checked again
    afterward so a fast impact cannot tunnel through the contact region. */
 if(s->pos.z<terrain(s->pos.x,s->pos.y)+a->bellyClear){if(flightForgive(s))flightEasyRescue(s);else s->crashed=1;}
}

const char *flightPhaseName(int phase){
 static const char *names[PHASE_COUNT]={"TAKEOFF","CLIMB","CRUISE","DESCENT","APPROACH","SHORT FINAL","SLOW FLIGHT","ENGINE-OUT GLIDE"};
 return names[phase>=0&&phase<PHASE_COUNT?phase:PHASE_APPROACH];
}
/* Test presets are initial conditions, not autopilot or published procedures. */
void resetFlightPhase(Flight *s,int type,int ap,int phase){
 resetFlight(s,type,ap);phase=phase>=0&&phase<PHASE_COUNT?phase:PHASE_APPROACH;s->startPhase=phase;
 if(phase==PHASE_APPROACH)return;
 const Airport *a=&airports[ap];float heading=a->heading*PI/180;
 float distance=phase==PHASE_TAKEOFF?a->length/2-80:phase==PHASE_FINAL?a->length/2+1400:phase==PHASE_CLIMB?1000:5000;
 s->pos.x=a->east-sinf(heading)*distance;s->pos.y=a->north-cosf(heading)*distance;
 if(phase==PHASE_TAKEOFF){
  s->pos.z=terrain(s->pos.x,s->pos.y)+aircraft[type].wheelHeight;s->vel=(Vec){0};s->throttle=s->engine=0;
  s->gear=s->gearActual=1;s->flap=s->flapActual=aircraft[type].takeoffFlap;s->trim=.05f;s->aileronTrim=s->rudderTrim=0;s->aileron=s->rudder=0;
  setAttitude(s,0,0,heading);s->onGround=1;s->ias=0;return;
 }
 if(aircraft[type].finalMass>0&&phase==PHASE_FINAL)s->mass=aircraft[type].finalMass;
 s->flap=s->flapActual=phase==PHASE_FINAL?1:0;s->gear=s->gearActual=aircraft[type].retractableGear?(phase==PHASE_FINAL):1;
 s->pos.z=a->elevation+(phase==PHASE_FINAL?a->lowElevation-a->elevation+74.57f:phase==PHASE_CLIMB?600:phase==PHASE_CRUISE?2400:1500);
 const AircraftSpeeds *v=&aircraft[type].speeds;
 float cas=v->approach;if(phase==PHASE_CLIMB)cas=v->climb;if(phase==PHASE_CRUISE)cas=v->cruise;if(phase==PHASE_FINAL)cas=v->final;
 if(phase==PHASE_STALL)cas=stallSpeedCAS(s,0)*1.13f;
 if(phase==PHASE_GLIDE)cas=v->glide;
 Atmosphere atm=atmosphere(s->pos.z,0);float lo=0,hi=250;for(int i=0;i<24;i++){float mid=(lo+hi)*.5f;if(calibratedAirspeed(mid,atm)<cas)lo=mid;else hi=mid;}float speed=(lo+hi)*.5f;
 float gamma=phase==PHASE_CLIMB?.07f:phase==PHASE_DESCENT?-.06f:phase==PHASE_FINAL?-.05236f:phase==PHASE_GLIDE?aircraft[type].glideGamma:0;
 float cl=2*s->mass*G*cosf(gamma)/(atm.density*speed*speed*aircraft[type].area);
 float alpha=(cl-aircraft[type].cl0-schedule(aircraft[type].flapLift,s->flapActual))/aircraft[type].cla;
 float f16Elevator=0;if(aircraft[type].aeroModel==AERO_F16)f16Trim(speed,atm.density,s->mass,gamma,&alpha,&f16Elevator);
 setAttitude(s,0,alpha+gamma,heading);s->vel=(Vec){sinf(heading)*speed*cosf(gamma),cosf(heading)*speed*cosf(gamma),speed*sinf(gamma)};
 s->aileronTrim=s->rudderTrim=0;s->aileron=s->rudder=0;
 s->trim=clampf(-(aircraft[type].cm0+aircraft[type].cma*alpha-.025f*s->flapActual)/aircraft[type].cme,-.3f,.3f);
 if(aircraft[type].aeroModel==AERO_F16)s->trim=f16Elevator;
 lo=0;hi=1;for(int i=0;i<24;i++){s->engine=(lo+hi)*.5f;Aero probe=evaluateAero(s);Vec world=bodyWorld(s,probe.force);float residual=(world.x*s->vel.x+world.y*s->vel.y+world.z*s->vel.z)/speed-s->mass*G*sinf(gamma);if(residual>0)hi=s->engine;else lo=s->engine;}
 s->engine=s->throttle=(lo+hi)*.5f;
 if(phase==PHASE_CLIMB)s->throttle=s->engine=1;
 if(phase==PHASE_GLIDE){s->engineCutoff=1;s->engine=s->throttle=0;}
 if(aircraft[type].engine==ENGINE_PISTON&&!s->engineCutoff){
  Aero probe=evaluateAero(s);float scale=.5f*atm.density*speed*speed*aircraft[type].area*aircraft[type].span;
  s->aileronTrim=clampf(-probe.moment.x/(scale*aircraft[type].clda),-.1f,.1f);
  s->rudderTrim=clampf(-(probe.moment.z/scale+aircraft[type].cnda*s->aileronTrim)/aircraft[type].cndr,-.1f,.1f);
  s->aileron=s->aileronTrim;s->rudder=s->rudderTrim;
 }
 Aero aero=evaluateAero(s);s->ias=aero.ias;s->alpha=aero.alpha;s->beta=aero.beta;s->loadFactor=1;
}
