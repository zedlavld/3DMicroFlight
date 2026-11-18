# CirrusFlight v27 playtest

V27 adds stronger sunlit terrain and rock-color differentiation (TERRAIN_V27.md). V26 adds bolder lower-screen instrument/menu text, stabilizes terrain detail grids and photo-tile seams, and fixes runway shoulders covering asphalt. See RENDERING_V26.md; v25 airport improvements remain included.

This is the complete playable prototype so far for New Nintendo 3DS XL. The package includes the app and offline scenery; no older release is required. Allow 1.5 GB free for installation, plus room for extraction on your PC. No runtime internet connection is needed.

## New 3DS XL installation

1. Extract the ZIP on your computer. Back up any existing `/3ds/CirrusFlight/` saves before updating.
2. Copy the **contents** of the extracted `sdmc` folder to the SD-card root, merging the `3ds` folder. Do not copy an extra `sdmc` folder onto the card.
3. Check these paths, then safely eject the card and launch CirrusFlight through your existing Homebrew Launcher:

       /3ds/CirrusFlight/CirrusFlight.3dsx
       /3ds/CirrusFlight/CirrusFlight.smdh
       /3ds/CanyonFlight/scenery/stream/manifest.json

Keep the CanyonFlight scenery folder name: the new app still uses that path. The old CanyonFlight executable is not required. Start with the stereo slider at zero.

Audio uses libctru NDSP. If Audio reports unavailable, your homebrew environment needs its DSP component (normally `/3ds/dspfirm.cdc` or support supplied by the launcher). This package does not include firmware. The simulator can run without audio.

## Azahar installation

1. Extract the ZIP to a normal folder; do not load the app from inside the ZIP.
2. Find the user-data directory used by your particular Azahar instance. A portable instance with a `user` folder beside its executable uses `user/sdmc`. Copy the **contents** of this package's `sdmc` into that instance's `sdmc`, merging `3ds`. Avoid `sdmc/sdmc/3ds` nesting.
3. Use Azahar's file-loading command to open the extracted `sdmc/3ds/CirrusFlight/CirrusFlight.3dsx`. Azahar supports homebrew `.3dsx` files; do not rename it to another ROM format.
4. In the emulator's controls configuration, explicitly bind the controls below, including ZL/ZR and the C-stick. Keyboard defaults vary and previously did not work in this session. Click the lower screen to operate touch menus. Select New 3DS emulation if your configuration offers it; use native resolution and normal CPU speed for initial comparisons.

If the scenery does not appear, check the emulator's SD directory rather than moving scenery beside the executable. Loading the app alone does not install the scenery into the emulated SD card. Emulator performance does not establish actual New 3DS XL performance.

## First flight

Choose **New flight**, **SR22**, **Sedona (KSEZ)**, **Approach**, and **Calm**, then Start. This gives an airborne view of the scenery. Try both cameras and the cockpit instruments before a takeoff test. Flagstaff (KFLG) and Grand Canyon (KGCN) are also available.

Stages: Takeoff, Climb, Cruise, Descent, Approach, Short Final, Slow Flight, and Engine-out Glide. The title and pause menus provide setup, Audio, and Exit. START or the lower MENU button opens the flight menu. Exit returns to the launcher; save explicitly before exiting.

## Controls

| Control | Action |
|---|---|
| Circle Pad | Pitch/roll; pull down to raise the nose |
| L / R | Left/right rudder; both cancel |
| ZR | Switch cockpit/exterior camera |
| C-stick | Look inside / orbit around the aircraft outside |
| Tap ZL without another control | Recenter camera |
| ZL + D-pad Up / Down | Exterior zoom in / out |
| D-pad Up / Down | Throttle increase / decrease |
| D-pad Left / Right | Pitch trim |
| A | SF50 landing gear toggle when airborne; SR22 has fixed gear |
| B | Cycle flap detents |
| X | Swap SR22/SF50 at the current position |
| Y | Pause/resume |
| START | Flight menu |
| SELECT | Restart the selected stage |
| ZL + SELECT | Cycle stages and restart |
| ZL + B | Both wheel brakes |
| ZL + L / R | Individual main-wheel brake plus rudder |
| ZL + A | Simplified engine cutoff/restart |
| ZL + Y | Start flight recording; again pauses and saves CSV |
| ZL + X | Save frame-time snapshot and pause |

Menus: D-pad Up/Down selects, Left/Right changes settings, A accepts, B returns. The lower touchscreen offers AIRCRAFT, AIRPORT, WIND, VIEW, MENU and FLAPS. The lower PFD remains available in both flight cameras; menus temporarily replace it.

## Three save slots

Use MENU > Save and choose Slot 1, 2 or 3. Use Load, or Continue on the title screen, to choose a saved slot. Files are `flight.sav`, `flight2.sav`, and `flight3.sav` under `/3ds/CirrusFlight/`, with separate `.bak` backups. The release contains no saved flights. Back up this directory before replacing an installation. Saving is explicit, not automatic.

## Suggested playtest (about 15 minutes)

1. At KSEZ, inspect both aircraft outside: wing roots, tail joins, gear, textures, camera orbit and zoom. Inspect each cockpit looking forward, sideways and down toward the footwell.
2. Try each airport's Approach start. Check runway/terrain alignment and imagery loading while flying a few miles. The photographic coverage is the Sedona-Flagstaff-Grand Canyon/South Rim corridor, not all Arizona.
3. Try Takeoff, Climb, Cruise, Slow Flight, Approach and landing in each aircraft. Check throttle, flaps, trim, rudder, SF50 gear, brakes and engine-out behavior. Note aircraft, stage, wind, speed and the inputs that reproduce a problem.
4. Save different flights to all three slots, exit, relaunch and load each slot. Check position, aircraft and camera restoration. Try volume and mute.
5. Capture performance as below. After mono tests, try stereo on actual hardware and repeat under matched conditions.

## Measure frame rates

Fly for 10-15 seconds after selecting the aircraft, view and stereo setting, allowing scenery to settle. Press **ZL + X** to save the rolling timing window and pause. Read average FPS, p95 frame time and sample count on the top screen. Resume with Y. Repeat for SR22/SF50 and cockpit/exterior views at the same airport/stage/weather.

CSV files are written to `/3ds/CirrusFlight/performance/frame_<clock>.csv` on the physical or emulated SD card. The 300-frame window spans about 5 seconds at 60 FPS or 10 seconds at 30 FPS. Keep stereo settings steady; changing aircraft, camera or pause state resets the window. Flight recordings use `/3ds/CirrusFlight/logs/`.

Share the performance CSVs with any screenshots and reproduction steps. `tools/analyze_frame_profile.py` in this package can summarize a CSV with Python. Stationary SF50 scenes in Azahar displayed about 59-60 FPS at 100% speed. Hardware FPS and moving-flight performance remain unverified.

## Scope and known limits

Includes textured SR22/SF50 exteriors, approximate virtual cockpits, live Garmin-inspired instruments, three airports, USGS elevation data, 538 streamed NAIP tiles plus regional fallback packs, stereo, menus, saves and synthesized audio. Terrain elevation sampling varies by area (30 m Sedona, 60 m Flagstaff, 100 m Canyon, 500 m regional background). Buildings seen in aerial imagery are mostly flat ground features.

The flight model is under calibration; inertia, stability and some engine behavior remain approximations. This is not a training or navigation tool. Cockpit controls and avionics are not a complete real Garmin implementation. Native rendering, audio, stereo and performance still need the current-build runtime playtest. See RENDERING_V26.md and the v25 airport notes for validation details. The reported SF50 V-tail triangle remains unresolved.

Aircraft asset credits, CC BY 4.0 license links and modification notices are in `credits/ATTRIBUTION.md`. Scenery credits: USGS, USDA, The National Map (public-domain NAIP imagery and USGS 3DEP terrain). No X-Plane meshes, textures, firmware or emulator are included.

