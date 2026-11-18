#ifndef COCKPIT_CACHE_H
#define COCKPIT_CACHE_H
#include "cockpit.h"
enum {COCKPIT_FIXED_DIRTY=1,COCKPIT_LIVE_DIRTY=2};
typedef struct {float roll,pitch,ias,altitude,heading,east,north;int aircraft,airport;float throttle,engine;} CockpitKey;
typedef struct {int valid,capacity,fixedCount,liveCount;CockpitKey key;} CockpitCache;
/* Faces remain owned by the caller and must survive between updates. A zero
   result permits reuse of the current GPU buffer, including across both eyes. */
int updateCockpitCache(CockpitCache *cache,const Flight *s,CockpitFace *faces,int capacity);
#endif
