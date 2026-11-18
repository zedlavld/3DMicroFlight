# CirrusFlight v18 — title, menus and saved flights

The application is now named **CirrusFlight**, including its launcher title, custom icon, executable and in-flight heading. The development project folder retains its old name. The top-screen title artwork is original vector canyon scenery with a flight mark; it renders at 400 x 240 without a new texture allocation. It is drawn identically for both eyes. Existing stereo flight rendering is unchanged.

## Start a flight

The title menu offers New flight, Continue saved flight and Exit. Continue is disabled until a valid primary or backup save is found. New flight opens a setup screen:

| Setting | Choices |
|---|---|
| Aircraft | SR22 / SF50 |
| Airport | Sedona KSEZ / Flagstaff KFLG / Grand Canyon KGCN |
| Stage | Takeoff, climb, cruise, descent, approach, short final, slow flight, engine-out glide |
| Weather | Calm / ISA, breeze, gusts |

Use D-pad up/down to select a row, left/right to change a setting, A to activate, B to go back. Touch either half of a setup row to decrement/increment its setting; touch an action row to activate it. Start flight initializes the selected conditions using the existing phase presets. These remain simulator starts rather than published aircraft procedures.

## In flight

START or the bottom-screen MENU button opens the pause menu. Simulation time and recording samples stop advancing while any menu is open. The lower PFD returns when you resume. All existing flight/camera controls remain; the former PAUSE softkey is now MENU, and Y still pauses directly.

The pause menu offers Resume flight, Save flight, Load saved flight, New flight, Title screen and Exit. START/B from the pause menu resumes. Exit opens a confirmation with Cancel selected first; confirmation returns to the launcher. Exit does not automatically save the flight state. Save before exiting if you want to retain progress.

## Save and resume

Save flight writes one manual slot at `/3ds/CirrusFlight/flight.sav`. It preserves the flight's position, velocity, attitude, body rates, aircraft, mass, trim, engine state, controls, flap/gear state, selected airport/stage, weather state and camera view. Load/Continue restores that snapshot paused; press Y, or open the menu and choose Resume flight. Saving a crashed aircraft preserves the crash state; use SELECT to restart its stage after loading if desired.

The file has a version/size header, checksum and sanity validation. Saving writes and verifies a temporary file before replacing the primary, retaining a previously verified primary as `.bak`. Loading falls back to the backup if the primary is missing or invalid. Failures display a message and keep the simulation paused. This scheme provides recovery from tested application-level failures; physical SD durability/power-loss behavior has not been verified. Version 1 uses the current ARM state layout; future structure changes require a format-version update or migration.

Flight state saves are separate from the 5-Hz CSV recorder. CSV logs now go to `/3ds/CirrusFlight/logs/` and identify model version 18. Beginning or loading a different flight first attempts to save any buffered recorder data; a failed log save blocks that transition. Exit still attempts to save buffered CSV data. CSVs themselves are not restored as part of a saved flight.

## Installation and compatibility

Copy the update ZIP's `sdmc/` contents to the SD-card root. Launch `/3ds/CirrusFlight/CirrusFlight.3dsx`. Keep the existing `/3ds/CanyonFlight/scenery/` folder: this release intentionally reads that installed scenery location, so the 1.3-GB dataset does not need to be copied or renamed. Previous logs remain in their original folder. The old CanyonFlight launcher entry may remain on an existing card; its `.3dsx` and `.smdh` can be removed without deleting scenery. Previous release archives remain unchanged.

## Verification

- Production app compiled with devkitARM, including menu, save and drawing integration.
- 178 compiled ARM menu/save checks passed: all 48 aircraft/airport/stage save round trips, navigation, disabled Continue, exit cancellation/confirmation, short writes, open/close errors, failed replacement, checksum damage, backup recovery, truncation and invalid states. Filesystem tests use an in-memory fault-injection implementation, not an SD card.
- Recorder tests passed with v18 identifiers and the existing 40-column schema.
- Layout preview executes the production splash/menu drawing functions on ARM, captures draw commands and rasterizes them with Pillow. The reviewed six-screen sheet has no detected right-edge text overflow. Its font approximates the 3DS system font; it is not a GPU or emulator capture.
- Flight coefficients and scenery are unchanged from v17. Existing numerical phase/terrain/cache reports are retained as regression evidence for that unchanged code. Actual emulator/device menu input, SD save/load and frame timing still need a runtime pass.
