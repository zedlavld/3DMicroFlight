# Arizona scenery v07 — 2026-10-24

The procedural height function has been replaced with offline, geographically
sampled elevation grids covering Sedona, Flagstaff and the Grand Canyon.

## Coverage and source

A continuous 180 x 230 km regional grid is centred on a local geographic origin
at Sedona (34.848598480225 N, 111.78800201416 W). Coordinates use a spherical
local equirectangular projection, not a survey-grade geodetic frame.

| Grid | Spacing in the packaged grid | Extent |
|---|---:|---:|
| Regional | 500 m | 180 x 230 km |
| Sedona | 30 m | 24 x 24 km |
| Flagstaff / San Francisco Peaks | 60 m | 48 x 48 km |
| Grand Canyon | 100 m | 90 x 65 km |

Elevation comes directly from the USGS National Map 3DEP bare-earth DEM image
service, downloaded 2026-10-24 as raw floating-point GeoTIFF exports (no hillshade).
The multi-resolution service chooses available sources; this is not a claim of
uniform 1 m or 10 m source resolution. At Sedona, the catalog lists 1 arc-second,
1/3 arc-second, and local 1 m products, with NAVD88 vertical datum; the catalog
response is retained in data/usgs_sedona_catalog.json. No datum conversion is
applied, and the service's elevation values are treated as metres above mean sea
level for this prototype. This is not a survey-grade vertical datum pipeline.

https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer

Grids are bilinearly resampled, rounded to whole metres, blended at their edges,
and locally conditioned around runways/aprons. Grid spacing is not measurement
accuracy. Rendering still uses distance-dependent mesh simplification, so the
full elevation-grid detail is not visible at every distance. Colors and plants
are procedural, not orthophotography or measured land cover.

About 4.08 MB of raw grid samples are embedded in the executable through
terrain_data.c. The .i16 files and exact export requests/checksums are retained
in data/. tools/build_usgs_scenery.py rebuilds the current data; build_scenery.py
is the legacy Mapzen pipeline and would replace it with the older source.
No network access or extra scenery installation is needed on the handheld.
USGS source data, modified for this simulator; not a USGS-approved product.

## Airport sample

Sedona (KSEZ) now includes a runway with end-to-end slope, edge/threshold/centreline
markings, runway numbers, a parallel taxiway and connectors, an apron, generic
hangars and small airport buildings. Its runway is 5132 x 100 ft, with the sourced
true alignment. Only the runway definitions are georeferenced from the dataset;
taxiway, apron, building positions, roof shapes and fixtures are approximate
sample assets, not a surveyed reproduction of Sedona Airport.

Flagstaff Pulliam (KFLG) and Grand Canyon National Park (KGCN) have georeferenced
runway strips with sourced dimensions and endpoint elevations; they do not yet
have detailed airport facilities. Airport selection remains the bottom-screen
AIRPORT button. All starts are airborne, roughly 8 km behind the selected runway,
aligned to its true heading. The prototype heading readout remains true heading,
not magnetic heading; runway designators are the published identifiers.

Runway records are from OurAirports, accessed 2026-10-24; data is public domain
and may be incomplete or out of date. Selected original rows are retained in
data/airports.json. They have not been certified or independently surveyed.
https://ourairports.com/data/
https://github.com/davidmegginson/ourairports-data

## Rendering and remaining limits

The world now uses the same depth range as the aircraft. Terrain, airport
structures and the aircraft can occlude each other. V06 uses eight stitched mesh rings with 32 m nearby cells and coarser distant
cells, extending roughly 65 km along each axis. Camera transforms now run on the
GPU, and background scenery rebuilds are split across frames at 256 m targets.
See PERFORMANCE.md for the caching, detail and zoom updates.

Terrain is shaded geometry with elevation/slope-based colours and haze, not
aerial photography or verified vegetation/land-cover textures. Mesh LOD reduces
visible detail relative to the source DEM. Narrow canyon features may be smoothed;
runway/apron conditioning modifies the underlying terrain locally. Raster source
age, interpolation and a simplified coordinate system limit spatial accuracy.
Outside the included regional bounds an on-screen warning appears and edge
heights are clamped; there is no worldwide terrain streaming.

Ground contact still ends flight. Taxi, wheel contact, braking, takeoff and
landing handling remain unfinished. This scenery update does not make the
simulator's flight dynamics fully validated.

## Aircraft refinements in this build

Wingtip red/green position-light colours, an SF50 engine fan detail, and small
SR22 antenna silhouettes were added. These are visual details, not simulated
lighting/electrical systems. Other controls and aerodynamic parameters are retained.

## Verification

The compiled ARM tests exercise coverage, representative peak/canyon elevations,
runway conditioning/slope, finite terrain vertices, world mesh capacity and colour
ranges, plus the existing physics/camera/aircraft geometry regressions. They pass.
The world meshes contain approximately 11.5–11.8 thousand triangles at the three
test positions. Aircraft counts are 3278 SR22 / 3270 SF50 triangles with gear down.

Desktop previews in art/terrain_preview.png and art/aircraft_detail_preview.png
were generated from the actual ARM-generated mesh data and visually inspected.
They are not 3DS screenshots. Compilation/linking pass. The new terrain renderer,
frame rate, GPU appearance and memory behaviour still need a New 3DS XL test.

## Rebuilding and numerical tests

Normal devkitPro make rebuilds from the embedded source with no downloads.
To regenerate current terrain, install Pillow/numpy and run
`python tools/build_usgs_scenery.py` from a directory where a work/ cache can be
created. It downloads four clipped USGS exports into work/usgs. It regenerates
terrain_data.c, scenery_metadata.json and the four .i16 files in this project.
Airport records remain unchanged. Python dependencies are not bundled.

For ARM numerical tests, use the command in FLIGHT_MODEL.md with these additional
source files before the test file:

    source/terrain.c source/terrain_data.c source/airports.c source/scenery.c source/imagery.c

Then run tools/run_arm_tests.py with an output directory of validation, followed
by tools/preview_scenery.py and tools/preview_aircraft.py for desktop previews.

## V08 refinement

See DETAIL_V08.md for the current detail changes, geometry counts, terrain
comparison and verification limits. Earlier counts above describe older builds.

## V09 Sedona aerial imagery

See IMAGERY.md for the new SD-card photo pack and installation. The elevation
data remains embedded; aerial textures require the four external .cft files.

## V10 update

See DETAIL_V10.md and IMAGERY.md for current liveries, expanded photographic
coverage, installation, rendering costs and verification limits.
