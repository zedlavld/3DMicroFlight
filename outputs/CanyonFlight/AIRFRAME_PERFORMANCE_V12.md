# Vision Jet shell repairs and rendering work — v12

The v11 importer removed SF50 triangles with vertices below a fixed height,
assuming they were landing gear. The cutoff also removed belly and wing-root
surfaces. V12 preserves all 7,294 source shell triangles, restoring 1,149 faces.
With generated fan and fully deployed gear, the SF50 has 7,628 triangles, inside
the existing 8,000-face budget. SR22 remains 7,102 triangles.

The imported shell matches every source triangle position after axis/scale
conversion. Original source openings remain; this is not a claim that the source
asset is watertight. The repair reduces welded body boundary edges from 537 to
440, the source count. Flap deformation now fades smoothly at its spatial mask
boundaries to reduce separation between nearby surfaces. Mechanism animation
and source dimensional fidelity retain their previous limitations.

## Rendering changes

- Resident world vertices are divided into chunks of at most 768 vertices, with
  bounding spheres computed once during upload. Conservative frustum rejection
  skips chunks wholly outside the top-screen view. The test includes near/far
  planes and both stereo eyes, including the off-axis convergence shift.
- Opaque world draws are grouped by material. Texture/environment setup occurs
  at most nine times per eye instead of once for every range. Depth testing
  continues to resolve visibility; there is no transparent scenery sorting.
- Terrain jobs wholly outside the 16 km imagery square bypass photo clipping.
  Material-region lookups are also hoisted out of the triangle loop.
- Physics still runs at 120 Hz. Terrain density, imagery resolution and source
  data are retained. Additional range metadata is statically bounded; no per-frame
  heap allocation is introduced. The restored aircraft does submit more triangles.

Actual performance depends on view direction, stereo and hardware. More draw
chunks can add CPU command overhead; the benefit of rejection must be measured.
No minimum FPS or measured handheld speedup is claimed.

## Verification

The 3DS build passes. Compiled ARM tests: 1,885,004 checks, zero failures, including
frustum rejection, sphere intersections, sampled stereo frustum edges, flight,
camera, aircraft swapping, mesh poses and terrain/imagery partition invariants.
Both texture mip chains pass complete pixel verification. The SF50 shell checker
verifies exact converted source positions and reports the restored triangle count.
The updated four-view asset preview is art/aircraft_preview.png. It is a CPU asset
preview, not an emulator or handheld screenshot.

## Azahar testing

Azahar 2126.2 is installed on this PC and loaded the homebrew. Automated visual
inspection could not complete because Computer Use approval timed out. Its log
reports unimplemented ConfigureNew3DSCPU calls; emulator CPU timing therefore
cannot establish actual New 3DS XL performance.

1. In Azahar, choose File > Open Azahar Folder. Copy the CONTENTS of this package's
   sdmc/ folder into Azahar's sdmc/ folder, including all eight scenery CFT files.
2. Open CanyonFlight.3dsx. Use New 3DS mode and normal CPU clock/emulation speed.
   The installed configuration already selected New 3DS and 100% CPU speed.
3. Check mapped controls in Azahar's input settings. In-game X swaps aircraft,
   ZR changes inside/exterior view, C-stick orbits, and ZL + D-pad zooms.
4. Select SF50, press Y to pause, and orbit underneath to inspect the belly,
   wing roots and tail. Inspect gear/flaps at multiple deployments, both views,
   and all three airports. Check scene edges while turning the camera quickly.
5. Compare steady views and moving flight. The paused top screen displays CPU/GPU
   times; the PFD shows FPS. Repeat on the New 3DS XL with stereo off and on.

No emulator performance measurements or successful emulator visual checks are
reported for this build. Source/asset licensing is in third_party/aircraft/ATTRIBUTION.md.
