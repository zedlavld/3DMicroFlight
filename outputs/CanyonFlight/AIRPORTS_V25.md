# CirrusFlight v25: airport surfaces and takeoff fixes

## Corrections

The runway/apron terrain conditioning used the apron rectangle in place of the runway rectangle. At the apron ends this introduced a step through the runway. New tests using the actual embedded terrain reproduced SF50 takeoff crashes at KFLG and KGCN; the terrain now uses the union of both rectangles. Six takeoffs (both aircraft at all three airports) now reach 8 m AGL without a crash and before the far runway end. These are compiled ARM numerical tests with a feedback test pilot, not an emulator or hardware flight, and not a validation against published takeoff performance.

The visible pavement previously sat 1.3 m above the wheel-contact surface, and markings up to 2 m above it. This could hide the lower airframe and put the cockpit camera below the drawn runway. Pavement and markings now sit only centimetres above the same sloped surface as the wheels. The software fallback runway also follows the slope instead of using an unrelated elevation plus 4 m.

The interior camera is now an estimated 0.55 m above the dynamics CG. The cabin uses that same eye anchor, preserving the panel layout while raising the outside viewpoint. Exterior framing and eye clearance are checked at each airport. This is an approximate seating position, not surveyed cockpit geometry.

## Airport appearance

- Original 128 x 128 aggregate/asphalt material, with five mip levels (128 through 8 pixels), 43,648 bytes in RGB565. It repeats at 4 m intervals and uses filtered mipmaps at distance.
- Bounded 40 m pavement strips add subtle broad weathering variation. Runway, taxiway and apron pavement share one texture binding per eye.
- Generic touchdown rubber streaks, paired aiming-point bars, and T-shaped parking marks add surface detail. Paint keeps its own color material. These markings and buildings are illustrative, not surveyed airport replicas.
- USGS/USDA aerial imagery and the existing 538 stream tiles remain compatible. No scenery download is required for an existing full v24 installation.

## Installation

Exit the simulator, extract the v25 update, then merge the **contents** of its `sdmc` folder into the physical or emulated SD root. Replace `3ds/CirrusFlight/CirrusFlight.3dsx` and its SMDH. Keep `3ds/CanyonFlight/scenery/` and your three save slots. The title should show v25. Restart a fresh Takeoff stage to test the corrected terrain rather than loading a flight already marked crashed.

For a fresh install use `CirrusFlight-v25-Playtest.zip`, which contains all scenery. Follow `PLAYTEST_V25.md`.

For a takeoff test, choose Calm weather, use D-pad Up to increase throttle, and leave the wheel brakes released. Apply gentle back pressure after about 68 KIAS in the prototype SR22 or 90 KIAS in the prototype SF50, then establish a modest climb. These are simulator test cues, not real aircraft operating instructions. ZL+B applies both brakes; plain L/R are rudder.

## Verification and remaining work

The SDK build passes. The airport suite checks actual-terrain takeoffs, runway continuity across the apron ends, visible pavement height, camera clearance, exterior framing, material grouping and bounded geometry. The broader ARM physics/scenery suite, cockpit/camera suite, cockpit cache suite and recorder checks also pass; exact counts are recorded in `validation/release_v25.json`.

Azahar testing was stopped at the user's request because another app is using it. No current v25 GPU screenshot, native runtime FPS, audio playback or stereo result is claimed. The fixes address specific reproduced defects; additional reported graphical issues still need runtime inspection when the emulator becomes available. ZL+X still captures actual wall-clock frame intervals for later performance comparison.
