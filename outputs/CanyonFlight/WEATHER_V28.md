# CirrusFlight v28: scenery and lightweight weather

Choose Weather on the flight setup menu, or tap WEATHER on the lower screen:

- Clear / calm: blue-to-pale sky gradient and clear nearby terrain.
- Clouds / breeze: sparse drifting cloud puffs with the existing breeze profile.
- Storm / gusts: taller dark cloud clusters, dimmer ground and sky, light rain streaks, occasional two-part lightning flashes, and delayed synthesized thunder. Uses the existing gust profile.

Haze is deliberately mild: it starts beyond 8 km and grows gradually. Clouds, rather than heavy haze, provide most atmospheric definition. Rain decreases above 2,900 m MSL and stops above 3,300 m MSL in this first implementation. Weather animation follows simulation time and freezes when paused. Audio follows the existing volume/mute settings and pauses with flight audio.

## Performance limits

At most 27 soft procedural cloud puffs (216 triangles), 36 rain streaks, and 48 sky-gradient triangles per eye. Clouds use one additional draw call per eye and approximately 46 KiB of double-buffered vertex storage. There are no additional cloud textures, scenery downloads, particle allocations, or per-vertex trigonometric calls. Cloud blending reads scene depth without writing it; cloud puffs are sorted back-to-front, and cockpit geometry is drawn over outside precipitation.

This is a lightweight weather prototype: no volumetric cloud simulation, cast cloud shadows, real weather feed, storm radar, lightning strikes/damage, icing, or convective updraft/downdraft model. Rain coverage and cloud bases are simplified. Wind direction/strength remain the established illustrative breeze/gust settings. Saved weather values remain compatible: old Breeze saves now show scattered clouds, and old Gusts saves now show storm visuals.

## Validation

SDK build passed without warnings. ARM tests passed: 907,090 terrain/weather checks (including cloud capacity, opacity, rain altitude cutoff, lightning/thunder timing and terrain seams), 6,680 synthesizer checks (including weather output, mute and combined sound headroom), and 11 recorder/version checks. Current weather visuals, NDSP playback and hardware FPS are NOT yet runtime-verified: Azahar launched, but screen capture showed the Windows background instead of its rendered window. A visual playtest is still required before judging cloud quality or frame-rate impact.

## Install update

Back up saves, then merge the ZIP's sdmc contents into the existing SD-card root. Keep /3ds/CanyonFlight/scenery/ as installed. No scenery re-download is needed. Three save slots and earlier scenery fixes remain included. The update contains no test saves.
