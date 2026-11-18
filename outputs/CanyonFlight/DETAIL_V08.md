# Terrain and airframe detail — v08

## Terrain

The USGS 3DEP grids from v07 are retained: Sedona 30 m, Flagstaff 60 m,
Grand Canyon 100 m, regional 500 m sample spacing. This release improves how
the renderer uses those samples; it does not introduce higher-resolution data.

Each terrain streaming job ranks elevation curvature and adds up to 24 interior
samples where either coarse diagonal misses the relief. Refinement now covers
six mesh levels, instead of three levels with the first eight qualifying cells.
Coarse shared edges remain linear, preserving watertight joins. Unrefined cells
choose the diagonal whose center height is closest to the elevation grid.
Corner and center samples are cached during each job to avoid duplicate reads.

Lighting uses 48 m gradient baselines instead of 120 m, showing smaller slopes.
Elevation bands and a high-altitude snow tint add surface variation. These are
illustrative colors, not measured geology, land-cover data, or seasonal snow.

Fixed sample comparisons within 16 km of the three validation scenes, excluding
2 km around airports, show mean absolute mesh approximation error changing:

| Scene | V07 | V08 |
|---|---:|---:|
| Sedona | 17.87 m | 13.84 m |
| Flagstaff | 12.14 m | 9.87 m |
| Grand Canyon | 34.84 m | 29.69 m |

These are errors against the packaged elevation grids, not independent survey
accuracy. Distant cliffs still lose detail with mesh simplification. Reproduce
with tools/check_terrain_detail.py and the v07 ZIP as its optional argument;
full results, including 95th percentile and worst cases, are in validation/.

## Airframes

- Fuselage, wheel-pant and nacelle lofts: 24 radial segments and three axial
  steps per station interval, compared with 20 and two in v07.
- Wing and tail profiles: 18 points with more samples around the leading edge,
  plus smooth vertex shading based on local surface derivatives.
- Control hinge lines extend through the final tip interval.
- Sixteen-sided tires and contrasting twelve-sided wheel hubs.
- SF50 beveled intake lip, recessed inlet throat and exhaust rim.
- SR22 fin edge surfaces connect the two side skins.

Wing-root fairings and embedded tail roots from v07 are retained. Meshes remain
original approximations fitted to the previously referenced three-view drawings;
this refinement is not a new dimensionally validated aircraft reconstruction.
Flight physics and controls are unchanged.

## Budget and verification

Scene vertex counts at the three validation positions: 51,180 / 51,546 / 50,628.
Largest tested streaming job: 1,968 vertices. Meshes with gear down: 5,518 SR22
and 5,198 SF50 triangles. Terrain buffers allow 66,000 vertices; aircraft buffers
allow 8,000 triangles. Fixed buffer allocations grow about 1.9 MB over v07.
Cached GPU geometry, staged scene updates and stereo buffer reuse remain.

ARM numerical/geometry/scenery tests pass, and desktop previews were inspected.
The devkitPro application builds successfully. These checks do not measure actual
New 3DS XL GPU output, stereo comfort, or frame rate. The depth slider at zero
continues to select one rendering pass for the top screen.
