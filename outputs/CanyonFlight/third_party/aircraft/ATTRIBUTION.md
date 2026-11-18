# Third-party aircraft exterior assets

## Cirrus SR22

"Cirrus SR22" by KOG_THORNS:
https://sketchfab.com/3d-models/cirrus-sr22-cba602c99c524cd4b40e5c2e5f9c5b4f
Creator profile: https://sketchfab.com/ioai25312
Official GLB download obtained through the signed-in Sketchfab Download 3D Model
control. Source GLB metadata identifies this title/author/source and CC-BY-4.0.
The diffuse texture also credits "Cirrus SR22: BARANGER Emmanuel 2013"; that
original on-texture credit is preserved and acknowledged here.

## cirrus vision Sf50

"cirrus vision Sf50" by hilos run:
https://sketchfab.com/3d-models/cirrus-vision-sf50-d46dd06b4b5646acaed90993db34d639
Creator profile: https://sketchfab.com/hilosrun
Official original-format GLB downloaded through Sketchfab's signed-in download
control. The download dialog identifies Creative Commons Attribution 4.0.
The supplied texture includes EGP-JET3D / EGPJET3D ENDO MODEL markings, retained.

## License and modifications

Both source listings/download dialogs specify CC BY 4.0:
https://creativecommons.org/licenses/by/4.0/
https://creativecommons.org/licenses/by/4.0/legalcode.en

These files are third-party assets, not original Canyon Flight artwork and not
endorsed by Cirrus, the creators, or Sketchfab. No manufacturer approval or
independent dimensional certification is implied. Source GLBs are bundled here.

Canyon Flight adaptations: removed hidden SR22 cockpit/instruments/seating,
simplified SR22 exterior with QEM while preserving open borders, transferred UVs,
converted axes and scaled span/length to the simulator's dimensions, approximated
glass as opaque, repacked diffuse textures into 512-square atlases with protected
material swatches and RGB565 mip chains, preserved the complete SF50 source shell and added
lightweight animated gear/fan, and added approximate flap deformation/prop rotation.
Original source liveries and embedded credits are retained. These modifications
are documented in tools/import_aircraft.py and validation/imported_aircraft_report.json.

Keep attribution, source links, license links and modification notices with any
redistributed copies or further adaptations. The third-party licenses apply to
these source/derived assets; they do not grant rights in trademarks depicted.

V12 removes the erroneous SF50 height-based face deletion from v11, restoring
1,149 belly/wing-root triangles, and smooths the approximate flap deformation mask.

## Boeing 737 MAX (v33)

"737 MAX Lion Air" by Sofyan Kurniawan:
https://sketchfab.com/3d-models/737-max-lion-air-2747cad8b4c64122abe992c9ad1e8bd1
Licence: Creative Commons Attribution 4.0 (CC BY 4.0),
https://creativecommons.org/licenses/by/4.0/
Source file: b737.glb. The Lion Air livery is the creator's and is retained.
Used for the simulator's Boeing 737-800 exterior (the model depicts a 737 MAX).

## F-16 (v35.24)

"F-16 jet" by rickslash:
https://sketchfab.com/3d-models/f-16-jet-69c32a15b8e24d6ebf276b76dde3ab95
Creator profile: https://sketchfab.com/RickSlash
Licence: Creative Commons Attribution 4.0 (CC BY 4.0),
https://creativecommons.org/licenses/by/4.0/
Source file: f16_rickslash.glb (downloaded as f-16_jet.glb, GLB with 2k textures,
2026-11-17). The GLB metadata names the title, author, source and CC-BY-4.0.
The South Carolina Air National Guard markings are the creator's and are retained.
Modifications: converted by tools/import_aircraft_v33.py (all 15,290 source
triangles kept; axes, scale and placement as below; base-colour texture repacked
into the 512-square RGB565 atlas, normal and metal/roughness maps not used, glass
drawn opaque). The source is modelled in flight without landing gear, so the
simulator adds its own simple gear (wheels, hubs and struts, 348 triangles,
proceduralGear in the importer); that gear is original work, not part of the
source model.

Until v35.23 the simulator used "F-16 Fighting Falcon - Fighter Jet - Free" by
bohmerang (CC BY-NC-SA 4.0). It was replaced so that the whole game can be shared
without a non-commercial restriction; its files are no longer in the project.

## Airbus A321 (v33.08)

"Airbus A321-232" by piggyrockstarYT:
https://sketchfab.com/3d-models/airbus-a321-232-22721a53dabc4ca282fbda3b8cd7f030
Licence: Creative Commons Attribution 4.0 (CC BY 4.0),
https://creativecommons.org/licenses/by/4.0/
Source file: a321.glb. This file is MODIFIED from the original download by
tools/clean_a321.py:
- the cartoon eyes and mouth are removed;
- the flaps, ailerons, elevators and rudder are removed (their node matrices
  were sheared), and two trailing-edge panels (flap_panel_R / flap_panel_L)
  are added;
- the main-gear struts are trimmed below the wing.
It is then converted by tools/import_aircraft_v33.py as described below.
All 10,603 triangles are kept; scaled to 44.51 m long and 35.80 m span.

## v33 modifications (tools/import_aircraft_v33.py)

737, F-16 and A321: axes converted to the simulator's body axes (x forward, y right, z down),
scaled to published dimensions (737 MAX 8 length 39.52 m and span 35.92 m;
F-16 length 15.06 m and span 9.96 m with wingtip missiles; heights use the
length scale), placed so the main wheels match the simulator's gear geometry,
diffuse textures cropped and repacked into one 512-square RGB565 atlas with
flat-colour material swatches (sRGB), glass drawn opaque, landing gear folded
up approximately when retracted. 737: all 13,332 source triangles kept. F-16 (since v35.24):
all 15,290 source triangles kept, nose placed and belly aligned to match the
earlier F-16 so the flight model and lights are unchanged; normal/roughness maps
not used. Report: validation/imported_aircraft_v33_report.json.
