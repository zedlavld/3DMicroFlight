# CirrusFlight v26 rendering corrections

Terrain detail levels now stay on independent fixed world grids. Their inner boundaries follow the adjacent finer grid, avoiding coarse terrain resampling at every 256 m scenery rebuild. Photo-tile clipping uses a canonical edge direction so shared intersections agree exactly.

Runway shoulders are separate side/end strips. The previous full-width shoulder polygon could rise through the low half of a sloping runway and hide the asphalt. World geometry also uses a local elevation origin to reduce GPU coordinate precision loss.

Validation: 897,764 terrain/clipping checks, including 23,240 seam vertices, passed. The old v25 terrain builder failed the regression checks. Airport/takeoff checks: 60,242 passed; both aircraft completed numerical takeoffs at all three airports. These numerical runs are not hardware flight tests.

Azahar 2126.2 OpenGL: reproduced the hidden Sedona runway, then confirmed asphalt and markings visible after the shoulder correction. Stationary SF50 cockpit/exterior scenes generally displayed 59-60 FPS at 100% emulation speed; this is not New 3DS XL performance. Moving terrain and stereoscopic hardware checks remain necessary.

Lower-screen PFD numbers, labels, soft keys and menu text are emboldened by a 0.6-pixel second glyph pass, reusing parsed text and preserving layout. Menus and PFD were visually checked in Azahar.

The reported triangle across the SF50 V-tail gap remains unresolved. No bridging face was found in the imported/ARM-generated mesh, and close rear and oblique emulator views showed an open gap. No valid tail faces were removed.

## Install the small update

Use an existing complete scenery installation. Back up saves, then merge the contents of this ZIP's sdmc folder into the SD-card root, replacing the CirrusFlight executable and SMDH. Keep scenery under /3ds/CanyonFlight/scenery/. No scenery re-download is needed. Existing three save slots remain compatible.
