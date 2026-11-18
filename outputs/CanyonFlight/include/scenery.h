#ifndef SCENERY_H
#define SCENERY_H
#include "flight.h"
#define WORLD_VERTEX_CAPACITY 66000

#define SCENERY_PARTS 36
#define SCENERY_PART_CAPACITY 6000
typedef struct {Vec p;float color[3];} WorldVertex;
int buildSceneryPart(const Flight *s,WorldVertex *vertices,int capacity,int part);
/* v34.04: buildSceneryPart, reusing a terrain part whose ring and elevation
   are unchanged (haze re-applied for the new viewer). */
int buildSceneryPartCached(const Flight *s,WorldVertex *vertices,int capacity,int part);
void sceneryPartCacheStats(int *hits,int *misses);
void freeSceneryPartCache(void);
/* Material for a vertex of the most recently built part: -2 asphalt, -1 color. */
int sceneryVertexMaterial(int index);
/* Unfogged RGB from elevation and east/north height gradients. */
void scenerySurfaceColor(float height,float eastGradient,float northGradient,float rgb[3]);
void sceneryGroundColorAt(float east,float north,float rgb[3]);
int sceneryGroundMapLoaded(void); /* v35.14: 1 when scenery/ground_us4000.bin was read from the card */
float sceneryHaze(float distance);
/* Rendered ground height: terrain() lowered slightly beneath airport pavement. */
float sceneryGroundHeight(float east,float north);
void scenerySkyColor(float elevationSine,float rgb[3]);
/* World-aligned nested rings; the inner hole follows the finer ring exactly. */
void sceneryLodLayout(Vec position,int level,float *east,float *north,int *innerX,int *innerY);
int buildScenery(const Flight *s,WorldVertex *vertices,int capacity);
#endif
