#ifndef IMAGERY_H
#define IMAGERY_H
#include "scenery.h"
#define IMAGERY_REGIONS 24
#define IMAGERY_FINE_REGION 20
#define IMAGERY_TILES 8
#define IMAGERY_TEXTURE_SIZE 1024
#define IMAGERY_MAX_LEVEL 7
#define IMAGERY_TILE_SPAN 2048.f
#define IMAGERY_MIN -8192.f
/* Four exterior regions, sixteen coarse-ring rectangles, four detailed tiles. */
Vec imageryRegionOrigin(int region);
const char *imageryRegionName(int region);
int chooseImageryRegion(Vec position,int current,int airport,int previousAirport);
void setImageryOrigin(Vec origin);
void setImageryWindows(Vec fine,Vec coarse);
void setImageryFineEnabled(int enabled);
Vec getImageryCoarseOrigin(void);
int imageryCoarseTile(int fineTile);
Vec getImageryOrigin(void);
int imageryMaterial(int region);
void imageryCoarseTransform(int tile,float uv[4]);
void imageryMaterialRegions(int material,int *first,int *end);
typedef struct {Vec p;float uv[2],color[4];} ImageryVertex;
int clipImageryTriangle(const WorldVertex input[3],int region,ImageryVertex *out,int capacity,Vec viewer);
/* v31.15: the rectangle clipImageryTriangle keeps for region: rect = left,
   right, bottom, top. 0 when the region keeps nothing (a triangle outside
   the rectangle also yields nothing). */
int imageryRegionRect(int region,float rect[4]);
#endif
