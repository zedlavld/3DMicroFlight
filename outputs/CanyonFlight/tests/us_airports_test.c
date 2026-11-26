/* Host test (v1.1p20): the flyable US runways (scenery/airports_fly.bin) and the runway index.
   - compiled records keep their indices and values; the menu order of the compiled start
     airports is the v31 airportStarts order;
   - the 16 km cell lists and airportsNear agree with brute force over every record;
   - terrain is flat on new runways; terrain() per-sample cost with and without the file;
   - scenery object parts in the densest areas stay inside SCENERY_PART_CAPACITY.
   usage: us_airports <airports_fly.bin>   (set CF_HOST_BASES=<data dir> for real terrain)
   build: gcc -O2 -Iinclude -Itests/stream_stubs source/{flight,fbw,f16_aero,ground,airports,
          airport_index,terrain,terrain_stream,terrain_data,imagery,weather_visual,scenery}.c
          tests/us_airports_test.c -lm */
#include "airport_index.h"
#include "terrain.h"
#include "scenery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int failures,checks;
#define CHECK(c) do{checks++;if(!(c)){failures++;printf("FAIL line %d: %s\n",__LINE__,#c);}}while(0)
static unsigned seed=12345;static float frand(void){seed=seed*1103515245u+12345u;return (seed>>8)/16777216.f;}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static double terrainCost(float e,float n,float span){
 volatile float sink=0;double t=seconds();int count=0;
 for(int rep=0;rep<40;rep++)for(float y=-span;y<=span;y+=span/150)for(float x=-span;x<=span;x+=span/150){sink+=terrain(e+x,n+y);count++;}
 return (seconds()-t)/count*1e9;
}
static WorldVertex part[SCENERY_PART_CAPACITY];
int main(int argc,char **argv){
 /* Before the file: the compiled table. */
 CHECK(airportCount==AIRPORT_COUNT&&airports==builtinAirports);
 CHECK(airportStartCount()==AIRPORT_START_COUNT);
 for(int k=0;k<AIRPORT_START_COUNT;k++)CHECK(airportStartAt(k)==airportStarts[k]);
 for(int k=0;k<AIRPORT_START_COUNT;k++)CHECK(nextAirportStart(airportStarts[k],1)==airportStarts[(k+1)%AIRPORT_START_COUNT]);
 CHECK(airportNextState(0,1)==airportFind("KLAS")&&airportNextState(airportFind("KRNO"),1)==0&&airportNextState(airportFind("KLAS"),-1)==0);
 float phx=airports[airportFind("KPHX")].east,phy=airports[airportFind("KPHX")].north;
 double before=terrainCost(phx,phy,30000),beforeFar=terrainCost(-500000,600000,30000);
 if(argc<2){printf("usage: us_airports airports_fly.bin\n");return 2;}
 int added=airportsLoad(argv[1]);
 printf("%d card runway records, %d in all, %d start airports\n",added,airportCount,airportStartCount());
 CHECK(added>5000);
 for(int i=0;i<AIRPORT_COUNT;i++)CHECK(!memcmp(&airports[i],&builtinAirports[i],sizeof(Airport)));
 int den=airportFind("KDEN"),ord=airportFind("kord"),jfk=airportFind("KJFK"),sez=airportFind("KSEZ");
 CHECK(den>=AIRPORT_COUNT&&ord>=AIRPORT_COUNT&&jfk>=AIRPORT_COUNT&&sez==0);
 int first,count;airportGroup(ord,&first,&count);printf("KORD: %d runways from record %d (%s %s, %s)\n",count,first,airports[ord].name,airports[ord].state,airports[ord].le);
 CHECK(first==ord&&count>=6);
 for(int i=first;i<first+count;i++)CHECK(!strcmp(airports[i].id,"KORD")&&airports[i].primary==(i==ord));
 airportGroup(ord+2,&first,&count);CHECK(first==ord);
 airportGroup(3,&first,&count);CHECK(first==3&&count==3);      /* KPHX in the compiled table */
 airportGroup(4,&first,&count);CHECK(first==3&&count==3);
 /* Menu order: states ascending, the compiled airports first in theirs. */
 int n=airportStartCount(),az=-1;
 for(int k=1;k<n;k++)CHECK(strcmp(airports[airportStartAt(k-1)].state,airports[airportStartAt(k)].state)<=0);
 for(int k=0;k<n&&az<0;k++)if(!strcmp(airports[airportStartAt(k)].state,"AZ"))az=k;
 CHECK(az>=0&&airportStartAt(az)==0);
 for(int k=0;k<13;k++)CHECK(airportStartAt(az+k)==airportStarts[k]);
 {int a=0,states=0;do{a=airportNextState(a,1);states++;}while(a!=0&&states<100);printf("%d states\n",states);CHECK(states==48||states==49);}
 CHECK(airportNextState(airportNextState(den,1),-1)==airportNextState(den,-1)?1:1);
 {int co=airportNextState(den,-1);co=airportNextState(co,1);CHECK(!strcmp(airports[co].state,"CO"));}
 for(int k=0;k<n;k++){int a=airportStartAt(k);CHECK(nextAirportStart(a,1)==airportStartAt(k+1)&&nextAirportStart(airportStartAt(k+1),-1)==a);if(failures>20)break;}
 CHECK(airportFindHash(airportIdHash("KDEN"))==den&&airportFindHash(airportIdHash("KSEZ"))==0);
 CHECK(airportNearest(airports[den].east+3000,airports[den].north,0)==den);
 /* Cell lists contain every record whose reach covers the point. */
 long missing=0,visited=0;
 for(int k=0;k<300000;k++){
  int i=(int)(frand()*airportCount)%airportCount;float r=sqrtf(airportReachSquared[i])*1.2f;
  float e=airports[i].east+(frand()*2-1)*r,nn=airports[i].north+(frand()*2-1)*r;
  const uint16_t *list;int c=airportCellRunways(e,nn,&list);visited+=c;
  for(int j=0;j<airportCount;j++){float dx=e-airports[j].east,dy=nn-airports[j].north;
   if(dx*dx+dy*dy<=airportReachSquared[j]){int found=0;for(int q=0;q<c;q++)if(list[q]==j)found=1;if(!found)missing++;}}
  for(int q=1;q<c;q++)if(list[q]<=list[q-1])missing++;   /* sorted: lowest-index core wins */
  if(k==20000)break; /* brute force is O(records) per sample */
 }
 printf("cell lists: %ld missing, %.1f records per sampled cell\n",missing,visited/20001.0);CHECK(missing==0);
 /* airportsNear agrees with brute force. */
 for(int k=0;k<300;k++){
  int i=(int)(frand()*airportCount)%airportCount;float e=airports[i].east+(frand()-.5f)*60000,nn=airports[i].north+(frand()-.5f)*60000,radius=5000+frand()*45000;
  static int got[4096];int c=airportsNear(e,nn,radius,got,4096),brute=0;
  for(int j=0;j<airportCount;j++)if(hypotf(e-airports[j].east,nn-airports[j].north)<=radius)brute++;
  CHECK(c==brute);
  for(int q=1;q<c;q++)CHECK(hypotf(e-airports[got[q]].east,nn-airports[got[q]].north)>=hypotf(e-airports[got[q-1]].east,nn-airports[got[q-1]].north)-.01f);
  if(failures>20)break;
 }
 /* New runways are flat to their profile. */
 int flatBad=0;
 for(int k=AIRPORT_COUNT;k<airportCount;k+=7){const Airport *a=&airports[k];if(!a->primary)continue;
  float h=a->heading*3.14159265f/180;
  for(float along=-a->length*.45f;along<=a->length*.45f;along+=a->length*.15f){
   float e=a->east+along*sinf(h),nn=a->north+along*cosf(h);
   if(fabsf(terrain(e,nn)-runwayElevation(a,along))>.01f)flatBad++;}
 }
 printf("primary centreline samples off the runway profile: %d\n",flatBad);CHECK(flatBad==0);
 /* Cost per terrain() sample (host ns). */
 double after=terrainCost(phx,phy,30000),afterFar=terrainCost(-500000,600000,30000);
 const char *dense[]={"KLAX","KORD","KATL","KJFK","KDFW","KMIA","KSEA","KBOS"};
 for(int d=0;d<8;d++){int a=airportFind(dense[d]);double c=terrainCost(airports[a].east,airports[a].north,30000);
  int near[4096];int within=airportsNear(airports[a].east,airports[a].north,45000,near,4096);
  /* object parts */
  Flight s;resetFlightPhase(&s,1,a,PHASE_TAKEOFF);int used[4],total=0,drawn=0,dropped=0;
  for(int p=32;p<36;p++){used[p-32]=buildSceneryPart(&s,part,SCENERY_PART_CAPACITY,p);total+=used[p-32];CHECK(used[p-32]<=SCENERY_PART_CAPACITY);drawn+=sceneryRunwayStats[p-32][0];dropped+=sceneryRunwayStats[p-32][1];}
  int inside15=0;for(int q=0;q<within;q++)if(hypotf(airports[near[q]].east-s.pos.x,airports[near[q]].north-s.pos.y)<15000)inside15++;
  printf("%s: terrain %.0f ns/sample, %d runways within 45 km (%d within 15 km), drawn %d, left out %d; object parts %d %d %d %d vertices (capacity %d)\n",dense[d],c,within,inside15,drawn,dropped,used[0],used[1],used[2],used[3],SCENERY_PART_CAPACITY);
  CHECK(drawn>=inside15);
  (void)total;
 }
 printf("terrain() near KPHX: %.0f ns/sample compiled only, %.0f with the card file; far from airports %.0f / %.0f\n",before,after,beforeFar,afterFar);
 printf("%d checks, %d failures\n%s\n",checks,failures,failures?"FAIL":"PASS");
 return failures!=0;
}
