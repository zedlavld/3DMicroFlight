# CirrusFlight v20 — virtual cockpit

Cockpit view now includes an original, simplified 3D interior instead of an unobstructed camera at the aircraft origin. Both aircraft have an open windshield and side windows, roof/window framing, glare shield, instrument panel, sidewalls, seat backs, a side control grip and a center power lever. The SF50 layout includes a third panel display. Geometry is approximate and is not a dimensional reproduction of the installed X-Plane interiors. No X-Plane meshes or textures were copied.

## Controls and instruments

- ZR or the VIEW softkey switches between exterior and cockpit views.
- C-stick looks around the cockpit, including down towards the panel and out the side windows.
- Tap ZL to recenter. Existing exterior orbit/zoom controls are unchanged.
- The lower-screen PFD remains available in either flight camera mode. Menus still temporarily replace that lower display.

The top panel's small PFD surfaces show live attitude, approximate IAS/CAS, altitude in feet and true heading. At 400 x 240 they are secondary cues; use the lower PFD for readable instruments. The center screen shows a schematic bearing cue towards the selected airport over a static grid, not a second terrain/navigation database. The cockpit switches, grip, power lever and gear knob are decorative in this release. Continue using the existing controls; there is no touch/click interaction with the 3D panel or animated stick/lever travel.

## Rendering and camera

The native renderer uses the existing shader with vertex colors and depth-tested cockpit triangles. It finishes the scenery command list, clears only the current eye's depth buffer and draws the interior with its own 7-cm near plane. Window openings preserve the scenery color underneath. This avoids cutting the nearby panel with the terrain renderer's 2-m near plane and prevents terrain depth values from obscuring interior surfaces. The sequencing follows Citro3D's [frame split](https://github.com/devkitPro/citro3d/blob/master/source/renderqueue.c) and [framebuffer clear](https://github.com/devkitPro/citro3d/blob/master/source/framebuffer.c) implementation. A simpler painter fallback is retained if the main renderer cannot initialize.

Mesh preparation/uploads happen once per frame and are shared by both eyes. Representative neutral meshes contain 435 SR22 and 557 SF50 triangles; digit/horizon changes alter the exact count. Capacity is 1024 triangles, with two bounded vertex buffers totaling 216 KiB of linear memory plus 80 KiB of CPU mesh storage. No cockpit textures are allocated. Uploading the representative SF50 mesh is about 60 KiB per rendered frame, separate from the scenery upload budget. Hardware timing remains to be measured.

Interior stereo uses a much smaller eye offset than the exterior chase camera (maximum 8 mm per eye, convergence at 2 m). The existing slider still sets depth; zero draws one eye. This makes near cockpit geometry practical without applying the old distant-view stereo offset to the panel. Comfort and visual integration need a real 3DS check.

Looking up/down also exposed a camera-axis error: the old up-vector was not perpendicular to the forward vector at nonzero look pitch. Its signs are corrected. Orthonormality checks now cover 405 combinations of aircraft attitude and interior look direction.

## Verification and installation

40,659 compiled ARM cockpit/camera checks passed, including finite/nondegenerate geometry, small-buffer bounds, changing instrument state, extreme attitude clipping and orthonormal camera axes. The eight-view preview sheet uses the production ARM-generated meshes with software clipping and depth rasterization and an illustrative background. It is a layout preview, not actual terrain, Citro3D output or an emulator screenshot. No frame-rate improvement or hardware cockpit verification is claimed.

Copy the update's `sdmc/` contents onto the existing SD card, then launch CirrusFlight. The installed scenery and three save slots remain compatible. Physics and save format are unchanged; CSV recordings identify model version 20.
