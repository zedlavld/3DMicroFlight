# Third-party notices

3DMicroFlight's own code, tools, documentation, artwork and audio are released
under the MIT licence (see [LICENSE](LICENSE)). The items below are not ours and
keep their own licences and terms. Keep this file, and
[outputs/CanyonFlight/third_party/aircraft/ATTRIBUTION.md](outputs/CanyonFlight/third_party/aircraft/ATTRIBUTION.md),
with any copy or adaptation you redistribute.

## Aircraft models

Full credits, source links and the list of modifications are in
[ATTRIBUTION.md](outputs/CanyonFlight/third_party/aircraft/ATTRIBUTION.md).
Source files: `outputs/CanyonFlight/third_party/aircraft/*.glb`; converted
meshes and textures: `outputs/CanyonFlight/data/*_imported_mesh.bin`,
`*_texture.bin`, and `art/*_paint.png`.

| Aircraft | Title and creator | Licence |
|---|---|---|
| Cirrus SR22 | "Cirrus SR22" by KOG_THORNS (texture credit: BARANGER Emmanuel 2013) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| Cirrus SF50 | "cirrus vision Sf50" by hilos run (texture: EGP-JET3D) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| Boeing 737 | "737 MAX Lion Air" by Sofyan Kurniawan | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| Airbus A321 | "Airbus A321-232" by piggyrockstarYT | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| F-16 | "F-16 jet" by rickslash (landing gear added by us) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |

All five were modified for the simulator (simplified, re-scaled, re-axed,
textures repacked into 512-square RGB565 atlases); the details are in
ATTRIBUTION.md.

All five licences allow use, sharing and changes, including commercial use,
as long as the creators are credited and changes are noted. (Until v35.23 the
F-16 was a CC BY-NC-SA model; it was replaced in v35.24 and is no longer in the
project.)

## Map and elevation data

| Data | Source | Terms |
|---|---|---|
| Aerial photos (ground imagery) | USDA Farm Service Agency NAIP, via the USGS National Map and Microsoft Planetary Computer | US public domain |
| Elevation | USGS 3D Elevation Program (3DEP) | US public domain |
| Night city lights and roads | US Census Bureau TIGER/Line 2023 | US public domain |
| Airports and runways | [OurAirports](https://ourairports.com/data/) | Public domain |

Credit is not legally required for public-domain data, but we give it: imagery
"NAIP, USDA Farm Service Agency"; elevation "USGS 3DEP". Neither agency endorses
this project.

## Libraries

The 3DS build links devkitPro's [libctru](https://github.com/devkitPro/libctru),
[citro3d](https://github.com/devkitPro/citro3d) and
[citro2d](https://github.com/devkitPro/citro2d), under the zlib licence.
They are not included in this repository.

## Reference material

X-Plane was used only as a visual and performance benchmark
(`tools/inspect_xplane.py` reads its files for reference and exports nothing).
No X-Plane models, textures or data are included.

## Trademarks and disclaimer

Nintendo 3DS is a trademark of Nintendo. Cirrus, SR22 and Vision SF50 are
trademarks of Cirrus Design Corporation; Boeing and 737 of The Boeing Company;
Airbus and A321 of Airbus; F-16 and Fighting Falcon of Lockheed Martin. Liveries
on the models (such as Lion Air) belong to their owners. 3DMicroFlight is an
unofficial fan project, not affiliated with or endorsed by any of them, and the
licences above grant no trademark rights.

3DMicroFlight is a game. It is not a certified flight simulator or training
device, and its maps, airports and flight models must not be used for real
navigation or flight training.
