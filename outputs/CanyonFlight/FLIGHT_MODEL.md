> Historical baseline report. Current ground handling, engine-out correction,
> phase presets and validation are documented in PHASES_V16.md. The older asset
> counts, airspeed description and collision-only ground behavior below are
> superseded; they describe the earlier development snapshot.

# Flight-model development report — 2026-10-23

This build is an improved engineering baseline. It is not a fully validated
SR22/SF50 simulation. The user confirmed the previous controls build worked on
hardware; this physics/texturing revision still needs a new hardware run.

## Selected baselines and sources

SR22: normally aspirated, 3400-lb G3-era configuration, not SR22T and not the
3600-lb later airframe. Primary reference: Cirrus SR22 POH P/N 13772-002,
original issue 13 May 2008, revisions through May 2011, hosted at:
https://www.cirrus-sr22-net.com/wp-content/uploads/2013/06/Online-SR22-POH-13772-002.pdf

SF50: early 6000-lb configuration. Primary reference: Cirrus SF50 AFM
P/N 31452-001, original issue October 2016, available with later revision pages:
https://flightsimcoach.com/wp-content/uploads/2020/12/SF50-POH.pdf

These manuals are reference sources, not bundled assets. Geometry and stall
anchors were selected from them. Most aerodynamic stability derivatives,
inertias, drag coefficients, propeller efficiency and turbine thrust lapse remain
engineering estimates; they have not been recovered from flight-test data.

## Implemented

- Separate lift, drag, stability and control derivatives for each aircraft.
- Quaternion attitude integration, normalized each step, with Euler angles derived
  only for the display. The previous near-vertical pitch clamp is removed.
- Body-axis rigid-body moments with diagonal inertia and angular cross coupling.
- ISA pressure/density/temperature including the tropopause; optional temperature
  offset at fixed pressure altitude. Default is ISA with no wind.
- Air-relative velocity from ground velocity minus a configurable wind vector.
- Lift-induced drag from wing aspect ratio and estimated span efficiency.
- Flap-dependent CLmax calibrated to idle stall anchors, with post-stall decay.
- Mass and coordinated-bank effects on predicted stall speed.
- Gradual control response and flap/gear deployment.
- Separate normally aspirated propeller and jet thrust models, with density lapse,
  engine response lag, simplified SR22 torque/P-factor and adverse yaw.
- Initial pitch/roll/yaw trim and thrust balance for level airborne starts.
- PFD now uses air-relative equivalent-airspeed estimate, actual flap position,
  load factor, and an aerodynamic stall warning. IAS remains an approximation:
  position error, compressibility and aircraft-specific calibration are not modeled.

## Measured checks

Tests execute the compiled ARM C through Unicorn with VFP enabled. This tests
numerical code, not 3DS services, PICA200 rendering, frame rate or actual hardware.
The raw report is validation/test_report.json. Stall figures below come from
sweeping the implemented lift curve and calculating the corresponding speed;
they are calibration-reproduction checks, not independent dynamic stall tests.

| Aircraft / idle, wings level | Manual target KCAS | Model KCAS-equivalent |
|---|---:|---:|
| SR22, flaps 0%, 3400 lb, forward CG | 70 | 70.008 |
| SR22, flaps 50%, same conditions | 64 | 64.012 |
| SR22, flaps 100%, same conditions | 60 | 60.008 |
| SF50, flaps 0%, 6000 lb | 86 | 86.013 |
| SF50, flaps 50%, same weight | 77 | 77.002 |
| SF50, flaps 100%, same weight | 67 | 67.006 |

SR22 stall source: POH section 5, page 5-12. SF50: AFM page 5-18.
Low-speed EAS is used as an approximation to CAS in this calibration.

| SR22, 3400 lb, ISA, full power | Manual climb FPM | Model energy estimate FPM |
|---|---:|---:|
| Sea level, 101 KIAS | 1398 | 1430 |
| 2000 ft, 100 KIAS | 1279 | 1300 |
| 6000 ft, 98 KIAS | 1041 | 1049 |

Climb source: SR22 POH 5-22. The model estimate is excess propulsive power divided
by weight at prescribed airspeed and near-level lift balance. It is not a
stabilized climb trajectory and does not capture climb angle, mixture, full
propeller-governor operation, or all installation effects.

Two SR22 cruise energy probes use 2900 lb and prescribed delivered shaft-power
fractions: 2000 ft / 72% / 167 KTAS and 6000 ft / 68% / 170 KTAS (POH 5-23).
They show approximately -3.77 and -3.97 kW residual power at target speed.
These are diagnostic mismatches, not exact cruise matches. Full cruise curves,
climb trajectories, fuel consumption and high-altitude SF50 performance remain
unvalidated.

At the default Sedona airborne starts, 60-second hands-off tests measured about
+2.0 metres altitude drift for both aircraft; roll stayed within 0.3 degrees.
Other tests cover atmosphere, axes/orthogonality, wind invariance, control signs,
weight/bank stall scaling, finite integration near vertical, camera tracking,
mesh dimensions, finite mesh vertices and valid UV ranges. Thousands of checks
in the raw count are individual vertex and orbit checks, not distinct flight
performance validation cases.

## Exterior assets

Original meshes now have lofted fuselages, solid tapered wings, tail surfaces,
window/paint atlases, a rotating SR22 propeller, an SF50 nacelle with intake and
exhaust, and moving/hiding landing gear. The gear-down meshes contain 3182 SR22
and 3174 SF50 triangles. They use separate 256x256 RGB565 atlases, 128 KiB each.
The exterior draw path uses a PICA200 vertex shader and perspective-correct
texture sampling. Aircraft and terrain now share GPU depth testing. See SCENERY.md for the
real-elevation world renderer and remaining terrain/airport simplifications. These are recognizable approximations, not
photogrammetric or manufacturer-approved shapes. No X-Plane mesh or texture
has been copied. Inboard flap geometry now deforms with actual flap position; full independent
control-surface articulation is not yet implemented.

art/aircraft_preview.png is a desktop software rendering of the actual
ARM-generated meshes and their atlas PNGs, not a 3DS screenshot. The mesh
previews were inspected before packaging. Runtime GPU rendering remains to be
checked on the handheld. The older simple shapes are retained as fallback if
texture renderer initialization fails.

To edit paint: change art/sr22_paint.png or art/sf50_paint.png, then run:

    python tools/paint_atlas.py --pack-only
    make -j2

Running the paint script without --pack-only regenerates the original designs.
Pillow is required. Generated data/*.bin files are embedded by the Makefile;
the PNGs do not need to be installed on the SD card.

## X-Plane comparison

The requested references are Laminar Research's included SR22 and SF50.
No aircraft files were found in the common local installation paths checked.
They have not yet been inspected or used to set this model's coefficients.

Laminar documents the separation of flight-model definitions and visual objects:
https://developer.x-plane.com/manuals/planemaker/
https://developer.x-plane.com/article/obj8-file-format-specification/

Provide the individual aircraft folders from the installed simulator. The tool
below exports a comparison report of raw ACF properties and a file inventory;
it does not export geometry or texture assets:

    python tools/inspect_xplane.py "D:/X-Plane 12/Aircraft/Laminar Research/Cirrus SR22" --output sr22-reference.json

ACF values, airfoil polars, plugin effects and version-specific fields need
interpretation. They do not recreate Laminar's solver by themselves. Using the
same coefficients in a different solver can produce different flight behavior.
We should match weight, CG, weather, configuration and engine settings before
comparing logged speed, attitude, climb, stall onset and control responses.
Asset reuse requires permission under the relevant license; this build uses
original assets instead.

## Remaining fidelity work

1. Compare the supplied X-Plane definitions and controlled runtime logs.
2. Validate sustained cruise/climb/descent, glide, turns, approach and dynamic
   stalls across weight, CG, altitude and temperature.
3. Obtain better control derivatives, inertia/CG data and engine/propeller maps.
4. Model SR22 slipstream and propeller governor more closely; model SF50 thrust
   installation effects, V-tail coupling, yaw stability and protection systems.
5. Add ground reaction, tire friction, braking, taxi, takeoff and landing handling.
6. Add fuel/weight/CG evolution and systems. No wind/weight setup UI exists yet.
7. Refine the new real-elevation scenery and sourced runway data; see SCENERY.md.
8. Profile the new depth-tested world renderer and refine exterior shape/paint mapping.

## Reproduce numerical tests

Install Python unicorn and pyelftools; Pillow/numpy are needed only for previews.
Build from this folder with devkitARM (PowerShell example):

    & C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe -O2 -marm '-march=armv6k' '-mfloat-abi=hard' '-mfpu=vfp' '-specs=nosys.specs' -Iinclude -Ibuild source/flight.c source/camera.c source/aircraft_mesh.c source/terrain.c source/terrain_data.c source/airports.c source/scenery.c source/imagery.c build/sr22_imported_mesh.bin.o build/sf50_imported_mesh.bin.o tests/physics_test.c '-Wl,-e,main' -lm -o physics_test.elf
    python tools/run_arm_tests.py physics_test.elf validation
    python tools/preview_aircraft.py

The test runner does not boot or emulate the 3DS application. Current devkitARM
build and ARM numerical tests pass. The shader is compiled, but no claim is made
that this revision's GPU output or performance has been verified on a 3DS.

V11 adds in-place switching and adapted exterior models; build with make before
linking the mesh objects for the bare ARM tests. See AIRCRAFT_V11.md.
