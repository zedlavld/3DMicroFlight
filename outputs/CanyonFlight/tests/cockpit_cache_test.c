#include "cockpit_cache.h"
#include <string.h>
int checks,failures,firstFailure,counts[2],fixedCounts[2],liveCounts[2];
CockpitFace reference[2][COCKPIT_CAPACITY],optimized[2][COCKPIT_CAPACITY];
static CockpitFace full[COCKPIT_CAPACITY];
int buildReferenceCockpit(const Flight *,CockpitFace *,int);
float clampf(float x,float lo,float hi){return fmaxf(lo,fminf(hi,x));}
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
int main(void){
 for(int type=0;type<2;type++){
  Flight s={0};s.aircraft=type;s.ias=type?170:105;s.pos.z=1800;s.yaw=.8f;
  CockpitCache c={0};CockpitFace *mesh=optimized[type];
  CHECK(updateCockpitCache(&c,&s,mesh,COCKPIT_CAPACITY)==3);
  fixedCounts[type]=c.fixedCount;liveCounts[type]=c.liveCount;counts[type]=c.fixedCount+c.liveCount;
  CHECK(buildReferenceCockpit(&s,reference[type],COCKPIT_CAPACITY)==counts[type]-2);
  CHECK(buildCockpit(&s,full,COCKPIT_CAPACITY)==counts[type]);CHECK(!memcmp(mesh,full,counts[type]*sizeof(*mesh)));
  CHECK(updateCockpitCache(&c,&s,mesh,COCKPIT_CAPACITY)==0);
  s.engine=.7f;s.throttle=.8f;s.wind.x=5;CHECK(updateCockpitCache(&c,&s,mesh,COCKPIT_CAPACITY)==0);
  float *fields[]={&s.roll,&s.pitch,&s.ias,&s.pos.z,&s.yaw,&s.pos.x,&s.pos.y};
  for(int i=0;i<7;i++){
   *fields[i]+=.12f;CHECK(updateCockpitCache(&c,&s,mesh,COCKPIT_CAPACITY)==COCKPIT_LIVE_DIRTY);
   CHECK(!memcmp(mesh,full,c.fixedCount*sizeof(*mesh)));
   int n=buildCockpit(&s,full,COCKPIT_CAPACITY);CHECK(n==c.fixedCount+c.liveCount&&!memcmp(mesh,full,n*sizeof(*mesh)));
  }
  s.airport=2;CHECK(updateCockpitCache(&c,&s,mesh,COCKPIT_CAPACITY)==COCKPIT_LIVE_DIRTY);
  s.aircraft=1-type;CHECK(updateCockpitCache(&c,&s,mesh,COCKPIT_CAPACITY)==3);
  CHECK(updateCockpitCache(&c,&s,mesh,7)==3&&c.fixedCount+c.liveCount==7);
  CHECK(updateCockpitCache(&c,&s,mesh,COCKPIT_CAPACITY)==3);
  /* Restore exported sample for baseline geometry comparison. */
  memset(&s,0,sizeof(s));s.aircraft=type;s.ias=type?170:105;s.pos.z=1800;s.yaw=.8f;
  c.valid=0;updateCockpitCache(&c,&s,mesh,COCKPIT_CAPACITY);
 }
 return failures;
}
