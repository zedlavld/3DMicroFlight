#ifndef TERRAIN_STREAM_H
#define TERRAIN_STREAM_H
/* Statewide 100 m elevation, streamed from SD in 25.6 km tiles (v31).
   Layout must match tools/build_arizona_elevation.py. Each tile stores
   257 x 257 samples (row 0 = south) and shares its edges with neighbours,
   so bilinear lookups never cross into another tile.

   Threading: the render/main thread owns the tile table. Tables change only
   in terrainStreamPublish() (start of a scenery rebuild) or in the blocking
   path of terrainStreamUpdate() when the aircraft's own tile is missing, so
   one 36-part rebuild never mixes two versions of the terrain. */
#include "flight.h"
#define ELEV_SPACING 100.f
#define ELEV_TILE 256
#define ELEV_SAMPLES 257
/* v35: the western states (tools/build_west_v35.py, scenery/elevation35):
   the v32 Arizona + Nevada grid extended 17 tiles west and to 84 x 78 tiles,
   lon -125.25 .. -101.7, lat 31.1 .. 49.1. The v32 tiles are kept, 17
   columns east in this grid. Ocean tiles are absent (500 m bases / procedural). */
#define ELEV_E0 -1228800.f
#define ELEV_N0 -1177600.f /* v35.10: 30 tiles further south (lat 24.26); v35.09 was -409600 */
#define ELEV_TX 209 /* v35.10: whole lower 48, lon -125.25 .. -66.6 */
#define ELEV_TY 110 /* lat 24.26 .. 49.58 */
#define ELEV_FOLDER "sdmc:/3ds/CanyonFlight/scenery/elevation36"
#define ELEV_TILE_METRES (ELEV_TILE*ELEV_SPACING)
#define ELEV_HEADER 16
#define ELEV_PAYLOAD (ELEV_SAMPLES*ELEV_SAMPLES*2)
#define ELEV_SLOTS 40
/* Tiles touching this Chebyshev distance of the aircraft are requested;
   published tiles farther than ELEV_KEEP_RADIUS are released. */
#define ELEV_NEED_RADIUS 45000.f
#define ELEV_KEEP_RADIUS 60000.f
#define ELEV_BLOCK_RADIUS 12000.f

/* Returns 1 and writes *height when (e,n) lies in a published tile. */
int streamedElevation(float e,float n,float *height);
/* Validates a complete tile file image (header, size, indices and CRC). */
int elevationTileValid(const unsigned char *file,unsigned bytes,int i,int j);
unsigned elevationCrc32(const unsigned char *data,unsigned bytes);

/* root: directory holding e_<i>_<j>.bin; NULL uses the SD card path. */
int initTerrainStream(const char *root);
void freeTerrainStream(void);
/* Call once per frame before physics. Returns 1 if the published terrain
   changed (the aircraft's own tile was missing and was loaded at once). */
int terrainStreamUpdate(Vec pos);
/* Adopt completed loads and release distant tiles. Call when a scenery
   rebuild starts. Returns 1 if the published terrain changed. */
int terrainStreamPublish(Vec pos);
/* Completed loads are waiting for terrainStreamPublish(). */
int terrainStreamPending(void);
/* Increments whenever published terrain changes (for derived caches). */
unsigned terrainStreamGeneration(void);
/* v34.04: changes when any tile overlapping the rectangle is published or
   released (or the stream is reset); monotonic per tile. */
unsigned terrainStreamRegionVersion(float e0,float n0,float e1,float n1);
int terrainStreamResident(void);
int terrainStreamFailures(void);
/* v31.15: tiles read on the render thread (nothing resident under the plane). */
int terrainStreamSyncReads(void);
#endif
