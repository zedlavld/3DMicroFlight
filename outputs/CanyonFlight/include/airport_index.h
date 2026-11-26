#ifndef AIRPORT_INDEX_H
#define AIRPORT_INDEX_H
/* v1.1p20: every flyable lower-48 runway, and the spatial index over all runway records.
   airportsLoad appends the card's runways (tools/build_us_runways.py: large and medium
   airports and small ones with 3000 ft+ of pavement, ~5,300 runways at ~3,900 airports) to
   the 49 compiled records, which keep their indices. The index is a 16 km grid: each cell
   lists the records whose flattening reach (or scenery-scatter keep-out) touches it, so a
   terrain sample visits only the runways that can affect it. Without the file (host tests,
   a card without it) the index covers the compiled records only. No 3DS dependencies. */
#include <stdint.h>
#include "flight.h"
#define AIRPORTS_FLY_PATH "sdmc:/3ds/CanyonFlight/scenery/airports_fly.bin"
#define AIRPORT_INDEX_CELL 16000.f
/* Card records added (0: file missing or invalid; airports[] is then the compiled table). */
int airportsLoad(const char *path);
/* Records whose reach box covers the grid cell of (e, n); the list stays valid until the
   next airportsLoad. */
int airportCellRunways(float east,float north,const uint16_t **list);
/* Per-record heading sine/cosine and squared airportReach(), for terrain sampling. */
extern const float *airportAxisSin,*airportAxisCos,*airportReachSquared;
/* Records whose centre is within radius, nearest first; returns how many were written
   (at most max). */
int airportsNear(float east,float north,float radius,int *out,int max);
/* Records whose flattening reach overlaps the box, each once (at most max). */
int airportsInBox(float e0,float n0,float e1,float n1,uint16_t *out,int max);
/* The records of i's airport: contiguous, primary first. */
void airportGroup(int i,int *first,int *count);
/* Primary record of an airport by identifier (case-insensitive), -1 if none. */
int airportFind(const char *id);
/* Nearest primary record to a point, skipping those within skipRadius metres; -1 if none. */
int airportNearest(float east,float north,float skipRadius);
/* The menu order of start airports (all primary records). */
int airportStartCount(void);
int airportStartAt(int k);
/* First start airport of the next (dir > 0) or previous state, wrapping. */
int airportNextState(int ap,int dir);
/* Stable identifier hash for saves (never 0). */
uint32_t airportIdHash(const char *id);
/* Primary record whose id hashes to h, -1 if none. */
int airportFindHash(uint32_t h);
#endif
