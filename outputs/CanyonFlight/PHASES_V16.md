# Flight dynamics and phase testing — v16

V16 adds three-wheel ground contact, suspension, dry-tire friction, rolling
resistance and independent main-wheel brakes. Gentle gear-down touchdowns can
continue into a rollout; gear-up, excessive-sink, wing-strike and severe-attitude
contacts still trigger a crash. Contact uses terrain normals and ground-relative
wheel velocities, so wind is not mistaken for movement across the runway.

The nose wheel is treated as castering. There is no invented rudder-linked
steering servo. Wheel positions, spring/damping constants, tire friction and
impact thresholds are estimates. This is an initial force-based ground model,
not a validated landing-gear, anti-skid, brake-thermal or rough-surface simulation.
Ground effect, propeller governor/slipstream detail, fuel burn, CG evolution and
validated high-altitude SF50 engine performance are still outstanding.

An empirical low-power / engine-off SR22 polar correction adds missing
windmilling drag. Engine cutoff removes propulsive thrust and propeller torque;
engine restart is a simulator test command with the existing spool lag, not a
complete starting-system model. The SF50 engine deck remains an estimate.

## Controls and presets

Existing flight, camera, aircraft, airport, weather and PFD controls remain.
The modifiers below are new. The lower PFD stays visible.

| Control | Simulator action |
|---|---|
| ZL + SELECT | Cycle the test start and reset at the selected airport |
| SELECT | Restart the currently selected test start |
| ZL + B, held | Both wheel brakes; suppresses the normal B flap command |
| ZL + L / R, held | Left / right main-wheel brake; L/R still command rudder |
| ZL + A | Toggle engine cutoff / simplified restart |
| ZL + Y | Start recording; press again to pause and save the CSV |
| Y | Normal pause / resume |

The cycle is TAKEOFF, CLIMB, CRUISE, DESCENT, APPROACH, SHORT FINAL, SLOW FLIGHT,
ENGINE-OUT GLIDE. It starts on APPROACH to preserve the previous default. The
upper screen shows TEST and the selected preset, rather than claiming automatic
phase detection. Selecting another airport keeps the selected preset and wind
mode. These are initial conditions; no autopilot is included.

TAKEOFF starts stopped, gear down and half flaps, 80 m beyond the selected runway
threshold. SHORT FINAL starts 1.4 km before the threshold, configured with full
flaps and gear down, near a three-degree path. Its SF50 mass is 5550 lb; other
SF50 presets retain 6000 lb. Airborne starts include initial pitch/roll/yaw trim.
Gear retraction through the A button is inhibited while ground contact is active.
These presets are software-test conveniences, not published aircraft procedures.

## Flight recorder

The recorder samples at 5 Hz into a fixed 4096-sample RAM buffer (about 13.6
minutes). Pausing stops simulation-time advancement and sampling. A full buffer
stops recording and remains available to save. No recorder filesystem writes
occur during normal sampling. Manual stop pauses before writing; application exit
also attempts to save an outstanding recording.

Files go to /3ds/CanyonFlight/logs/flight_<clock>.csv on the SD card. Columns
include model version, simulation time, aircraft, initial test profile, position,
velocity, wind, approximate CAS, attitude, alpha/beta, air load, stall warning,
normalized throttle/engine values, actual flaps/gear, mass, flight controls,
brakes, ground contact, crash and cutoff state. Engine values are not real N1
or delivered shaft-power indications. Temperature is the preset's default ISA;
there is no temperature setup UI. CSV speed is the model's CAS approximation to
IAS; aircraft-specific instrument/position errors remain absent.

A failed save keeps the RAM buffer and displays LOG SAVE FAILED; ZL+Y retries.
The temporary file is renamed only after successful writing. Treat a full buffer
or save warning as a reason to save/retry before leaving the app. Sampling, capacity,
CSV schema/units and open/write/rename error paths are tested on ARM with mocked
filesystem calls. Actual SD-card recording still needs a device/emulator run.

Summarize a copied hardware log with standard Python:

    python tools/analyze_flight_log.py path/to/flight_123.csv

This writes a .summary.json beside the log. It groups contiguous aircraft/start
profiles and summarizes speed, height change, average climb, warnings and contacts.
A preset label does not automatically change when the aircraft later enters a
new phase; interpret the actual trajectory and configuration columns.

## What was tested

`tools/test_phases.py` compiles the production flight and ground C for ARM and
executes it through Unicorn. A scripted test pilot supplies ordinary bounded
controls; it never teleports the aircraft during a trajectory. The suite includes
climb at three altitudes, trimmed cruise, descent, engine-out glide, approach,
go-around, banked turns, dynamic stall/recovery, ground settling, braked power
hold, takeoff, gentle touchdown/braking, differential braking, slope hold,
crosswind hold and rejection of gear-up/hard impacts.

The flat-surface cases isolate wheel physics. The broader ARM suite also checks
all 48 aircraft/airport/preset combinations against the actual terrain, including
settling at all six takeoff starts and the short-final threshold offset. Existing
geometry, scenery, camera, wind and flight regressions remain in that suite.
The scenery-streaming cache tests remain unchanged. Run after a normal make:

    python tools/validate_arm.py
    python tools/test_phases.py
    python tools/test_recorder.py

Python Unicorn and pyelftools plus devkitARM are required. The recorder test's
validation/recorder_sample.csv is explicitly synthetic, not a real aircraft or
device flight. Automated assertions are engineering regressions, not proof of
flight-test fidelity. Simulator frame rate and actual SD behavior are not measured
by Unicorn.

## Reference comparisons and limitations

SR22 reference: Cirrus POH P/N 13772-002, sections 3 and 5:
https://www.cirrus-sr22-net.com/wp-content/uploads/2013/06/Online-SR22-POH-13772-002.pdf

The 3400-lb windmilling reference is about 88 KIAS and 9.6:1. The model's timed
60-second glide at approximately that speed gives 9.73:1. Its 20-second full-power
climbs at initial sea level, 2000 ft and 6000 ft give approximately 1424, 1294 and
1045 FPM against selected POH anchors 1398, 1279 and 1041 FPM. These are selected
calibration anchors already used in development, now exercised as trajectories;
they are not an independent full-envelope validation.

SF50 reference: Cirrus AFM P/N 31452-001, enroute-climb tables 5-31 through 5-33,
landing weight, and ground-handling discussion:
https://flightsimcoach.com/wp-content/uploads/2020/12/SF50-POH.pdf

The SF50 reference probes interpolate ISA points for a clean 6000-lb aircraft
at maximum continuous thrust, anti-ice off. They report the model setting needed
to match each gradient and the separate full-lever result. Required normalized
settings vary roughly 0.835–0.884 over the three points. This demonstrates that
full lever cannot be treated as a validated MCT setting. No automatic MCT/FADEC
deck or full-envelope SF50 calibration is claimed. Its engine-out glide is an
internal consistency scenario, not a manufacturer-validated glide result.

Takeoff test distances are measured until 8 m AGL, not certified ground rolls or
50-ft obstacle distances. Landing test distance includes the initial near-contact
segment and braking; neither is compared with POH/AFM landing-distance tables.
The scripted pilot, surface, altitude, mass and control history matter. Dynamic
stall minimum speed is not the published steady stall speed.

The requested Laminar included SR22/SF50 files were not found in common local
X-Plane paths. No X-Plane coefficients or runtime data were used in this revision.
The inspection tool remains available when those aircraft folders are supplied.
There has been no v16 runtime visual inspection or New 3DS XL performance test.

## Suggested simulator test session

Start in CALM weather with the SR22 at KSEZ, and record one preset at a time.
For ground checks, verify brake hold/release and gentle contact before adding wind.
For airborne checks, hold a steady configuration long enough to separate transient
behavior from sustained speed/climb. Save each recording, then repeat in the SF50.
Use the displayed FPS and paused CPU/GPU timing readouts separately; the CSV is
flight-state telemetry and does not contain rendering performance counters.

Compare runs only at matching mass, configuration, ISA conditions, altitude and
engine settings. Next calibration work should focus on the SF50 installed thrust /
MCT deck, SR22 governor/idle-prop behavior, CG/control derivatives, and matched
real-world takeoff/landing performance rather than fitting to a single trajectory.

## Install

The small v16 update ZIP replaces the application and includes source/test reports;
it reuses the v15 SD scenery. Copy the update's sdmc/ contents over the existing
SD card. The complete v16 Arizona ZIP also contains all scenery for a fresh install.
The v15 archive is preserved as a complete fallback.
