# CirrusFlight v29: hardware flicker investigation

This is a rendering fix candidate for the widespread ground/sky flicker reported on New 3DS XL with v28. The hardware symptom has not yet been reproduced or confirmed fixed.

Changes:
- Keep terrain, exterior aircraft and clouds on the shared 3D shader. Previously each pass temporarily rebound the 2D shader even when no 2D draw followed it.
- Restore 2D state explicitly at the sky, rain and HUD boundaries, with depth writes disabled for overlays.
- Queue the top-left, top-right and bottom screen clears before any screen drawing rather than interleaving clears with partially submitted rendering.
- Retain weather, scenery, controls, save compatibility and flight dynamics.

The shader-binding cleanup removes a questionable transition pattern. Citro3D issue 39 documents inconsistent uniforms with unused program bindings, although its reproducer uses a geometry shader and is not proof of the cause here: https://github.com/devkitPro/citro3d/issues/39

Validation: SDK build completed without warnings; 11 recorder/version checks passed. Azahar OpenGL spot checks rendered the sky, terrain, exterior SF50, cockpit and storm rain correctly. The paused cockpit inspection displayed 60 FPS. These emulator observations do not establish New 3DS hardware stability or performance, and are not an exhaustive flight, cloud visibility, audio playback or stereo test. Terrain and audio numerical reports are retained from v28 because those implementations are unchanged.

## Install

Merge the ZIP's sdmc folder into the existing SD root, replacing the application and SMDH. Keep existing scenery and all three flight saves. No scenery download is required. This package contains no test saves.

## Hardware check

First use Clear weather with the 3D slider off; then select Clouds and Storm. Repeat with the slider enabled and switch between cockpit and exterior views. Report whether flicker affects the top screen only or both screens, and which of these settings changes it. Storm's occasional brief whole-scene brightening is intentional lightning; random triangles or sustained flickering are defects.
