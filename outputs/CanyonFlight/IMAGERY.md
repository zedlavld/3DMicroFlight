# Expanded Sedona aerial imagery - v10

## Install

Copy the CONTENTS of sdmc/ to the SD-card root, merging the existing 3ds/ folder.
The app and scenery must be under:

    /3ds/CanyonFlight/CanyonFlight.3dsx
    /3ds/CanyonFlight/CanyonFlight.smdh
    /3ds/CanyonFlight/scenery/sedona_0.cft
    ... through sedona_7.cft

Install all eight tiles with the v10 executable. Restart after copying. The top
screen says SEDONA NAIP | USGS / USDA when loaded, or SEDONA PHOTO PACK NOT LOADED
when the complete pack is absent, invalid or cannot be allocated. That case
uses procedural terrain colors. No runtime network access is required.

## Coverage and resolution

V10 covers 16,384 x 16,384 metres around KSEZ, sixteen times v09's footprint.
The original central 4096-square-metre area retains four 1024-square textures
at about 2.004 m/pixel. Four 512-square regional textures extend the area to
about 8.2 km in each direction, at about 16.063 m/pixel. These deliberately
coarser textures keep texture memory bounded and include the approach start.
Flagstaff and Grand Canyon remain outside the photographic coverage.

A nominal 384 m band blends the inner textures into matching regional imagery;
a 512 m outer band blends into procedural colors. Height and geographic alignment
are preserved by triangle clipping. Central detailed tiles and outer rectangles
form a partition, avoiding a competing overlay surface or uncovered terrain.

Source: public-domain USGS/USDA NAIP natural-color imagery, retrieved 2026-10-24.
Attribution: USGS, USDA, The National Map: Orthoimagery.
https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPImagery/ImageServer

The service catalog and exact export requests are retained in data/naip_catalog.json
and data/naip_metadata.json, together with file hashes, extents and sample spacings.
Pixel spacing in this pack is not a claim of survey accuracy or current conditions.
Source dates vary by footprint; catalog entries provide the available source dates.

Photographed buildings, parked aircraft, vegetation and shadows remain flat ground
features. The generic KSEZ airport structures remain overlaid and can differ from
the photograph. No photogrammetry or surveyed airport reconstruction was added.
The underlying elevation grids are unchanged from v07.

## Rendering, storage and checks

All eight textures stay resident: approximately 13.33 MiB including mipmaps, an
increase of about 2.67 MiB over v09. Scenery files total 13,980,928 bytes with headers.
Detailed files have eight mip levels, regional files seven; filtering is configured
for bilinear/trilinear sampling. The two stereo views reuse these textures and
resident geometry. This is not a texture-paging system.

The renderer supports 90,000 vertices after clipping and 324 material draw ranges.
Expanded blending adds material draws and, in the central area, a second texture
sample. First loading and scene construction may pause. No New 3DS XL FPS, memory
headroom, GPU output or stereo comfort has been verified. Slider zero retains the
single-pass top-screen mode.

Loader validation checks magic, index, dimensions, mip count and exact payload
length. Offline tools also validate SHA-256 hashes. Missing/invalid packs retain
the procedural-color fallback. The app does not modify scenery files on the SD card.

Use tools/build_naip.py with Pillow/numpy to rebuild, caching source imagery under
work/naip. tools/check_naip.py verifies every packed pixel at every mip level,
headers, hashes and matching tile gutters. tools/preview_naip.py makes a desktop
comparison from the ARM-generated clipped mesh and decoded texture data. It is
not a handheld screenshot and samples the base mip level, unlike GPU filtering.

The ARM test suite covers partition area conservation, height preservation, UV
bounds, fade boundaries, inner-to-outer coordinate transforms and staging/scene
capacity. It also retains the flight, camera and airframe tests. It does not boot
or emulate the 3DS GPU or SD filesystem. Reports are bundled under validation/.
