#ifndef AUTOPILOT_H
#define AUTOPILOT_H
/* Test autopilot (v31.8) for scripted route flights: takeoff, climb, cruise
   to another airport, descend and orbit it at 600 m above the field. Not a
   modelled avionics autopilot; it only drives the stick, throttle, gear and
   flaps so hardware autotests can sample a whole cross-country flight.
   No 3DS dependencies, so the same code flies the route in host tests. */
#include "flight.h"
enum {AP_TAKEOFF,AP_CLIMB,AP_CRUISE,AP_DESCENT,AP_ORBIT,AP_STAGE_COUNT};
typedef struct {
 int stage,destination;
 float cruiseMsl,arrivalMsl,targetMsl,pitchBias,throttleBias,time;
 float holdHeading,holdMsl;   /* v32.03 panel: kept at engagement */
} Autopilot;
void autopilotBegin(Autopilot *ap,const Flight *s,int destinationAirport);
/* Advances the autopilot by dt and returns the stick inputs for stepFlight.
   Sets s->throttle, s->gear and s->flap itself. */
void autopilotStep(Autopilot *ap,Flight *s,float dt,float *elevator,float *aileron,float *rudder);
float autopilotDistance(const Autopilot *ap,const Flight *s);
const char *autopilotStageName(int stage);
/* v31.17 player HOLD mode: wings to a heading, pitch to an altitude (MSL,
   metres), rates limited to about 1600 ft/min. The pilot keeps the
   throttle, gear and flaps; climbs give way to airspeed near the stall. */
void autopilotHoldBegin(Autopilot *ap,const Flight *s);
void autopilotHold(Autopilot *ap,Flight *s,float dt,float heading,float altitudeMsl,float *elevator,float *aileron,float *rudder);
/* v32.03 panel autopilot. ap: master (pitch/roll); athr: autothrottle to
   speed (independent of ap). Lateral: hdg -> heading, else nav -> direct to
   destination, else the heading at engagement. Vertical: vs+alt -> climb or
   descend at |verticalSpeed| and level at altitude; vs -> vertical speed;
   alt -> go to altitude (about 1600 ft/min) and hold; none -> the altitude at
   engagement. speed kt CAS, heading rad true, altitude m MSL, verticalSpeed m/s. */
typedef struct {int ap,athr,hdg,vs,alt,nav,destination;float speed,heading,altitude,verticalSpeed;} ApPanel;
void autopilotPanelBegin(Autopilot *ap,const Flight *s);
void autopilotPanelStep(Autopilot *ap,Flight *s,const ApPanel *p,float dt,float *elevator,float *aileron,float *rudder);
#endif
