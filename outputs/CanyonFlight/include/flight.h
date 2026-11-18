#ifndef FLIGHT_H
#define FLIGHT_H
#include <math.h>
typedef struct {float x,y,z;} Vec;
/* v33: per-aircraft properties replace the v31/v32 'SR22 or SF50' checks.
   Speeds are knots CAS. */
enum {ENGINE_PISTON,ENGINE_TURBOFAN};
enum {LAW_DIRECT,LAW_AIRBUS,LAW_F16};
enum {AERO_LINEAR,AERO_F16};
typedef struct {float approach,climb,cruise,descent,final,glide,orbit,rotate,liftoff;} AircraftSpeeds;
typedef struct {
 const char *name;
 float mass,area,span,chord,power,ix,iy,iz;
 float length,cl0,cla,cd0,efficiency,cm0,cma,cmq,cme;
 float clb,clp,clr,clda,cnb,cnr,cnda,cndr,cyb,cydr;
 float stallCAS[3],flapLift[3],flapDrag[3];
 int engine,retractableGear,mesh,cockpitStyle;   /* mesh: 0 SR22, 1 SF50, 2 737, 3 F-16, 4 A321; cockpitStyle: 0 SR22, 1 SF50, 2 737, 3 A321, 4 F-16 */
 float spoolUp,spoolDown,gearDrag;                /* engine time constants (s); gear-down CD */
 float noseGearX,mainGearX,gearTrack,gearArmZ,wheelHeight; /* body m; track is half the main-gear spacing */
 float cockpitRight;                             /* right cabin wall, eye-relative m */
 float vmo,mmo,vno,gearLimit,flapLimit[2],flapArc[2]; /* PFD limits: mmo 0 = none; flapLimit {half,full}; arcs 0 = none */
 float resetTAS,finalMass,glideGamma;              /* APPROACH preset m/s TAS; SHORT FINAL mass (0 = same); glide path rad */
 AircraftSpeeds speeds;
 /* v33.01 */
 int controlLaw,yawDamper,flapSteps;               /* flapSteps: detents above zero (B key cycles) */
 const char *flapNames[5];                        /* detent names, NULL = show percent */
 float mdd,bellyClear,tailStrike,takeoffFlap;     /* drag-divergence Mach; CG height that is a crash; ground pitch limit (rad) */
 float spoilerLift,spoilerDrag;                   /* CL lost / CD added at full spoilers (0 = none) */
 float thrustLapse[2];                            /* turbofan thrust x (1 - k1 M + k2 M^2) */
 int aeroModel;                                   /* AERO_F16: table aerodynamics and thrust (f16_aero.c) */
} Aircraft;
#define AIRCRAFT_COUNT 5
/* One record per runway (v31). name/le/he are the menu name and the painted
   runway numbers; primary marks the airport's start runway, which also gets
   the apron and buildings, on apronSide (-1 left of heading, +1 right, 0 none: both sides blocked). */
typedef struct {const char *id; float east,north,elevation,heading,length,width,lowElevation,highElevation;
 const char *name,*le,*he;int primary,apronSide;} Airport;
#define AIRPORT_COUNT 49
#define AIRPORT_START_COUNT 24
enum {PHASE_TAKEOFF,PHASE_CLIMB,PHASE_CRUISE,PHASE_DESCENT,PHASE_APPROACH,PHASE_FINAL,PHASE_STALL,PHASE_GLIDE,PHASE_COUNT};
typedef struct {
 Vec pos,vel,wind;
 float roll,pitch,yaw,p,q,r,throttle,engine,trim,flap;
 int aircraft,airport,gear,crashed;
 float qw,qx,qy,qz,mass,isaOffset,flapActual,gearActual,elevator,aileron,rudder;
 float aileronTrim,rudderTrim;
 float ias,alpha,beta,loadFactor,stallWarning,thrust;
 int weather;
 float weatherTime;
 int engineCutoff,onGround,startPhase;
 float brakeLeft,brakeRight,groundLoad,touchdownSink;
 /* v33.01, appended so v32 saves still load (flight_save.c). */
 float speedbrake,spoilerActual;
} Flight;
typedef struct {float density,pressure,temperature,sound;} Atmosphere;
typedef struct {Vec force,moment;float cl,cd,alpha,beta,ias,thrust,stallRatio;} Aero;
extern const Aircraft aircraft[AIRCRAFT_COUNT];
extern const Airport airports[AIRPORT_COUNT];
extern const int airportStarts[AIRPORT_START_COUNT];
/* Next/previous primary runway record after ap (dir = +1 or -1), wrapping. */
int nextAirportStart(int ap,int dir);
/* Bounding radius of the flattened runway + apron + 100 m blend. */
float airportReach(const Airport *ap);
float clampf(float x,float a,float b);
float terrain(float east,float north);
Atmosphere atmosphere(float altitude,float temperatureOffset);
float calibratedAirspeed(float trueSpeed,Atmosphere atmosphere);
/* weather: 0 clear, 1 clouds, 2 storm, or WEATHER_VARIED+seed (v31.22): each
   start airport has its own level (0 clear .. 2 storm) and wind from the
   seed, blended by distance and drifting over 15-45 minutes. The seed lives
   in the same int, so the save format is unchanged. */
#define WEATHER_VARIED 3
#define WEATHER_SEEDS 4096
void setWeather(Flight *flight,int preset);
Vec sampleWeatherWind(int preset,Vec position,float time);
int weatherVaried(int weather);
/* Local weather level, 0 (clear) .. 1 (scattered) .. 2 (storm). */
float weatherLevelAt(int weather,Vec position,float time);
float weatherLevel(const Flight *s);
void setAttitude(Flight *s,float roll,float pitch,float yaw);
void resetFlight(Flight *s,int type,int airport);
void resetFlightPhase(Flight *s,int type,int airport,int phase);
const char *flightPhaseName(int phase);
void groundReaction(Flight *s,float dt,Vec external,Vec *force,Vec *moment);
void switchAircraft(Flight *s,int type);
void stepFlight(Flight *s,float dt,float elevator,float aileron,float rudder);
Vec bodyWorld(const Flight *s,Vec v);
Aero evaluateAero(const Flight *s);
float stallSpeedCAS(const Flight *s,float bank);
/* Stall angle of attack (rad) for the current flaps. */
float aircraftAlphaMax(const Flight *s);
#endif
