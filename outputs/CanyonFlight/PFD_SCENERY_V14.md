# Regional scenery, dynamics and lower-screen PFD — v14

## Install and controls

Copy the CONTENTS of sdmc/ onto the SD-card root. Keep the app and all 24 CFT
files in /3ds/CanyonFlight/. Copying only the executable omits the new scenery.
The same layout belongs inside Azahar's chosen virtual sdmc/ folder.

The bottom row now has six touch softkeys: AIRCRAFT, AIRPORT, WIND, VIEW,
PAUSE/RESUME, FLAPS. L/R rudder, Circle Pad flight controls, ZR view, C-stick
look/orbit, ZL + D-pad zoom, X aircraft swap, Y pause, A gear, B flaps and the
other hardware controls remain. Touch view/pause provide alternatives for
emulator testing. AIRPORT preserves the selected weather preset while restarting
the approach. SELECT restarts the selected approach in calm/default conditions.

## Scenery expansion

Sedona, Flagstaff and the Grand Canyon area each have a separate 16.384 km square
NAIP photo zone: approximately 805 square kilometres across three separate zones.
This is not uninterrupted photographic coverage along the entire route between
them. Real-elevation terrain outside the photo zones retains procedural colors.
Existing USGS 3DEP elevation grids and sample spacing are unchanged.

The Flagstaff zone is centred on KFLG and includes surrounding urban/forest detail.
The Canyon zone spans KGCN/Tusayan toward the South Rim and the first nearby gorge
areas. Its detailed centre lies on the plateau between the airport and rim; the
airport and rim use the coarser outer imagery. This does not cover the full Canyon.
The accompanying art/regional_scenery_overview.png shows all three footprints.

Each pack has a 4.096 km central square sampled at approximately 2 m per pixel and
an outer zone sampled at approximately 16 m per pixel, with mip maps and blended
boundaries. Sampling is not a statement of source survey accuracy. Data and source
catalog responses are recorded in data/naip_*metadata.json and *_catalog.json.
New public imagery was retrieved on 2026-10-24 from USGS/USDA/The National Map:
https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPImagery/ImageServer

Only one eight-tile texture set is resident at a time: approximately 13.33 MiB of
image payload, as before. The three sets total 41,942,784 bytes on disk, including
headers. AIRPORT selects its region immediately; entering another covered zone
also chooses that region with a distance margin to avoid repeated swaps. A change
frees the previous textures and synchronously loads/rebuilds the complete scene.
It can cause a brief pause; background disk streaming is not implemented. Missing
or malformed pack files produce procedural fallback for that region.

KFLG and KGCN now receive generic taxiway/apron/hangar geometry as well as their
runways. Additional box-shaped terminal/tower structures and simple edge fixtures
add silhouettes. These are illustrative placements, not surveyed airport replicas
or verified operational lighting systems, and may not match the aerial photograph.
Terrain conditioning was expanded to accommodate those sample aprons. Source DEM
samples themselves are unchanged. Maximum tested source job: 3,024 vertices,
below its 6,000-vertex cap. Terrain density was not increased.

## Flight and air-data changes

Displayed airspeed now comes from subsonic isentropic pitot impact pressure,
calibrated against ISA sea-level pressure and sound speed. This distinguishes
calibrated airspeed from equivalent airspeed at altitude. No instrument/position
error model exists, so the prototype's IAS display equals this ideal CAS.
The FAA explains the IAS/CAS/EAS/TAS distinction in the Instrument Flying Handbook:
https://www.faa.gov/sites/faa.gov/files/pilots/FAA-H-8083-15B.pdf

WIND selects calm, breeze or gusty presets. The latter two use mean wind FROM
270 degrees TRUE, bounded horizontal/vertical gust harmonics, spatial variation,
and a shear factor rising from 0.55 near terrain to 1.0 by 800 m AGL. Unscaled
mean speeds are 5 and 9 m/s. Air-relative forces respond to the changing airmass;
changing the preset does not teleport the aircraft or alter its ground velocity.
Weather advances with the 120 Hz flight simulation and freezes while paused.
This is deterministic illustrative weather, not measured weather or a calibrated
Dryden/von Karman turbulence model. No new empirical aircraft derivatives were
invented. Existing trim, stall, engine and rigid-body limitations remain.

## PFD

The original artwork now uses moving airspeed/altitude tapes, a bank scale,
attitude ladder, slip/skid cue, vertical-speed pointer, heading-up compass/HSI,
GPS runway course/deviation, true heading/track, TAS, groundspeed and wind.
Numeric IAS/altitude pointers, color bands, a navigation strip and dark touch
softkeys give the display a Garmin-like organization at 320 x 240 pixels.
Reference for instrument placement and presentation:
https://static.garmincdn.com/pumac/G1000:Diamond_CockpitReferenceGuide_DA42version0370.13orlater_.pdf

This is not Garmin software or a complete G1000/Perspective emulation. It has no
radio tuning, autopilot, flight director, approach database or certified alerts.
MANUAL is an honest mode label. The GPS course follows the selected runway axis;
the CDI uses one nautical mile for full-scale deflection. It is a simulator aid,
not a published approach or an ILS receiver. Angles are explicitly TRUE because
magnetic variation is not implemented. Barometric reference is fixed at 29.92 inHg.

SR22 display speed limits and flap references use POH P/N 13772-002 section 2.
SF50 configuration limits and its 250 KIAS / Mach 0.53 cap use AFM 31452-001 section 2.
These display cues do not implement structural damage or aircraft protection logic.
Original manuals, used as references and not bundled:
https://www.cirrus-sr22-net.com/wp-content/uploads/2013/06/Online-SR22-POH-13772-002.pdf
https://flightsimcoach.com/wp-content/uploads/2020/12/SF50-POH.pdf

## Verification and limits

Production compilation passes without warnings. The updated ARM suite covers
calibrated airspeed, wind invariance and bounded gusts, integration, slip cue sign,
HSI geometry, configuration speed limits, region selection, translated clipping
and full terrain jobs at both new photo origins. All 24 tile files pass headers,
lengths, SHA256, every Morton-swizzled mip pixel and shared-gutter comparisons.
See validation/test_report.json and imagery_*_report.json for raw results.
Existing geometry/flight checks are regression tests, not proof of aircraft fidelity.

The PFD preview executes the actual compiled ARM drawing code and renders the
resulting drawing commands with approximate desktop fonts. Three situations were
inspected, including bank/wind and high altitude, both enlarged and at native
320 x 240 size. art/pfd_preview.png is labelled as a drawing-command preview.
Reproduce it with tools/preview_pfd.py (devkitARM, Pillow, Unicorn and pyelftools).

Azahar was started for v14, but Windows capture failed twice with:
IGraphicsCaptureItemInterop.CreateForMonitor: 0x80070057. Runtime visual inspection,
regional texture swapping and touch actions therefore were not verified in the
emulator for v14. The successful v13 screenshots/FPS measurements do not validate
this new revision. New 3DS XL appearance, frame rate, load pauses and stereo still
need a hardware run. Terrain contact still ends flight; ground handling is not added.
