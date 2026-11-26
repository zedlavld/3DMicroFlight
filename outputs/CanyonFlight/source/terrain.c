#include "terrain.h"
#include <stdlib.h>
#include "terrain_data.h"
#include "terrain_stream.h"
#include "airport_index.h"
#ifdef __3DS__
#include "elevation_west2000_bin.h"
#include "elevation_us4000_bin.h"
#define WEST_BASE_DATA ((const int16_t*)elevation_west2000_bin)
#define US_BASE_DATA ((const int16_t*)elevation_us4000_bin)
#define HOST_BASES 1
#else
/* host test builds: no embedded data, the bases are absent; v1.1p20: unless CF_HOST_BASES names
   the data folder (elevation_west2000.bin, elevation_us4000.bin), for routes outside Arizona. */
#include <stdio.h>
#include <stdlib.h>
static int16_t westHost[1076*999],usHost[1338*705];
static int hostBases(void){
 static int state;if(state)return state>0;
 state=-1;const char *dir=getenv("CF_HOST_BASES");if(!dir)return 0;
 char path[512];FILE *f;int ok=1;
 snprintf(path,sizeof(path),"%s/elevation_west2000.bin",dir);f=fopen(path,"rb");ok=f&&fread(westHost,sizeof(westHost),1,f)==1;if(f)fclose(f);
 snprintf(path,sizeof(path),"%s/elevation_us4000.bin",dir);f=fopen(path,"rb");ok=ok&&f&&fread(usHost,sizeof(usHost),1,f)==1;if(f)fclose(f);
 state=ok?1:-1;return ok;
}
#define WEST_BASE_DATA westHost
#define US_BASE_DATA usHost
#define HOST_BASES hostBases()
#endif
#define PI 3.14159265359f
static float sample(const ElevationGrid *g,float e,float n){
 float x=clampf((e-g->east)/g->spacing,0,g->width-1.001f),y=clampf((n-g->north)/g->spacing,0,g->height-1.001f);
 int ix=(int)x,iy=(int)y;float fx=x-ix,fy=y-iy;const int16_t *p=g->data+iy*g->width+ix;
 return ((1-fx)*p[0]+fx*p[1])*(1-fy)+((1-fx)*p[g->width]+fx*p[g->width+1])*fy;
}
static int gridContains(const ElevationGrid *g,float e,float n){return e>=g->east&&n>=g->north&&e<=g->east+(g->width-1)*g->spacing&&n<=g->north+(g->height-1)*g->spacing;}
/* v35: 2 km base over the whole western grid (tools/build_west_v35.py base),
   for the far terrain outside the Arizona and Nevada 500 m bases. */
static const ElevationGrid westBase={ELEV_E0,-409600.f,2000,1076,999,WEST_BASE_DATA}; /* the v35.09 84 x 78 tile grid */
static int westContains(float e,float n){return westBase.data&&HOST_BASES&&gridContains(&westBase,e,n);}
/* v35.10: 4 km base over the whole lower-48 grid (tools/build_west_v35.py usbase), outside the West base. */
static const ElevationGrid usBase={ELEV_E0,ELEV_N0,4000,(int)(ELEV_TX*ELEV_TILE_METRES/4000)+1,(int)(ELEV_TY*ELEV_TILE_METRES/4000)+1,US_BASE_DATA}; /* 1338 x 705 */
static int usContains(float e,float n){return usBase.data&&HOST_BASES&&gridContains(&usBase,e,n);}
int terrainContains(float e,float n){return gridContains(&elevationGrids[0],e,n)||gridContains(&elevationGrids[4],e,n)||westContains(e,n)||usContains(e,n);}
/* v32: Arizona base inside its rectangle (unchanged), else the Nevada base;
   both are decimated from the same 100 m data on aligned 500 m lattices.
   v35: else the 2 km West base; v35.10: else the 4 km US base. */
static const ElevationGrid *baseGrid(float e,float n){
 if(gridContains(&elevationGrids[0],e,n))return &elevationGrids[0];
 if(gridContains(&elevationGrids[4],e,n))return &elevationGrids[4];
 if(westContains(e,n))return &westBase;
 return usContains(e,n)?&usBase:&elevationGrids[0];
}
/* Statewide: 100 m streamed tiles where published, else the embedded 500 m
   base (v31). The finer embedded Sedona/Flagstaff/Canyon grids still blend
   in on top over their 1.2 km edge bands. */
float rawElevation(float e,float n){
 float h;if(!streamedElevation(e,n,&h))h=sample(baseGrid(e,n),e,n);
 for(int i=1;i<4;i++){
  const ElevationGrid *g=&elevationGrids[i];float dx=fminf(e-g->east,g->east+(g->width-1)*g->spacing-e),dy=fminf(n-g->north,g->north+(g->height-1)*g->spacing-n);
  float t=clampf(fminf(dx,dy)/1200,0,1);t=t*t*(3-2*t);if(t>0)h=h*(1-t)+sample(g,e,n)*t;
 }
 return h;
}
float runwayElevation(const Airport *ap,float along){float t=clampf(along/ap->length+.5f,0,1);return ap->lowElevation+(ap->highElevation-ap->lowElevation)*t;}
/* Runway axes are constant. terrain() runs thousands of times per scenery job
   on the ARM11, so each heading's sine/cosine is evaluated once (v1.1p20: by the
   runway index, include/airport_index.h, which also limits a sample to the
   records registered in its 16 km cell). */
/* 1 outside the flattened area. Cheap reject first (v31). */
static float airportBlend(int i,float e,float n,float *along){
 const Airport *ap=&airports[i];float dx=e-ap->east,dy=n-ap->north;
 if(dx*dx+dy*dy>airportReachSquared[i]){*along=0;return 1;}
 float si=airportAxisSin[i],co=airportAxisCos[i];
 float a=dx*si+dy*co,across=dx*co-dy*si;
 float side=fabsf(across)-ap->width/2-65,end=fabsf(a)-ap->length/2-30;
 /* Union the runway and apron rectangles. Replacing the runway distance
    with the apron distance created an abrupt terrain step along the runway
    at the apron ends (and false wing/tire impacts during the takeoff roll).
    Only primary runways have an apron, on their apronSide. */
 float runwayDistance=fmaxf(side,end);
 float apronAcross=across*(float)-ap->apronSide;
 float apronDistance=ap->primary&&ap->apronSide?fmaxf(fmaxf(apronAcross+25,-250-apronAcross),fmaxf(-650-a,a-250)):1e9f;
 float distance=fminf(runwayDistance,apronDistance),blend=clampf(distance/100,0,1);
 *along=a;return blend*blend*(3-2*blend);
}
float airportFlattening(float e,float n){
 float flat=0,along;const uint16_t *list;int count=airportCellRunways(e,n,&list);
 for(int k=0;k<count;k++)flat=fmaxf(flat,1-airportBlend(list[k],e,n,&along));
 return flat;
}
/* v31: several runways can overlap (crossing or close parallels). Inside a
   runway core the lowest-index record wins; records list each airport's
   primary first. Elsewhere overlapping blend zones mix with weights that grow
   without bound towards a core, so the surface stays continuous. For a single
   runway this is exactly h*blend + runway*(1-blend), as before. */
/* v34.04: a scenery part may limit the runway loop to the records whose
   flattening reach overlaps it (terrainLimitAirports); -1 = the sample's grid
   cell. v1.1p20: a list sorted by index, so the lowest-index core still wins. */
#define LIMIT_MAX 1024
static int limitCount=-1;static uint16_t limitList[LIMIT_MAX];
static int byIndex(const void *a,const void *b){return (int)*(const uint16_t*)a-(int)*(const uint16_t*)b;}
void terrainLimitAirports(float e0,float n0,float e1,float n1){
 limitCount=airportsInBox(e0,n0,e1,n1,limitList,LIMIT_MAX);
 qsort(limitList,limitCount,sizeof(uint16_t),byIndex);
}
void terrainLimitAirportsOff(void){limitCount=-1;}
float terrainWithFlattening(float e,float n,float *flattening){
 float h=rawElevation(e,n),flat=0,sumW=0,sumE=0,coreE=0;int core=-1;
 const uint16_t *list=limitList;int count=limitCount;
 if(count<0)count=airportCellRunways(e,n,&list);
 for(int k=0;k<count;k++){int i=list[k];
  const Airport *ap=&airports[i];float along,blend=airportBlend(i,e,n,&along);
  if(blend>=1)continue;
  float runway=runwayElevation(ap,along);
  if(blend<=0){if(core<0){core=i;coreE=runway;}continue;}
  float w=(1-blend)/blend;sumW+=w;sumE+=w*runway;flat=fmaxf(flat,1-blend);
 }
 if(core>=0){h=coreE;flat=1;}
 else if(sumW>0)h=h*(1-flat)+(sumE/sumW)*flat;
 if(flattening)*flattening=flat;
 return h;
}
float terrain(float e,float n){return terrainWithFlattening(e,n,0);}
