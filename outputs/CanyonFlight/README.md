# CirrusFlight - New 3DS XL, v30

V30 targets the runway flicker filmed on New 3DS XL hardware (z-fighting between pavement and the terrain beneath it) and makes Sedona and the Grand Canyon more dramatic: a lower sun with baked terrain shadows, illustrative Grand Canyon rock strata, Sedona red-rock coloring and lighter distant haze. See RENDERING_V30.md.

V28 adds a sky gradient, mild distance haze, scattered clouds and a simple rainy storm preset with lightning flashes and synthesized thunder. See WEATHER_V28.md for limits and pending runtime validation.

V27 strengthens directional terrain shading and blends slope-based rock colors into aerial imagery. See TERRAIN_V27.md.

V26 stabilizes terrain grids and shared photo edges, separates runway shoulder geometry, and uses a local elevation origin. See RENDERING_V26.md and PLAYTEST_V26.md.

V25 corrects airport terrain discontinuities and elevated pavement, raises the pilot eye, and adds asphalt grain, weathering and surface markings. See AIRPORTS_V25.md and PLAYTEST_V25.md.

V24 closes the cockpit footwell, adds subtle cabin shading, clears interior help overlays, and adds uncapped frame-time profiling. ZL+X saves a performance snapshot and pauses. See RENDERING_V24.md.

V23 adds synthesized aircraft engine audio, wind and a stall-warning tone, plus Audio menus for volume and mute. See AUDIO_V23.md for playback requirements and validation.

V22 caches fixed cockpit geometry and skips unchanged instrument updates. Representative explicit cockpit vertex updates fall by 75-78%; this is not a measured FPS increase. See OPTIMIZATION_V22.md.

V21 refines both interiors from Google Images references, corrects the SF50 display layout, adds SR22 standby gauges/control stack, and introduces a small original matte material. See COCKPIT_V21.md.

V20 adds an original 3D cockpit interior with live panel cues, corrected look-camera axes and near-field stereo. See COCKPIT_V20.md for controls and limits.

V19 adds three independent flight save slots. Save, Load and Continue now offer a slot picker. Existing saves remain in Slot 1. See SAVES_V19.md.

V18 renames the app CirrusFlight and adds an original top-screen splash, flight setup, pause menu, exit confirmation and persistent save/load. See MENUS_V18.md for installation and controls.

V17 corrects elevator travel saturation and expands dynamics tests and flight-log diagnostics. See DYNAMICS_V17.md for results, installation and calibration limits.

V16 adds selectable flight-test starts, three-wheel ground reactions and braking, engine-out SR22 drag, and a bounded RAM flight recorder. See PHASES_V16.md for controls, test results and remaining calibration work. The v15 scenery is unchanged.

V15 adds continuous offline NAIP scenery along the Sedona�Flagstaff�Grand Canyon / South Rim corridor, an asynchronous SD loader, look-ahead requests, a bounded texture cache and incremental GPU uploads. See STREAMING_V15.md for coverage, installation, budgets and validation limits.

V14 adds regional Flagstaff and Grand Canyon photo packs, automatic regional
texture switching, airport detail, selectable wind/gusts, corrected subsonic
airspeed, and a Garmin-inspired lower PFD with tapes and HSI.
See PFD_SCENERY_V14.md for controls, coverage, references and validation limits.

V13 corrects vertically inverted aircraft texture packing, verified on the SR22
and SF50 in Azahar. See EMULATOR_V13.md for the visual-test results and limits.

V12 restores missing Vision Jet belly/wing-root surfaces and reduces scenery
rendering work with conservative stereo-aware culling and material batching.
See AIRFRAME_PERFORMANCE_V12.md for verification and Azahar test instructions.

V11 imports adapted, attributed SR22/SF50 exterior meshes and source liveries.
The lower-screen AIRCRAFT button swaps aircraft in place (X moved to landing gear in v31).
See AIRCRAFT_V11.md for sources, controls, conversion details and limits.

V10 expands Sedona imagery to 16 x 16 km, retains detailed KSEZ textures, and
adds mapped SR22/SF50 liveries and material detail. See DETAIL_V10.md.

V09 introduced the offline NAIP aerial-texture pack around Sedona/KSEZ. Copy the
CONTENTS of sdmc/ to the SD-card root: it includes the app and scenery files.
See IMAGERY.md for installation, coverage and verification limits.

V08 refines the USGS terrain mesh, terrain lighting, airframe contours, wings,
landing gear, and SF50 inlet/exhaust. See DETAIL_V08.md for changes, budgets and
validation. Stereo slider, camera zoom, controls and flight physics are retained.

V07 adds direct USGS 3DEP terrain (Sedona 30 m, Flagstaff 60 m, Canyon 100 m
sample spacing), closed wing/tail root gaps, wing-root fairings, and top-screen
stereo controlled by the hardware depth slider. See STEREO.md and SCENERY.md.
Build and ARM math/mesh/scenery tests pass; hardware testing remains required.

# Current build

Native homebrew for New Nintendo 3DS XL, using devkitARM, libctru, Citro3D and
Citro2D. The app requests New 3DS CPU speedup. See PHASES_V16.md for the current
flight model, testing workflow and measured limits; STREAMING_V15.md describes
the unchanged scenery dataset. Earlier documents are development history.

## Install

Copy the CONTENTS of sdmc/ to the SD-card root, then launch through an existing
Homebrew Launcher. The new app lives under /3ds/CirrusFlight/; keep the installed scenery under
/3ds/CanyonFlight/scenery/ for compatibility. The full scenery install needs about 1.3 GB; allow 1.5 GB free.
The small v24 update ZIP reuses the v15 scenery already on the SD card.

## Controls

- START or lower-screen MENU: open the pause menu for save/load, flight setup and exit.
- Menus: D-pad up/down selects; left/right changes settings; A confirms; B returns. Touch is supported.

- Circle Pad: pitch/roll; pull down to raise the nose.
- L / R: left/right rudder; both together cancel.
- ZR: cockpit/exterior. C-stick: cockpit look/exterior orbit.
- ZL + D-pad Up/Down in exterior view: zoom, 14–160 m.
- Tap ZL without a chord: recenter the view.
- D-pad Up/Down otherwise: throttle; Left/Right: pitch trim.
- X (or A, or tap GEAR on the lower PFD): SF50 gear up/down while airborne; the SR22 has
  fixed gear. GEAR is green when down, white when up and amber in transit.
- B: cycle flap detents. ZL + X: save a frame-time snapshot and pause.
- Lower AIRCRAFT button: swap SR22/SF50, preserving flight position.
- Y: pause/resume. START: open the flight menu.
- SELECT: restart current test. ZL + SELECT: cycle test starts.
- ZL + B: both brakes. ZL + L/R: individual main-wheel brakes plus rudder.
- ZL + A: simulator engine cutoff/restart.
- ZL + Y: start recording; press again to pause/save a CSV.

Lower touchscreen: AIRCRAFT, AIRPORT, WIND, VIEW, MENU, FLAPS. The lower
PFD remains visible in both camera modes. The stereo slider controls depth;
zero uses a single upper-screen view. See STEREO.md for implementation limits.
Flight recordings save under /3ds/CirrusFlight/logs/. Flight state saves use three slots under /3ds/CirrusFlight/. See SAVES_V19.md.

## Flight and scenery

The selectable baselines are a 3400-lb normally aspirated SR22 and an early
6000-lb SF50. The SF50 short-final preset uses 5550 lb. Physics runs at a fixed
120 Hz independently of rendering, with air-relative forces, quaternion attitude,
ISA density/pressure, estimated stability/control derivatives and separate prop /
jet engine models. Wheel contact and braking are initial engineering estimates.
Fuel burn, CG evolution, detailed governor/slipstream, complete aircraft systems
and an installed SF50 MCT/FADEC deck are not implemented or validated.

The offline scenery covers the Sedona–Flagstaff–Grand Canyon/South Rim corridor,
with USGS/USDA NAIP imagery streamed from SD and USGS 3DEP elevations. Generic
airport facilities supplement sourced runway records; buildings are not surveyed
replicas. See art/corridor_overview.png and STREAMING_V15.md. Terrain has limited
regional coverage; this build does not cover all of Arizona.

The Garmin-inspired PFD has attitude, airspeed/altitude tapes, VSI, HSI, slip and
wind cues. Its GPS CDI is a simulator runway-axis aid, not a published instrument
approach. Angles are TRUE, pressure is fixed at 29.92, and IAS approximates CAS.
There is no flight director, autopilot, radio tuning or moving-map database.

## Build and test

Set DEVKITPRO and DEVKITARM to the installed 3DS SDK, put devkitARM/tools/MSYS2
bin directories on PATH, and run make -j2 from this directory. The Makefile
embeds data/*.bin and compiles the shader. Source aircraft attribution is in
third_party/aircraft/ATTRIBUTION.md; terrain/imagery provenance is in data/.

After building, the numerical test scripts are:

    python tools/validate_arm.py
    python tools/test_phases.py
    python tools/test_recorder.py

They require devkitARM, Unicorn and pyelftools. Image-generation/verification
utilities also use Pillow/numpy. ARM numerical execution does not test 3DS OS,
GPU, SD latency or frame rate. The current source compiles and its automated
checks pass; v16 still needs a fresh Azahar or New 3DS XL runtime test. Previous
v13 emulator measurements do not validate this revision.

