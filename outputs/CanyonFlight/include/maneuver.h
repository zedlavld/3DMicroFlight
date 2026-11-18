#ifndef MANEUVER_H
#define MANEUVER_H
/* v33.09 scripted dynamics manoeuvres for the autotest (include/autotest.h).
   A case whose 10th column names a manoeuvre is flown by a scripted pilot
   that moves the stick, throttle, flaps and gear through the same inputs a
   player would, so the control laws (direct, Airbus normal law, F-16 FLCS)
   are part of the test:
     STALL    idle power, hold altitude until the wing stalls or the law's
              alpha protection stops it, then recover with full power.
     TURN     45 deg steep turn (60 for the F-16), altitude and speed held,
              360 deg of heading, roll out.
     PULL     full aft stick for 4 s, neutral, full forward stick for 1 s.
     ROLL     full aileron for 4 s, neutral 4 s (bank-hold behaviour), roll
              level, full opposite aileron for 2 s, level.
     CONFIG   gear down, flaps one step, flaps full, flaps up, gear up, with
              altitude and speed held (trim change check).
     GOAROUND full power and 12 deg pitch from short final; flaps one step
              up and gear up once climbing, flaps up at 15 s.
   v34.06 extreme-envelope manoeuvres (all aircraft):
     DIVE     full power, nose down (30 deg F-16, 25 SR22, 20 SF50, 12
              airliners) past VMO + 6%, then idle, speedbrake and a pull
              at 80% of the limit load back to level. Starts 2,000 m higher.
              The F-16 dives to its 700 kt limit, not past it.
     SPIN     slow to 1.3 VS at idle, stall, then full aft stick and full rudder for 6 s,
              recovery by PARE (idle, ailerons neutral, opposite rudder,
              stick forward) and a pull-out. Starts 2,500 m higher.
     SPIRAL   full aileron to 120 deg of bank (the A321 law stops it at
              67), hands off 4 s, then recover: idle, wings level, pull.
              Starts 1,500 m higher.
     MAXTURN  full power, 360 deg at the limit load (9 g F-16, 3.8 SR22
              and SF50, 2.5 airliners), bank set to hold altitude.
   F-16 aerobatics (the other four are not aerobatic aircraft):
     LOOP     full power, 5.5 g pull through 360 deg of pitch.
     IMMELMANN half loop, half roll to upright at the top.
     SPLITS   slow to 300 kt, roll inverted, 5.5 g pull through the half
              loop to level. Starts 2,500 m higher.
     INVERTED roll inverted, hold altitude upside down for 10 s, roll back.
     VERTICAL pull to the vertical, hold 85 deg up to 300 kt, pull over the
              top, roll upright, pull out.
   No 3DS dependencies, so it builds on the host and in the unicorn tests. */
#include "flight.h"
enum {MAN_NONE,MAN_STALL,MAN_TURN,MAN_PULL,MAN_ROLL,MAN_CONFIG,MAN_GOAROUND,
 MAN_DIVE,MAN_SPIN,MAN_SPIRAL,MAN_MAXTURN,MAN_LOOP,MAN_IMMELMANN,MAN_SPLITS,MAN_INVERTED,MAN_VERTICAL,MAN_COUNT};
typedef struct {
 int kind,stage,noGuard;
 float t,stageTime,stick,bankCmd,turned,lastYaw,nzFiltered,rolled;
 float startAlt,startCas,startYaw;
 float pitched,metric[3];int done,failed;   /* v34.06: pitch angle flown (integrated q); per-manoeuvre results */
} Maneuver;
/* Name to MAN_* (0 when not a manoeuvre), and back. */
int maneuverIndex(const char *name);
const char *maneuverName(int kind);
/* Some envelope manoeuvres raise the aircraft first (see above), so s is written. */
void maneuverBegin(Maneuver *m,int kind,Flight *s);
/* One line of manoeuvre-specific results ("" when there are none). */
const char *maneuverReport(const Maneuver *m);
/* Called every physics step before stepFlight: writes the stick/rudder
   inputs and may change s->throttle, s->flap and s->gear. */
void maneuverStep(Maneuver *m,Flight *s,float dt,float *pitchIn,float *rollIn,float *rudderIn);
/* Per-case flight-dynamics extremes, kept for every autotest case. */
typedef struct {
 float time,maxNz,minNz,maxAlpha,minCas,maxCas,maxBank,maxPitch,minPitch,maxRollRate,maxPitchRate;
 float minAlt,maxAlt,startAlt,minAgl,stallTime,protTime,maxBeta,maxYawRate;
 int samples;
} DynStats;
void dynStatsBegin(DynStats *d,const Flight *s);
/* protActive: the control law reports a protection or limit this step. */
void dynStatsStep(DynStats *d,const Flight *s,float dt,float agl,int protActive);
#endif
