# CirrusFlight v22 — cockpit geometry cache

V21 rebuilt every cockpit triangle, converted it into GPU vertices and explicitly flushed those vertices once per rendered frame. Most of those triangles describe fixed cabin surfaces, controls and bezels. V22 separates fixed geometry from live instrument geometry, without reducing detail or instrument update cadence.

The fixed prefix is generated and uploaded to both existing vertex buffers when the aircraft changes. Later frames rebuild and update only the live suffix. A key covering every current instrument input skips all cockpit geometry work and explicit cockpit-buffer updates when those values are unchanged. Camera movement is independent of that key: looking around a paused cockpit reuses the mesh. Both stereo eyes share the same preparation. Changing aircraft or cache capacity invalidates the fixed geometry; loading or restarting a flight refreshes instruments when their values differ.

## Measured workload

Compiled ARM tests compare the published v21 source with the optimized path. For the representative instrument values used in the cockpit preview:

| Aircraft | Total triangles | Cached fixed | Live | V21 vertex bytes updated/frame | V22 changing frame | Reduction |
|---|---:|---:|---:|---:|---:|---:|
| SR22 | 785 | 588 | 197 | 84,780 | 21,276 | 74.9% |
| SF50 | 699 | 542 | 157 | 75,492 | 16,956 | 77.5% |

An unchanged frame rebuilds no cockpit geometry and performs no explicit cockpit VBO flush. On aircraft selection there is a one-time fixed-prefix upload to both buffers (127,008 / 117,072 bytes), plus the live suffix. Those uploads are outside the scenery loader's separate 64-KiB-per-frame budget. Existing vertex buffer capacity, draw triangle counts, depth pass and stereo remain unchanged. Exact live counts vary with displayed digits and attitude.

These are measured vertex-write/explicit-flush byte counts, not total GPU traffic, CPU milliseconds or FPS. Citro3D's existing `C3D_FrameEnd(0)` still flushes the linear heap. The [Citro3D frame-end implementation](https://github.com/devkitPro/citro3d/blob/master/source/renderqueue.c) and [Citro2D batching implementation](https://github.com/devkitPro/citro2d/blob/master/source/base.c) were checked: Citro2D's vertex/index buffers rely on that flush. This release retains it rather than enabling the command-list-only flush mode without a replacement 2D buffer flush path. The cache still avoids fixed-geometry generation, conversion, writes and explicit range flushes.

## Verification

- 62 cache tests passed on compiled ARM, including every current key field, ignored nonvisual fields, repeated unchanged states, aircraft/airport changes, capacity invalidation and agreement with uncached output.
- Representative triangle multisets and colors match the published v21 mesh to 1 micrometre. Triangle ordering changes to group fixed and live geometry; the normal GPU path uses depth testing. The CPU fallback retains full uncached generation.
- 60,198 cockpit/camera checks passed. Recorder, menu/save and layout checks passed. Flight dynamics and scenery code are unchanged.
- `tools/test_cockpit_cache.py` reproduces the comparison against `tests/references/cockpit_v21.c`; the reference source hash is recorded in `validation/optimization_report.json`.
- The app compiles with devkitARM. GPU synchronization and visual behavior remain to be verified in Azahar/on hardware. No frame-rate improvement is claimed from numerical byte-count tests alone.

## Runtime comparison

Use the same saved flight, aircraft, weather, camera pose and stereo setting in v21 and v22. Allow scenery to settle, compare flying and paused looking-around cases, then repeat for the other aircraft. Test stereo off and full depth separately. Y pauses with the PFD still visible: the upper diagnostics show CPU/GPU milliseconds, plus fixed/live cockpit triangle counts and explicit cockpit bytes updated in that frame. The paused, settled case should report zero cockpit update KiB; the first pause frame may still update the final instrument state. Use Y or the menu to resume.

Install the update's `sdmc/` contents over the existing app. Scenery and all three save slots remain compatible. CSVs identify model version 22.
