# CirrusFlight v31: all of Arizona

v31 extends the scenery from the Sedona–Flagstaff–Grand Canyon region to the
whole state. Work is landing in phases; this file records each one.

## Phase 1: statewide terrain (100 m streamed, 500 m embedded)

**Coverage.** A 536 x 666 km box in the existing local equirectangular frame
centred on Sedona (east −281.6 to +256 km, north −409.6 to +256 km), which
contains all of Arizona with margin. The frame is unchanged, so airports,
imagery and saves keep their coordinates. Across the state the frame's
east–west scale error reaches about 4%, so runway headings far from Sedona
can be off by up to about 1°. That is acceptable for this simulator.

**Sources.** USGS 3DEP bare-earth DEM, exported 2026-11-04 as 12 raw F32
GeoTIFF chunks at 100 m game-grid spacing (`tools/build_arizona_elevation.py`,
requests and checksums in `data/scenery_metadata.json`). 3DEP has no coverage
south of the border, and the export returns noise there (zeros, denormals,
NaN and isolated spikes up to 4.6 km). The builder rejects those cells and fills them
with a smooth multi-scale average, so northern Sonora is placeholder terrain,
not measured. Cells below −100 m (bathymetry near the Colorado delta) are
treated the same way. No cells inside the other 11 chunks were rejected.

**Layers, coarse to fine** (`rawElevation()`):

| Layer | Spacing | Where it lives | Size |
|---|---|---|---|
| Statewide base | 500 m | embedded (`terrain_data.c`, grid 0) | 2.87 MB |
| Statewide stream | 100 m | SD: `/3ds/CanyonFlight/scenery/elevation/e_<i>_<j>.bin` | 546 tiles, 72.1 MB |
| Sedona / Flagstaff / Canyon | 30 / 60 / 100 m | embedded, unchanged, blended on top | 4.1 MB |

The 500 m base is the 100 m data low-passed with a [1,4,6,4,1] binomial
filter and decimated, replacing the old 180 x 230 km regional grid. It is
always available, so the game still runs (with smoother terrain) without the
SD tiles. Each tile is 25.6 km square, 257 x 257 int16 samples sharing edges
with its neighbours, plus a 16-byte header (`AZEL`, version, indices, CRC-32).

**Streaming** (`source/terrain_stream.c`). A low-priority worker reads one
tile at a time into a slot that is not yet visible; the main thread owns the
tile table. 40 slots (5.3 MB) hold tiles within 60 km. Tiles within 45 km
are requested nearest first. Loaded tiles are published only when a scenery
rebuild starts, so all 36 parts of one rebuild see the same terrain. If the
tiles within 12 km of the aircraft are missing (new flight, airport change,
loaded save), they are read synchronously before physics runs, so the
aircraft never flies over the 500 m stand-in. Baked shadows are invalidated
whenever published terrain changes.

**Verification.**
- `tests/terrain_stream_test.c` (host gcc, real tiles): all 546 tiles pass
  header/CRC checks and corruption is detected; worst step across a tile edge
  is 0.002 m; stream vs base mean difference is 1.95 m. Landmarks, against
  published elevations: KPHX 341 (345), KTUS 791 (806), KYUM 57 (65), KPRC
  1518 (1537), KIGM 1035 (1053), KSOW 1952 (1953), Humphreys Peak 3834 (3851),
  Mount Graham 3258 (3267). A Douglas to Utah-border sweep never lost the tile
  under the aircraft; at most 33 tiles were resident. 965 checks, 0 failures.
- ARM terrain stability test: 907,298 checks, 0 failures. ARM airport
  playtest: 60,242 checks, 0 failures. Both run without SD, so they cover the
  embedded-base path.
- Azahar, KGCN CRUISE to the South Rim, exterior view: no seams, holes or
  popping. ZL+X profile 25.0 FPS (p95 80 ms, CPU 31.4 ms) against 18.5 FPS
  for v30 as committed on the same leg (mostly the shadow cache).
  `validation/azahar_v31/`.
- `validation/arizona500_hillshade.png`: the base grid with the state outline.

**Installation.** Copy `sdmc/` to the SD card root as before; the new
`scenery/elevation/` folder adds 72 MB. Rebuild the tiles with
`python -P outputs/CanyonFlight/tools/build_arizona_elevation.py` from the
repository root (needs numpy and Pillow; downloads about 200 MB into `work/`).

Not yet verified on New 3DS hardware: SD read time of the 12 km blocking load
(9 tiles, about 1.2 MB) at flight start.

## Phase 2: 13 airports, 25 runways

`tools/build_airports.py` generates `source/airports.c` and `data/airports_v31.json`
from OurAirports `airports.csv`/`runways.csv` (public domain, downloaded
2026-11-04 into `work/ourairports`). There is one record per open runway; closed
runways are skipped.

| Start | Airport | Start runway | Other runways |
|---|---|---|---|
| KSEZ | Sedona | 03 | |
| KFLG | Flagstaff | 03 | |
| KGCN | Grand Canyon | 03 | |
| KPHX | Phoenix Sky Harbor | 08 | 07L, 07R |
| KDVT | Phoenix Deer Valley | 07R | 07L |
| KTUS | Tucson | 12 | 04 |
| KPRC | Prescott | 03R | 12, 03L |
| KIGM | Kingman | 03 | 17 |
| KHII | Lake Havasu City | 14 | |
| KNYL | Yuma | 03L | 03R, 08 |
| KPGA | Page | 15 | 07 |
| KSOW | Show Low | 07 | 04 |
| KDUG | Bisbee-Douglas | 17 | 08 |

- KSEZ, KFLG and KGCN keep their validated v25 records bit-for-bit, so existing
  saves and regression results are unchanged.
- New runways are converted into the game frame: heading and length use the
  east–west scale factor cos(lat0)/cos(lat) (up to 2.6% at Yuma), so they line
  up with NAIP imagery exported in the same frame. True headings are kept in
  the JSON.
- Menus, the AIRPORT softkey and saves use the runway-record index; starts are
  the 13 primary runways (`airportStarts`, `nextAirportStart`).
- Painted runway numbers come from the record (all ten digits now exist;
  L/R/C letters are not painted).
- Generic apron, parallel taxiway and buildings are drawn only at primaries,
  on the side whose apron box (along −700..300 m, 20..300 m off the
  centreline) has no other runway: left at most fields, right at KDVT, none at
  KPRC and KPGA, where both sides are blocked.
- Flattening priority: inside any runway core the lowest-index record wins
  (primaries are listed first). Elsewhere overlapping blend zones mix with
  weights that keep the surface continuous; for one runway this is the old
  formula. Secondary runways that cross their primary are shifted vertically
  to meet it at the intersection (KPRC 12 −7.65 m, KIGM 17 −0.97 m, KPGA 07
  +4.11 m), and secondary pavement is drawn on the flattened surface. Paint,
  pavement and wheel contact therefore agree, but secondaries no longer follow
  a strictly linear profile.
- Runway records are drawn in object part 32 + (index mod 4). The largest
  part, at Phoenix, uses 4,728 of 6,000 vertices.
- Every flattening and paving lookup first rejects runways by bounding radius,
  so 25 records cost the same as three (host benchmark).
- Facilities are generic, not surveyed. Elevations are OurAirports endpoint
  values, which are rounded.

**Verification:** ARM airport playtest 186,082 checks, 0 failures (both aircraft
take off from all 13 start runways, camera clearance, pavement and terrain
agreement); terrain stability 907,298 / 0; menu/save 230 / 0 (cycles through all
starts); phases 185 / 0; elevation stream 965 / 0. Not yet flown in Azahar.

## Phase 4: statewide procedural palette

Beyond the aerial-photo window, the ground is coloured procedurally. That
palette was tuned for the Sedona–Canyon region, so v31 adds illustrative
statewide palettes in `surfaceColor()`. They are driven by elevation, slope
and broad region masks, not land-cover data:

- **Sonoran / Mojave basins** (full below ~700 m, gone by 1,150 m): pale desert
  floor, mottled creosote scrub, grey-brown desert ranges.
- **Central transition zone**: muted tan-olive brush and chaparral replace the
  warm sandstone base, which read as red rock far from Sedona.
- **South-east semi-desert grassland**: golden grass.
- **North-east Colorado Plateau**: buff-to-orange sandstone, with painted
  banding on exposed slopes only.
- **Phoenix and Tucson**: a light built-up tint with ragged noise edges (not
  surveyed boundaries).
- **Strata banding** now applies mainly to exposed rock, which removes
  contour-like stripes on flats.
- **Sedona**: the red-rock fade is wider and ragged so it does not end in a
  visible circle.
- **Canyon strata and Sedona** palettes still override in their areas.

`validation/statewide_colour_map.png` is rendered on the host by
`tests/statewide_colour_map.c` (500 m base, 600 m sampling, no fog).
Terrain stability and airport playtests are unchanged (0 failures).

## Phase 3: statewide aerial photos (NAIP)

`tools/build_corridor.py` (format unchanged, manifest version 15) now builds:

- **Corridor:** the original Sedona–Flagstaff–Grand Canyon tiles (unchanged).
- **Statewide coarse tiles:** 8.192 km square, 512 px, about 16 m per pixel,
  over every tile touching Arizona plus a 12 km buffer.
- **Airport detail tiles:** 2.048 km square, 1024 px, about 2 m per pixel,
  within 3.5 km of every runway at the ten new airports.

| | Tiles | Notes |
|---|---|---|
| Detail (2 m) | 679 | 415 corridor + 264 airports |
| Coarse (16 m) | 4,891 | 123 corridor + statewide |
| Excluded | 88 coarse | no NAIP coverage (Mexico, parts of CA/NV) or outside the service extent |
| Total | 5,570 files | 5,317,619,008 bytes |

Excluded tiles are left out of the embedded catalog
(`data/corridor_index.bin`), so the game never requests them; photo windows
there fall back to the procedural palette. One 2x2 batch was repeatedly
truncated by the service at exactly 1 MiB, so the builder now falls back to
single-tile requests. Source images are cached outside the repository
(`D:\CirrusFlightWork\work\corridor`, about 6 GB) and verified by SHA-256
stamps on reruns.

**Azahar:** at KPHX (TAKEOFF and CRUISE) the photos show the city grid and all
three parallel runways, with procedural desert beyond the photo window. The
ZL+X cruise profile is 25.2 FPS (p95 84 ms, CPU 29.8 ms), the same as the KGCN
cruise, so the larger catalog has no runtime cost.
`validation/azahar_v31/sheet_kphx.png`, `cruise_profile_kphx_photos.csv`.

## Installation (v31)

Copy the contents of `sdmc/` to the SD card root: `/3ds/CanyonFlight/scenery/`
now holds `stream/` (5.32 GB, 5,571 files including the manifest) and
`elevation/` (72 MB, 546 files). Allow about 6 GB free. The app runs without
either folder, using the embedded 500 m terrain and procedural colours.
