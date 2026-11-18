#ifndef TERRAIN_DATA_H
#define TERRAIN_DATA_H
#include <stdint.h>
typedef struct {float east,north,spacing;int width,height;const int16_t *data;} ElevationGrid;
/* [0] Arizona 500 m base, [1..3] Sedona/Flagstaff/Canyon detail,
   [4] Nevada 500 m base (v32). */
#define ELEVATION_GRID_COUNT 5
extern const ElevationGrid elevationGrids[ELEVATION_GRID_COUNT];
#endif