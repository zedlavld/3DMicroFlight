# CirrusFlight v19 — three flight save slots

Open START / MENU, select Save flight, then choose Slot 1, 2 or 3. Each row shows Empty or Saved flight. Selecting a used slot replaces that slot's primary save and retains its previous verified save as a backup. Other slots are unaffected. B or Back cancels the picker.

Load saved flight and the title screen's Continue option open the same three-slot picker for loading. Empty slots are disabled. Loaded flights start paused, as before. Flight state saving remains separate from CSV recording.

| Slot | Path under `/3ds/CirrusFlight/` |
|---|---|
| 1 | `flight.sav` |
| 2 | `flight2.sav` |
| 3 | `flight3.sav` |

Each file has its own `.bak` recovery copy and temporary write. The existing v18 `flight.sav` automatically appears as Slot 1 without migration or file renaming. The save format remains version 1; flight physics, camera and saved-state content are unchanged.

Copy the update's `sdmc/` contents onto your existing card. Saves are not included in the ZIP, so installation does not replace them. Scenery remains under `/3ds/CanyonFlight/scenery/`.

Validation: 209 compiled ARM menu/save checks passed, including three independent slot values, isolated backup recovery, every slot's save/load menu routing, empty-slot rejection and cancellation from the title and pause menus. Recorder schema tests passed with model version 19. The reviewed layout preview executes production drawing functions with an approximate system font; real 3DS/emulator input and SD behavior remain unverified. See MENUS_V18.md for the underlying snapshot and recovery design.
