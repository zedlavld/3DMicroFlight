# Regional imagery and aircraft materials - v10

The aerial footprint grows from 4.096 km square to 16.384 km square, retaining
sharp central KSEZ imagery and using lower-resolution regional tiles. See IMAGERY.md
for installation: the new app expects sedona_0.cft through sedona_7.cft.

## Original aircraft liveries

The fuselage loft now has continuous longitudinal/angular UV coordinates instead
of sampling flat paint swatches. Wing and tail surface coordinates follow chord
and span, with separate upper/lower atlas strips. Root fairings and small fittings
retain stable paint swatches to avoid projecting body graphics onto unrelated parts.

The SR22 has an original blue/white scheme with silver/dark pinstripes. The SF50
has an original charcoal/white scheme and a warm pinstripe. Paint includes restrained
composite-skin shading/weathering, cowl/radome seams, wing access-cover and fuel-cap
motifs, control hinge lines, tip accents and small fictional registrations. These
markings are illustrative; their placements are not maintenance-document validated.
They are not copied Cirrus factory liveries or X-Plane textures. Composite aircraft
skins intentionally do not receive an invented grid of exposed metal rivets.

Each airframe uses a 512-square RGB565 atlas with seven precomputed mip levels,
699,008 bytes per aircraft (about 1.33 MiB combined). Material swatches include
paint, rubber, metal, navigation lights and a glass gradient with baked highlights.
This is a diffuse/baked material treatment, without real-time environment reflections
or physically based shading. UV seams run along an existing fuselage seam, with
white root fairings retained. Geometry, controls and physics are unchanged.

Use tools/paint_atlas.py to regenerate the original artwork and mip chains;
--pack-only preserves edited PNGs with the established UV layout. Edit those maps
with care: body and wing regions are separate from the material swatches.
tools/check_aircraft_textures.py verifies every mip pixel against the PNG and
records hashes. tools/preview_aircraft.py renders the ARM mesh with the atlas;
these are desktop previews rather than 3DS captures.

## Verification and cost

The app and extended shader compile with devkitPro. ARM geometry, imagery mapping,
partition, capacity and flight checks pass. Packed ground and aircraft textures
pass pixel/layout/hash validation. Compared with v09, the new regional and aircraft
textures add about 3.75 MiB of resident texture data. Additional material ranges and
coarse-image sampling add work. No device frame-rate improvement or minimum is claimed.
