# CirrusFlight v24 — cockpit cleanup and frame measurements

The looked-down cockpit had an open footwell behind/below the instrument panel, exposing outside scenery. A two-triangle forward closure now joins the panel/footwell area to the floor. Fixed interior faces also receive subtle directional shading so sidewalls, console and glare shield read as volumes rather than uniformly colored slabs. Live instrument artwork remains unshaded. These changes preserve the static/live cache; representative meshes are 787 SR22 / 701 SF50 triangles and live updates remain 197 / 157 triangles.

The two bottom help lines are hidden during normal interior flight so they no longer cover the small gauges/controllers. They remain available in exterior view and when paused. START still opens the menu, ZR selects camera, C-stick looks/orbits and tapping ZL recenters. The lower PFD remains available in either flight camera. Software previews include the new geometry and directional shading but remain approximate rasterizations with illustrative backgrounds, not native GPU or emulator captures.

## What is the frame rate?

There is no verified current-build FPS measurement yet. The v22 75–78% figure measures reduced cockpit vertex updates, not an FPS gain. No running Azahar process was found during this follow-up. Device timing, native rendering and stereo comfort remain unverified.

V24 adds a rolling 300-frame profile using actual wall-clock frame intervals, independent of the simulation's 100-ms elapsed-time cap. The old smoothed display used that capped time and could hide long stalls. FPS is now frames / total elapsed time over the rolling window; p95 and worst frame times include real stalls. Aircraft, camera, pause state, menu screen and stereo on/off changes start a fresh window. Stereo slider changes within the nonzero range are not separated, so hold the slider steady for a comparison.

### Capture a measurement

1. Start the desired flight, camera and stereo setting. Allow scenery to settle, then fly for at least 10–15 seconds.
2. Press **ZL + X**. This chord saves the current timing window and pauses. Plain X still swaps aircraft.
3. Read the saved average FPS, p95 time and frame count on the top screen. Find the CSV under `/3ds/CirrusFlight/performance/frame_<clock>.csv`.
4. Resume with Y or the menu. Repeat for SR22/SF50, interior/exterior and stereo off/on under matched conditions.

Use `python tools/analyze_frame_profile.py <csv>` to create a summary. The CSV includes wall-clock frame milliseconds, most recently completed Citro3D CPU/GPU counters and the aircraft/camera/pause/stereo context. The counters can lag the corresponding wall-clock row; use the frame intervals as the end-to-end FPS evidence. A 300-frame window covers about 5 seconds at 60 FPS or 10 seconds at 30 FPS. Menu/pause profiles are distinct from active-flight measurements. Saving occurs while paused and outside the captured window; no performance CSV writes occur during normal flight.

## Verification and install

- 60,358 compiled ARM cockpit/camera checks passed.
- 62 cache checks passed; previous geometry still matches v21 to 1 micrometre, apart from the two added closure faces. Native directional shading is a deliberate color change.
- Frame-statistics tests passed, including a simulated 250-ms stall, rolling-window wrap, context resets, chronological CSV output and save failures. These timings are synthetic fixtures, not a measured FPS result.
- Menu/save, recorder and layout checks passed. The app compiles with devkitARM. Actual SD capture, GPU rendering and measured FPS still need a runtime pass.

Copy the update ZIP's `sdmc/` contents onto the existing SD card. Scenery, audio and all three saves remain compatible. This release is distributed as an update; the previous full scenery archive remains available separately. CSV flight recordings identify model version 24.
