# Canyon Flight v15: Arizona corridor streaming

V15 connects the previous isolated imagery packs with an offline scenery corridor
from Sedona through Flagstaff to Grand Canyon National Park Airport and the South
Rim. It does not cover all of Arizona. The aircraft, PFD, controls and flight model
remain those of v14.

## Coverage and installation

The dataset contains 415 detailed tiles (2,048 m square, 1,024-pixel textures)
and 123 outer tiles (8,192 m square, 512-pixel textures). The detail corridor has
a nominal half-width of 4,096 m around the route; the outer corridor has a nominal
half-width of 16,384 m. Whole tiles intersecting these bands are retained, so the
actual boundary follows the tile grid. See art/corridor_overview.png for coverage.
The route reaches the South Rim; it is not a complete Grand Canyon scenery package.

Source sample spacing is approximately 2.004 m per detailed pixel and 16.063 m
per outer pixel after allowing for texture gutters. These numbers describe the
export grid, not a guarantee of the acquisition's native resolution or accuracy.
The existing USGS elevation grids remain: 30 m Sedona, 60 m Flagstaff, 100 m Canyon,
and 500 m regional background, with blended edges and local airport conditioning.
New imagery adds surface detail without inventing more accurate terrain heights.

Copy the CONTENTS of sdmc/ to the SD card root. The executable, SMDH and scenery/
folder all belong in /3ds/CanyonFlight/. The stream dataset alone occupies
1,246,401,600 bytes (about 1.25 GB decimal / 1.16 GiB), in 538 tile files. Allow at
least 1.5 GB free for the complete install. No network connection is used in flight.
The original three regional packs remain as a compatibility fallback. A missing
or incomplete streaming manifest selects that older regional mode at startup.
Keep the v14 archive if you want to restore the entire previous build.

## Streaming and performance budgets

A full-detail active photo window contains four detailed tiles in a 4.096 km square and
four outer tiles in a 16.384 km square. The windows follow independent nested
2.048 km / 8.192 km grids. Fine triangles blend into the correct coarse tile;
coarse triangles blend into the procedural landscape. Coarse tiles load first;
a coarse-only window remains visible when fine tiles are still loading, absent
from the catalog or fail to read. This also covers the default airport approaches.
The complete SD corridor
is available for travel; it is not all rendered at full resolution simultaneously.

A low-priority libctru worker reads and validates one tile at a time into a
2,796,160-byte ordinary-memory staging buffer. It does not allocate GPU textures
or call Citro3D. The render thread owns all GPU resources and uploads at most
65,536 bytes per rendered frame. At 60 FPS the upload component of one fine tile
alone takes about 0.72 seconds; filesystem and allocation time are additional.

The cache has 12 metadata slots and a strict 20 MiB payload budget. A complete
8-tile active window uses 13,980,672 bytes. The cache is shared between both eyes;
stereo does not duplicate these textures. Mesh buffers and aircraft textures are
additional allocations. Current-window requests take priority over an eight-second
velocity look-ahead. Failed reads are cached rather than retried every frame.
The top screen shows cache occupancy and READ / UPLOAD / READY status; TILE ERROR
means at least one file failed validation or could not be read in this session.

Active terrain and its replacement pin their texture slots. A complete 36-part
terrain job swaps its geometry and texture mapping together; partially uploaded
textures are never drawn. Eviction skips both pinned snapshots. Large airport
jumps and rare cache-pressure transitions use procedural terrain while the new
photo window fills. These transitions can briefly reduce scenery detail. Initial
terrain construction and teleport reconstruction still run synchronously, and
GPU allocation can still cost time; this is not a guarantee of stutter-free flight.
Outside the supplied corridor, procedural terrain remains available within the
existing elevation coverage. This is a regional build, not worldwide scenery.

## Data provenance and rebuilding

Imagery is natural-color USGS / USDA NAIP, distributed through The National Map:
https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPImagery/ImageServer

USGS / USDA / The National Map attribution is retained. This build contains no
Google Earth imagery. The service mosaics acquisitions; download date is not the
photograph's capture date. Per-tile requests, SHA256 hashes, source-image hashes,
projection origin and route are recorded in data/corridor_metadata.json.
NAIP acquisition boundaries can retain small tone differences; seam statistics
are recorded rather than claiming every independent export is pixel-identical.
The source is exported on a local equirectangular grid matching the simulator;
it is not a survey-grade geospatial transformation.

Run tools/build_corridor.py from the workspace root to regenerate the dataset
(Python, numpy, Pillow and network access required). It reuses work/corridor/
source PNGs and existing packed tiles. tools/check_corridor.py verifies the files,
all Morton mip pixels, shared source gutters and route coverage, then builds
the overview. Its source-pixel checks require that local download cache, which is
excluded from the distributable to avoid duplicating gigabytes of source imagery.
Mip levels are generated with Lanczos filtering and packed as RGB565 Morton 8x8.
The executable embeds a sorted tile-key catalog, not the full scenery payload.

## Validation limits

The production build compiles with devkitARM / libctru / Citro3D / Citro2D.
The ARM flight/geometry suite includes independent-window partition tests and
fine-to-coarse UV checks at all 25 grid alignments, plus route coverage every
100 m. Existing flight checks remain regression tests, not aircraft certification.

The cache test executes the production cache and uploader on ARM with OS, SD
completion and GPU allocation substitutes. It checks pinned-slot safety, byte
budgets, the full route at 150 m/s, look-ahead, reversal, airport jumps, corrupt
headers, failed fine/coarse file completions, default approach starts and recovery
from allocation failures. This is
an algorithm test; it does not measure real SD latency, GPU timing or frame rate.
See validation/test_report.json, stream_report.json and corridor_report.json.
Reproduce the two ARM suites with tools/validate_arm.py after running make; it
requires devkitARM, Unicorn and pyelftools. Cache-test GPU memory is a host-test
substitute and is excluded from the production executable.

No v15 emulator visual inspection or New 3DS XL benchmark is claimed. Previous
v13 Azahar screenshots/FPS do not validate this revision. Hardware verification
of SD loading, appearance, stereo and frame rate is still required.

