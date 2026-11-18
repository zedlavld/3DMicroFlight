# Performance, scenery detail and exterior zoom — v06

## Exterior camera controls

- Hold ZL + D-pad Up: move the exterior camera closer.
- Hold ZL + D-pad Down: move it farther away.
- Range: 14 to 160 metres; movement scales with frame time.
- Tap and release ZL: recenter the view, retaining the chosen zoom distance.
- Zoom chords do not change throttle. D-pad throttle otherwise works normally.
- C-stick orbit, ZR camera selection, L/R rudder and the persistent PFD remain.

The top header shows exterior camera distance. Zoom also works while paused.
Y still pauses; the paused display now shows CPU/GPU processing times, while
FPS remains on the PFD. These measurements need interpretation on the actual
handheld; desktop ARM emulation is not a hardware performance benchmark.

## Removed recurring CPU work

V05 transformed about 35,000 world vertices on the CPU and rewrote/flushed their
GPU buffer on every frame. V06 keeps world-space vertices resident on the GPU
and sends a camera matrix instead. It no longer rewrites the complete terrain
buffer simply because the camera or aircraft moved.

World rebuilds now use 36 small jobs into an inactive GPU buffer, one per rendered
frame, followed by an atomic swap of the completed scene. Rebuild targets are
spaced 256 metres apart. The previous complete scene stays visible while the
replacement is prepared. Initial loading and large teleports/airport changes
still construct the scene synchronously and may pause briefly.

Aircraft geometry is cached too. Its pose now comes from a GPU model/view matrix.
Only the propeller/fan vertex range changes each frame. Flap and gear geometry
are rebuilt at small deployment increments rather than rebuilding the entire
airframe during every frame of ordinary flight.

The existing 120 Hz fixed-step physics is retained. No frame-rate improvement
or minimum FPS is claimed until this build is measured on New 3DS XL.

## Additional scenery detail

- Smooth per-vertex terrain lighting replaces the coarse face-by-face shading.
- Eight terrain rings, with 32 m nearby cells and progressively coarser distant
  cells, retain approximately the previous maximum viewing extent.
- A bounded number of rough cells gain extra interior elevation samples to better
  represent relief without opening cracks at their coarse neighbouring edges.
- Lightweight procedural trees at higher elevations and shrubs/rocks lower down
  add nearby scenery detail. They are generic decoration, not verified vegetation
  or a geographically surveyed object inventory. Runway/apron corridors are excluded.

The original Mapzen/USGS-derived elevation grids are unchanged. A denser mesh and
smoother shading do not increase source measurement accuracy. There are still no
orthophoto ground textures, real land-cover classification or detailed settlements.

## Accurate elevation sources

For a later source upgrade, USGS 3DEP is the recommended starting point: use the
approximately 10 m regional DEM and evaluate 1 m products where available for
airport surroundings and key cliff/mesa areas. Availability, acquisition dates,
vertical accuracy and datum should be checked per dataset before importing.
Sample spacing alone is not a vertical-accuracy specification.

https://www.usgs.gov/faqs/what-types-elevation-datasets-are-available-what-formats-do-they-come-and-where-can-i-download
https://www.usgs.gov/faqs/why-dont-elevations-your-maps-agree-those-provided-my-gps-system-which-are-correct

This release does not claim that direct USGS 10 m or 1 m data has been imported.
Existing data origins/attribution and the Sedona sample-airport limitations remain
in SCENERY.md. Ground contact still ends flight; landing/taxi physics are unfinished.

## Validation and remaining risks

Compiled ARM tests pass for camera/world and aircraft matrix equivalence,
zoom clamps/recentering, dynamic aircraft ranges, scenery job capacity, terrain,
geometry, and prior flight-model regressions. At the tested positions the largest
scenery job contains 1908 vertices; complete scenes contain about 43–45 thousand
vertices. These are workload counts, not measured milliseconds or FPS. Most raw
assertions are individual geometry/camera checks, not independent fidelity tests.

The cached scene reduces recurring CPU work, but the denser mesh and extra objects
increase GPU workload and buffer memory. GPU behaviour, shader matrix upload,
new input chords, frame pacing and actual memory/performance still need a handheld
run. Scenery swaps can still cause LOD/object popping; there is no temporal geomorph.
Colour and position precision can still differ between the desktop preview and
PICA200. Visual examples in art/ are software renders, not 3DS screenshots.

## V07 stereo and elevation

Direct USGS grids double the local sample density without increasing the mesh
ring dimensions or vertex budget. Tests produce 43,314 / 45,156 / 44,364 scene
vertices, max streaming job 1,908 vertices. Stereo shares resident scene buffers
but draws twice; use the depth slider at zero for single-pass rendering.

## V08 refinement

See DETAIL_V08.md for the current detail changes, geometry counts, terrain
comparison and verification limits. Earlier counts above describe older builds.

## V09 aerial texture pilot

See IMAGERY.md for resident texture memory, mipmaps, clipping, rendering
buffers and hardware verification limits. No frame-rate claim is made.

## V10 update

See DETAIL_V10.md and IMAGERY.md for current liveries, expanded photographic
coverage, installation, rendering costs and verification limits.
