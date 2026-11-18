#include "cockpit_cache.h"
#include <string.h>
int updateCockpitCache(CockpitCache *c,const Flight *s,CockpitFace *faces,int capacity){
 if(!c||!s||!faces||capacity<=0)return 0;
 CockpitKey key={s->roll,s->pitch,s->ias,s->pos.z,s->yaw,s->pos.x,s->pos.y,s->aircraft,s->airport,s->throttle,s->engine};
 int fixed=!c->valid||c->key.aircraft!=s->aircraft||c->capacity!=capacity;
 if(!fixed&&!memcmp(&key,&c->key,sizeof(key)))return 0;
 if(fixed)c->fixedCount=buildCockpitStatic(s->aircraft,faces,capacity);
 c->liveCount=buildCockpitDynamic(s,faces+c->fixedCount,capacity-c->fixedCount);
 c->key=key;c->valid=1;c->capacity=capacity;
 return COCKPIT_LIVE_DIRTY|(fixed?COCKPIT_FIXED_DIRTY:0);
}
