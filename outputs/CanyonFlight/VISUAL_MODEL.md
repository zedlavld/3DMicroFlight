# Exterior refinement v04 / details v05

This revision refines the original models further while keeping the existing
flight controls, camera modes and physics parameters.

Changes since v03:

- Shaped left/right windshield panes with a centre post and curved edges.
- Subtle blue-gray glazing gradients; rounded rectangular SF50 side windows.
- SR22 door, baggage-hatch and handle outlines; SF50 left door/right hatch outlines.
- Finer wing airfoil sections, softened tip caps, control hinge seams and small
  position-light housings. Inboard trailing-edge geometry responds to flap position.
- Splayed landing-gear struts, SF50 braces and the existing SR22 wheel fairings.
- A dorsal attachment fairing joins the SF50 nacelle to the upper fuselage.
- Removed per-frame triangle sorting from the opaque aircraft draw pass because
  the aircraft now uses a depth buffer. Terrain rendering is still approximate.

Gear-down counts: 3182 triangles for SR22, 3174 for SF50. The shared mesh/VBO
capacity is 4500 triangles. Paint atlases remain 256x256 RGB565 per aircraft.

Quarter, side, top and front desktop previews were generated from the compiled
ARM mesh code and visually inspected. Geometry/UV, flap-deflection, gear visibility
and existing physics/camera regression checks pass. Compilation/linking pass.
This revision's 3DS GPU appearance, control feel and frame rate still need a
hardware test. A software asset preview is not a handheld screenshot.

These remain lightweight reference-based approximations, not manufacturer CAD
or X-Plane geometry. Control surfaces are not independently articulated in full;
flap deflection is a simplified mesh deformation. Lighting, surface finish,
complete antennas, vents, gear doors and close-up details remain unfinished.
Door outlines are surface strokes, not opening doors. Position-light housings
are not an operating aircraft lighting system.

Original visual references retained from v03: Cirrus SR22 POH P/N 13772-002 and
Cirrus SF50 AFM P/N 31452-001, each Figure 1-1, printed page 1-4. See
FLIGHT_MODEL.md for the source links and flight-model validation scope.

V05 also adds wingtip red/green lens colours, SF50 fan detail, and SR22 antenna
silhouettes. Terrain and airport work is documented separately in SCENERY.md.

## V07 surface joins

Main wings and tailplanes now extend to the centerline inside the fuselage,
rather than stopping at a shared 0.5 m lateral root. The SF50 V-tail root was
lowered into the tailcone, and both aircraft gain small white wing-root fairing
surfaces bridging the body skin and wing. A short root span interval refines the
transition. These correct light-through geometry seams; the paint remains an
approximate swatch atlas rather than a continuous production livery unwrap.

## V08 refinement

See DETAIL_V08.md for the current detail changes, geometry counts, terrain
comparison and verification limits. Earlier counts above describe older builds.

## V10 update

See DETAIL_V10.md and IMAGERY.md for current liveries, expanded photographic
coverage, installation, rendering costs and verification limits.

## V11 replacement exteriors

Current meshes and liveries are adapted third-party assets. See AIRCRAFT_V11.md
and third_party/aircraft/ATTRIBUTION.md. Older original-mesh descriptions above
refer to the retained procedural source implementation, not the active renderer.
