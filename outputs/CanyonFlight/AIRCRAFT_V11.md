# Aircraft swapping and replacement-model search - v11

## Swap controls

Press X or tap the lower-screen X: TO SR22 / X: TO SF50 button. The button names
the aircraft that will be selected. A short top-screen notice confirms the change;
the PFD remains visible. Both input routes call switchAircraft() in flight.c and
are combined so an X press and touch in the same frame do not switch twice.

Switching retains position, velocity, quaternion attitude, wind, temperature,
selected airport, throttle/engine fraction, flap deployment, camera and pause state.
It uses the new airframe's nominal mass and recalculates pitch trim and PFD aero
values. Angular rates, control deflections and lateral trim offsets are cleared.
The SR22 forces fixed gear down; the SF50 retains the current gear command and
deployment. Airspeed is not silently changed: retrim and adjust power as needed
for the newly selected aircraft. A crashed flight stays crashed until restarted.

SELECT still restarts the selected aircraft's approach. Airport selection also
starts a new approach. Swap alone no longer teleports to the approach start.
The simulation accumulator is cleared on swapping; the camera is retained.

Tests cover both directions, in-place position/attitude/velocity preservation,
configuration/environment transfer, correct mass and gear, invalid/no-op targets,
aerodynamic display refresh and crash-state preservation. The app builds, but
button handling and the notice still need testing on a New 3DS XL.

## Imported replacement exteriors

Source credits and licenses are in third_party/aircraft/ATTRIBUTION.md. The
original GLBs, obtained via Sketchfab's official signed-in download controls,
are included under third_party/aircraft/. They replace the v10 procedural meshes.

- Cirrus SR22 by KOG_THORNS, with original on-texture E. BARANGER credit:
  https://sketchfab.com/3d-models/cirrus-sr22-cba602c99c524cd4b40e5c2e5f9c5b4f
  About 57,900 source triangles including cockpit detail; adapted exterior is
  7,102 triangles. Hidden cockpit, seats and avionics were omitted. Exterior
  components were QEM-simplified with open boundaries preserved and UVs transferred.
- cirrus vision Sf50 by hilos run, with supplied EGP-JET3D texture markings:
  https://sketchfab.com/3d-models/cirrus-vision-sf50-d46dd06b4b5646acaed90993db34d639
  About 7,300 source triangles. Low static gear fragments were removed; adapted
  source exterior has 6,145 triangles. Simulator gear and fan yield 6,479 with
  gear down, below the 8,000-triangle budget.

The source listings/download dialogs identify CC BY 4.0. Source links, creator
credit, license links and modification notices are retained with the assets:
https://creativecommons.org/licenses/by/4.0/

Source axes were converted and length/span independently fitted to the existing
simulator dimensions. This improves the exterior detail but does not certify
fuselage cross-sections, generation-specific details or flight performance.
The physical flight models remain the same provisional SR22/early SF50 profiles.

The supplied liveries were retained, resized into 512-square diffuse atlases with
seven RGB565 mip levels and a protected material palette. Glass is approximated
as opaque tinted material, because cockpit interiors and full glass transparency
are outside this rendering pass. Source propeller geometry rotates on the SR22;
a small procedural inlet fan rotates on the SF50. Flap deformation and SF50 gear
animation are approximate; there are no articulated gear-door mechanisms.

Use tools/import_aircraft.py with numpy, Pillow and fast-simplification to rebuild
from the bundled GLBs. It writes the converted mesh binaries, PNG atlases and mip
chains. tools/paint_atlas.py repacks edited PNGs without regenerating the model.
The legacy v10 procedural atlas generator requires an explicit --legacy flag.
Conversion counts, selected source parts, scaling and hashes are recorded in
validation/imported_aircraft_report.json. Derived mesh binaries are linked into
the executable; they do not add SD-card installation steps.

Build with devkitPro make before running the bare ARM tests, then add -Ibuild
and build/sr22_imported_mesh.bin.o build/sf50_imported_mesh.bin.o to the existing
test compile command. These link the same embedded mesh data used by the app.
The numerical tests and pixel-by-pixel texture/mip validation pass. The previews
in art/ render the actual ARM-generated meshes with the derived atlases; they
are desktop previews, not evidence of device FPS or stereo/GPU appearance.
