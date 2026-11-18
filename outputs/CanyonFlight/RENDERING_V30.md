# CirrusFlight v30: runway flicker fix and dramatic terrain

## Runway flicker (hardware report, v28/v29)

A New 3DS XL video of a KGCN takeoff run showed large patches of the runway switching between dark asphalt and light grey every frame, with jagged sawtooth edges. Sky and the lower-screen PFD were stable. This is depth fighting, not the shader-binding issue v29 addressed:

- Inside the flattened airport core, the terrain mesh lay exactly on the runway plane. Asphalt was only 2.5 cm above it, shoulders 1 cm. The light grey is the aerial photograph of the real runway on that terrain.
- PICA vertex arithmetic is float24 (about five significant digits). Scene coordinates were kilometre-scale and the camera translation lived in the modelView matrix, so per-vertex rounding of a centimetre or more changed every frame as the camera moved. With two nearly coplanar surfaces, centimetre errors move their intersection by metres.

Changes:

- Camera-relative vertex transform. The world pass now uploads a rotation-only modelView plus an `eyeOffset` uniform (eye position relative to the scene anchor). The shader subtracts it first, so float24 work is done on camera-relative metres. The cockpit, aircraft and cloud passes set the offset to zero and are otherwise unchanged.
- The rendered ground under airport pavement is lowered 12 cm, fading out over the existing 100 m flattening blend (`sceneryGroundHeight`). Physics, wheel contact, crash checks and camera clearance still use `terrain()`.

## Dramatic terrain

- The sun is lower (30 degrees instead of 39, still from the southwest), with stronger directional shading.
- Baked terrain shadows: each unique terrain vertex marches eight samples, up to 3 km, toward the sun through the elevation grids. Canyon walls, buttes and ridges now shade the ground behind them. These are per-vertex, so shadow edges follow mesh resolution.
- Illustrative Grand Canyon stratigraphy on exposed slopes: Kaibab rim, Coconino cream band, Hermit/Supai reds, Redwall, Tonto Platform, Tapeats and Vishnu schist. Layers rise gently toward the North Rim. Forested rim plateaus remain vegetated. These are artistic approximations, not a geologic map.
- Sedona red rock: Schnebly Hill red-orange slopes, red soil and buff Coconino caps, fading out toward the Mogollon Rim forest.
- Distance haze starts at 12 km instead of 8 km and builds more slowly.

Inside the NAIP photo corridor the aerial imagery still supplies 65% of the colour, so the procedural palette mostly shows outside the photo window, for example across the canyon toward the North Rim. The shading and shadows apply everywhere.

### Terrain mesh: measured, not changed

Error-ranked sub-grid refinement (2x2 and 4x4 elevation samples per coarse cell) was prototyped and measured against the elevation data. Over the inner Grand Canyon, 99th-percentile height error at 4-8 km was 163 m in v29, 152-147 m with refinement, at a cost of about 23k extra vertices (+48%). That trade was rejected. Each LOD ring's cells are about 1/16 of their distance, and the non-indexed triangle-list mesh cannot afford a finer base grid. The next real step is a more compact mesh (strips or indexed vertices), paired with finer canyon elevation data streamed from SD.

`terrain()` now evaluates each runway heading's sine/cosine once instead of on every call.

## Validation

- ARM terrain stability test: 907,090 checks, 0 failures (boundary checks now use the rendered ground height).
- ARM airport playtest: 60,242 checks, 0 failures; takeoff results unchanged.
- Changed sources syntax-checked with arm-none-eabi-gcc against libctru/citro3d/citro2d headers.
- Desktop mesh previews (no aerial photos) compared v29 and v30 views of Sedona and the Grand Canyon: validation/v30_desktop_terrain_preview.png (left v29, right v30).
- Scenery build CPU cost is about 1.45x v29 on a desktop host, spread across the incremental per-frame build. Hardware frame time is not yet measured.

Not verified on New 3DS hardware. Please re-run the KGCN takeoff from the video, with the 3D slider off and on, and capture a ZL+X frame profile while cruising.

## Azahar runtime pass (2026-11-03)

Full devkitPro build, including picasso assembly of the modified `aircraft_shader.v.pica`: no errors or warnings. Azahar 2126.2, New 3DS mode, 100% CPU clock, native resolution, 3D slider off. Emulation speed stayed at 100%. Captures and profiles are in `validation/azahar_v30/`.

- **KGCN runway flicker: not reproduced.** Exterior view, from rest through a full-throttle roll to 74 KIAS (`sheet_takeoff_roll.png`, 15 frames at ~0.67 s intervals). The asphalt stays uniformly dark, with no grey photo-runway patches and no sawtooth edges. Azahar (hardware shaders on) evaluates PICA vertex shaders at host float32 precision rather than float24, so this shows the change renders correctly but cannot prove the hardware fix. The New 3DS check is still needed.
- **Takeoff with keyboard input:** two attempts ended in "AIRFRAME / HARD IMPACT" at rotation. Digital back-stick pitched the nose beyond the 0.32 rad wheel-contact limit (`ground.c`) before the mains left the ground. This is a tail strike caused by binary input, not a v30 change: the check uses `terrain()`, which v30 did not modify.
- **Terrain look:** the Grand Canyon (`sheet_kgcn_cruise_to_rim.png`, `canyon_rim_closeup.png`) shows the strata palette (Supai/Hermit reds, cream Coconino, grey Redwall), darker baked shadows on walls facing away from the sun, and green rim forest, matching the right-hand column of `v30_desktop_terrain_preview.png`. Sedona (`sheet_ksez_cruise.png`) shows red-orange slopes and soil below the Mogollon Rim forest. Inside the NAIP corridor the aerial photo still dominates near-field colour, as expected. Per-vertex shadows show triangle-shaped edges at coarse LODs.
- **Frame rate (ZL+X, exterior, SR22 CRUISE stage, ~72 s in):** v30 was CPU-bound and slower than v29.

| Run | FPS | Mean | p95 | Max | CPU mean |
|---|---|---|---|---|---|
| KSEZ v29 baseline (dde1fe9~1) | 17.8 | 56.3 ms | 94 ms | 98 ms | 48.4 ms |
| KSEZ v30 as committed | 14.4 | 69.3 ms | 102 ms | 117 ms | 61.3 ms |
| KSEZ v30 + shadow cache | 16.3 | 61.5 ms | 96 ms | 102 ms | 52.8 ms |
| KGCN South Rim v30 as committed | 18.5 | 54.0 ms | 99 ms | 105 ms | 45.2 ms |

### Shadow cache

A host benchmark (all 32 terrain parts, rebuilt every 256 m as the game does) showed that the shadow march accounted for the entire v30 slowdown: 31.2 ms vs 21.6 ms per rebuild with shadows stubbed out at KSEZ, against 20.8 ms for v29. Shadows depend only on vertex position (fixed sun, static elevation grids), and the LOD rings are world-snapped, so `cachedTerrainShadow()` now keeps results across rebuilds in a 512 KB, 4-way set-associative table keyed on exact x/y/z. Host rebuild time fell to 23.4 ms at KSEZ and the KGCN rim, and to 21.9 ms at the KGCN airport. Colour checksums match the uncached build exactly, so the image is unchanged. On Azahar, Sedona cruise rose from 14.4 to 16.3 FPS.
