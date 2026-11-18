# Azahar verification and aircraft texture correction — v13

The repeated emulator test succeeded using an isolated copy of installed Azahar
2126.2, with the executable renamed CanyonFlight-Test.exe. Distinct process naming
lets Computer Use distinguish this window from another running Azahar instance.
The separate profile and virtual SD folder preserve the user's installed emulator
settings and saves. Sedona NAIP imagery loaded successfully.

## Fix discovered in the emulator

Aircraft RGB565 textures were vertically flipped during packing even though their
UV convention already accounted for V orientation. The pixel checker validated
consistency with that same incorrect convention; it could not establish GPU
orientation. The CPU preview therefore looked correct while actual aircraft
liveries had misplaced colors and missing window appearance.

V13 removes the extra row reversal from both import_aircraft.py and paint_atlas.py,
and adjusts the consistency checker. All seven mip levels are repacked for both
aircraft. The converted meshes and flight model are unchanged from v12.
The corrected SF50 shows a white upper fuselage, red belly and dark windows in
Azahar. The SR22 shows its white/green livery and dark windows. Ground-image packing
is a separate pipeline and is unchanged.

## What was checked

An isolated test build starts paused in the SF50 and cycles rear-quarter, side,
front, underside, rear and high angles with gear/flap poses. Belly and wing-root
surfaces were visible in the inspected exterior views. A fixed side-view diagnostic
then established the texture correction. The final livery test alternates SR22
and SF50 every 15 seconds. The lower-screen instruments remain present.

The production build still starts in normal flight and uses its existing controls.
The exact final harness source is tests/visual_test_main.c, for use as main.c only
in a separate copy of the project. It is not compiled by the normal Makefile.
It bypasses keyboard input for visual inspection. Manual and automated keyboard
commands did not produce a confirmed response in the earlier session; keyboard,
C-stick and stereo behavior are not declared verified by this visual test.

Representative paused exterior samples showed 60 app FPS, approximately 3–4 ms
CPU and GPU processing readings, with Sedona imagery enabled. These are emulator
observations, not a handheld benchmark or a controlled before/after comparison.
Multiple desktop applications, including another emulator, were running. Flight,
scene rebuilds, all airports and physical 3DS stereo need separate verification.

## Validation

Production and visual-test builds pass. Both corrected texture mip chains pass
complete pixel consistency checks against their PNG atlases. The v12 compiled ARM
suite (1,885,004 checks, zero failures) remains the applicable math/mesh result;
physics and geometry were not changed for v13. Updated texture hashes are in
validation/aircraft_texture_report.json. Details of the gap repair and renderer
optimizations remain in AIRFRAME_PERFORMANCE_V12.md; v13 supersedes its statement
that no emulator visual inspection had completed.
