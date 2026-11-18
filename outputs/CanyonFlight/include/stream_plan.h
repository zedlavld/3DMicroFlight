#ifndef STREAM_PLAN_H
#define STREAM_PLAN_H
#include "flight.h"
#define STREAM_SLOTS 24
/* v31.13: slots 0..5 hold fine tiles (1024 px), 6..11 coarse (512 px). Each
   slot keeps its texture for the whole session, so tiles of two sizes are
   never freed and reallocated (that fragmented the linear heap on long
   flights until no 2.8 MB block was left). 6x2796160+6x699008 bytes is
   just under STREAM_GPU_BUDGET. */
/* v32: tiles are ETC1 (4 bits/pixel, a quarter of RGB565): fine 699,040
   and coarse 174,752 bytes. 10 fine + 14 coarse slots = 9.4 MB, under half
   the v31 pool, with twice the tiles. Pins are a 32-bit mask (<=32 slots). */
#define STREAM_FINE_SLOTS 10
#define STREAM_GPU_BUDGET (10u*1024u*1024u)
#define STREAM_UPLOAD_BUDGET 65536u
typedef struct {int tier,x,y;} StreamKey;
typedef struct {Vec fine,coarse;StreamKey keys[8];int slots[8];int detailed;} StreamWindow;
void streamWindow(Vec pos,StreamWindow *w);
int streamKeyEqual(StreamKey a,StreamKey b);
int streamCatalogHas(const unsigned char *data,unsigned bytes,StreamKey key);
unsigned streamTileBytes(StreamKey key);
int streamHeaderValid(const unsigned char *header,unsigned bytes,StreamKey key);
#endif
