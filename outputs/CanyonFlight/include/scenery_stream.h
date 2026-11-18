#ifndef SCENERY_STREAM_H
#define SCENERY_STREAM_H
#include <citro3d.h>
#include "stream_plan.h"
int initSceneryStream(void);
void freeSceneryStream(void);
void updateSceneryStream(Vec pos,Vec velocity,unsigned pinned);
int readySceneryWindow(Vec pos,StreamWindow *window);
unsigned sceneryWindowPins(const StreamWindow *window);
C3D_Tex *sceneryStreamTexture(int slot);
int sceneryStreamBlocked(void);
/* Frames blocked by: [0] GPU budget, [1] no free slot, [2] texture allocation. */
void sceneryStreamBlockCounts(int counts[3]);
/* Pool: [0] fine slots, [1] coarse slots, [2] linear KB free before, [3] after. */
void sceneryStreamPoolInfo(int info[4]);
const char *sceneryStreamStatus(void);
int sceneryStreamDiagnostics(char *out,int size); /* v35.03: 0 when no tile read has failed */
/* v31.15: while the flight waits for scenery, upload whole tiles per frame. */
void sceneryStreamBoost(int on);
/* 1 when the catalog has all four fine tiles of the window at pos. */
int sceneryWindowCanDetail(Vec pos);
/* v32: 1 when the catalog has all four coarse tiles of the window at pos
   (photo scenery is possible there at all). */
int sceneryWindowCatalogued(Vec pos);
#endif
