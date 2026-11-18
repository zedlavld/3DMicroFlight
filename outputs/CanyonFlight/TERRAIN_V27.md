# CirrusFlight v27: terrain differentiation

Stronger fixed southwest sunlight shading makes opposing slopes easier to distinguish. Ambient light preserves detail on shaded faces. Steep ground transitions toward exposed rock: warm sandstone at lower elevations and paler rock higher up. These colors are illustrative, not a land-cover survey.

The photographic terrain now mixes approximately 65% aerial imagery with 35% shaded terrain color. Previously the photo interior largely hid the procedural slope lighting. The same blend applies to both detailed and coarse imagery; tile-edge fades remain intact. This is baked slope shading, not cast shadows or a time-of-day simulation.

Performance: surface colors are calculated during incremental scenery generation and cached within each job. No new terrain triangles, texture allocations, SD scenery files, or draw passes are added. A single texture-combiner stage performs the photo/color blend. Existing elevations and flight dynamics remain unchanged.

Validation: SDK build passed without warnings. 905,300 terrain, lighting-range, tile-clipping and seam checks passed, including 23,240 terrain boundary vertices. Equal opposing slopes are checked for directional lighting contrast. Recorder/version checks passed (11 checks). A paused elevated Sedona scene was inspected in Azahar 2126.2 OpenGL at about 59-60 displayed FPS after loading. This is a spot check, not a hardware benchmark or moving-flight regression test. Screenshot: validation/v27_azahar_terrain.png.

All v26 fixes and bolder lower-screen fonts are included. The previously reported SF50 V-tail triangle remains unresolved; v26 close rear and oblique emulator views did not reproduce it.

## Install

This small update requires an existing complete scenery installation. Back up your saves, then merge the contents of sdmc onto the SD-card root, replacing the CirrusFlight executable and SMDH. Keep scenery at /3ds/CanyonFlight/scenery/. Existing three save slots remain compatible; the update contains no saves. No scenery re-download is necessary.
