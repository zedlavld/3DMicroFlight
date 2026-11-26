#ifndef AUTOTEST_H
#define AUTOTEST_H
/* Scripted, input-free test runs (v31.4). If sdmc:/3ds/3DMicroFlight/autotest.txt
   exists at startup, the app flies each case, saves top-screen BMPs and a frame
   profile to sdmc:/3ds/3DMicroFlight/autotest/, writes done.txt and exits.
   Works the same in Azahar (run in the background, no focus needed) and on
   New 3DS hardware. One case per line, '#' starts a comment:
     name aircraft airport stage view render warmup_s interval_s shots
   e.g.  kgcn_climb_ext SF50 KGCN CLIMB ext 0 12 0.5 6
   aircraft SR22|SF50; airport = a start ICAO (KPHX...); stage = TAKEOFF,
   CLIMB, CRUISE...; view ext|cockpit; render = render-test mode number.
   v31.8: an optional 10th column names a destination ICAO; the case is then
   flown by the test autopilot (include/autopilot.h) to that airport and the
   shots sample the whole route, e.g.
         tus_phx SF50 KTUS TAKEOFF ext 0 5 30 60 KPHX
   v31.15: view may carry a weather suffix, ext:cloud or cockpit:storm.
   v31.22: ext:varied (seed 1) or ext:varied42 for VARIED weather.
   v31.19: and a look direction in degrees, cockpit@90/-20 (yaw right,
   pitch up), before any weather suffix: cockpit@45/0:storm.
   v34: a local time after the view, ext+21.5 or cockpit@45/0+6.3:cloud
   (hours, 0-24); without one the case flies at 12:00 (full daylight).
   Case time counts flight time only: screenshot saves, the scenery loading
   hold and clock jumps (more than 1 s in one frame) are not counted. */
#define AUTOTEST_PATH "sdmc:/3ds/3DMicroFlight/autotest.txt"
#define AUTOTEST_DIR "sdmc:/3ds/3DMicroFlight/autotest"
#define AUTOTEST_GO_PATH "sdmc:/3ds/3DMicroFlight/autotest.go" /* v35.15: start the autotest without the menu (Azahar runner) */
#define AUTOTEST_MAX_CASES 450 /* v34.02: + day/night and night-lighting cases; v1.1p12: the 1.1 release run (~430) */
/* v31.17: destination HOLD engages the player autopilot's HOLD mode, turned
   30 degrees right and 500 ft up from the start. */
#define AUTOTEST_PLAYER_HOLD (-2)
/* v32.03: PANEL = HOLD with the autopilot panel open (its picture is the
   lower-screen capture); CLIMB = AP + A/THR, HDG +90, VS +1500 ft/min to
   ALT +3000 ft, SPD 20 kt below the start. */
#define AUTOTEST_PLAYER_PANEL (-3)
#define AUTOTEST_PLAYER_CLIMB (-4)
/* v33.09: a manoeuvre name (STALL, TURN, PULL, ROLL, CONFIG, GOAROUND; see
   include/maneuver.h) as destination flies that scripted manoeuvre; the
   destination holds AUTOTEST_MANEUVER_BASE - kind. Every case also writes
   name_dyn.csv (10 Hz flight-dynamics trace) and a dyn line in done.txt. */
#define AUTOTEST_MANEUVER_BASE (-10)
/* v35: AT<lat>,<lon> as destination (e.g. AT38.70,-121.59) starts the case
   over that point instead of the airport: the phase's height above the
   ground and the runway heading are kept. For checking new scenery. */
#define AUTOTEST_AT_POSITION (-5)
/* v1.1p15: route options after the destination ICAO, separated by '/': FLnnn cruise flight
   level, MAX cruise at the maximum allowed speed, LAND land and stop instead of orbiting
   (autopilotRoute), e.g. KTUS/FL350/MAX/LAND. Up to 120 shots per case. */
#define AUTOTEST_MAX_SHOTS 120
#define AUTOTEST_BUILD "v1.1"
/* v1.1.1: the Makefile sets this to 0 for a release tag (no "p" test number): the game then never
   loads autotest.txt, shows no "Run autotest" row and ignores autotest.go. */
#ifndef AUTOTEST_ENABLED
#define AUTOTEST_ENABLED 1
#endif
typedef struct {char name[32];int aircraft,airport,phase,exterior,renderTest,shots,destination,weather;float warmup,interval,lookYaw,lookPitch,hours;
 float atEast,atNorth;
 float cruiseFt;int routeMax,routeLand;} AutotestCase;
/* Parses the script; returns the number of valid cases (0 when absent).
   Malformed lines are skipped and counted in *rejected when non-NULL. */
int autotestLoad(const char *path,AutotestCase *cases,int max,int *rejected);
#endif
