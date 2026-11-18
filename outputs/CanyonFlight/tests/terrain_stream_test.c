/* Host test for statewide elevation streaming (v31). Build with host gcc:
   gcc -O2 -Iinclude tests/terrain_stream_test.c source/terrain_stream.c
       source/terrain.c source/terrain_data.c source/airports.c -lm
   and run with the elevation tile directory as argv[1]. */
#include "terrain_stream.h"
#include "terrain.h"
#include "terrain_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static int checks,failures;
#define CHECK(x,...) do{checks++;if(!(x)){failures++;printf("FAIL line %d: ",__LINE__);printf(__VA_ARGS__);printf("\n");}}while(0)
static const double LAT0=34.848598480225,LON0=-111.78800201416;
static Vec geo(double lat,double lon){
 double m=6371000*3.14159265358979/180,mx=m*cos(LAT0*3.14159265358979/180);
 return (Vec){(float)((lon-LON0)*mx),(float)((lat-LAT0)*m),0};
}
static float baseOnly(float e,float n){
 const ElevationGrid *g=&elevationGrids[0];
 float x=(e-g->east)/g->spacing,y=(n-g->north)/g->spacing;int ix=(int)x,iy=(int)y;float fx=x-ix,fy=y-iy;
 const int16_t *p=g->data+iy*g->width+ix;
 return ((1-fx)*p[0]+fx*p[1])*(1-fy)+((1-fx)*p[g->width]+fx*p[g->width+1])*fy;
}
int main(int argc,char **argv){
 const char *root=argc>1?argv[1]:"sdmc/3ds/CanyonFlight/scenery/elevation32";
 /* Every tile file must validate (header, indices, CRC). */
 static unsigned char buf[ELEV_HEADER+ELEV_PAYLOAD+1];int good=0,absent=0;
 for(int j=0;j<ELEV_TY;j++)for(int i=0;i<ELEV_TX;i++){
  char path[300];snprintf(path,sizeof(path),"%s/e_%d_%d.bin",root,i,j);FILE *f=fopen(path,"rb");
  if(!f){absent++;continue;} /* v32: only Arizona + Nevada + 60 km are written */
  size_t n=fread(buf,1,sizeof(buf),f);fclose(f);
  int ok=elevationTileValid(buf,(unsigned)n,i,j);CHECK(ok,"invalid tile %d %d",i,j);good+=ok;
  if(i==23&&j==4){buf[100]^=1;CHECK(!elevationTileValid(buf,(unsigned)n,i,j),"corruption not detected");}
 }
 printf("tiles valid: %d, absent %d of %d\n",good,absent,ELEV_TX*ELEV_TY);CHECK(good>=1000,"only %d tiles",good);
 CHECK(initTerrainStream(root),"init failed");
 /* Blocking path: the aircraft's surroundings are resident after one update. */
 Vec phx=geo(33.4343,-112.0116);
 float before;CHECK(!streamedElevation(phx.x,phx.y,&before),"tile visible before update");
 CHECK(terrainStreamUpdate(phx)==1,"update should publish nearby tiles");
 float h;CHECK(streamedElevation(phx.x,phx.y,&h),"PHX tile not resident");
 printf("KPHX %.1f m (published elevation 345)\n",h);CHECK(fabsf(h-345)<25,"KPHX %.1f",h);
 /* v32: Nevada. Reno Tahoe field elevation 1345 m. */
 Vec rno=geo(39.4991,-119.768);CHECK(terrainStreamUpdate(rno)==1,"update should publish Reno tiles");
 float hr;CHECK(streamedElevation(rno.x,rno.y,&hr),"RNO tile not resident");
 printf("KRNO %.1f m (published elevation 1345)\n",hr);CHECK(fabsf(hr-1345)<30,"KRNO %.1f",hr);
 /* Seamless edges: both sides of a tile boundary agree. */
 int maxStep=0;float worst=0;
 for(int k=0;k<200;k++){
  float e=ELEV_E0+((int)((phx.x-ELEV_E0)/ELEV_TILE_METRES))*ELEV_TILE_METRES;float n=phx.y-9000+k*90;
  float a,b;if(streamedElevation(e-.01f,n,&a)&&streamedElevation(e+.01f,n,&b)){float d=fabsf(a-b);if(d>worst)worst=d;maxStep++;}
 }
 printf("edge samples %d, worst step %.3f m\n",maxStep,worst);CHECK(maxStep>50&&worst<.5f,"edge step %.3f",worst);
 /* Streamed 100 m data agrees with the embedded 500 m base on average. */
 double sum=0;int count=0;float maxd=0;
 for(int k=0;k<4000;k++){
  float e=phx.x-11000+(k%63)*350,n=phx.y-11000+(k/63)*350;float s;
  if(streamedElevation(e,n,&s)){float d=fabsf(s-baseOnly(e,n));sum+=d;count++;if(d>maxd)maxd=d;}
 }
 printf("stream vs base: mean |d| %.2f m, max %.1f m over %d points\n",sum/count,maxd,count);
 CHECK(count>3000&&sum/count<20,"mean diff %.2f",sum/count);
 /* Landmarks through the full rawElevation path, loading each area first. */
 struct {const char *name;double lat,lon;float expect,tol;} marks[]={
  {"Humphreys Peak",35.3463,-111.6779,3851,140},{"Mount Graham",32.7017,-109.8714,3267,140},
  {"KTUS",32.1161,-110.9410,806,25},{"KYUM",32.6566,-114.6060,65,25},{"KPRC",34.6545,-112.4196,1537,25},
  {"KIGM",35.2595,-113.9380,1053,25},{"Lake Havasu City",34.4839,-114.3225,250,90},{"KSOW",34.2655,-110.0055,1953,25}};
 for(unsigned m=0;m<sizeof(marks)/sizeof(marks[0]);m++){
  Vec p=geo(marks[m].lat,marks[m].lon);terrainStreamUpdate(p);
  float r=rawElevation(p.x,p.y);int streamed=streamedElevation(p.x,p.y,&h);
  printf("%-17s %7.1f m (expect ~%.0f) streamed=%d\n",marks[m].name,r,marks[m].expect,streamed);
  CHECK(streamed&&fabsf(r-marks[m].expect)<marks[m].tol,"%s %.1f",marks[m].name,r);
 }
 /* A long flight: resident tiles stay bounded and distant tiles retire. */
 Vec p=geo(31.6,-110.5);Vec q=geo(36.8,-113.5);int maxResident=0;
 for(int k=0;k<=400;k++){
  Vec c={p.x+(q.x-p.x)*k/400,p.y+(q.y-p.y)*k/400,0};
  for(int r=0;r<6;r++)terrainStreamUpdate(c); /* background loads */
  terrainStreamPublish(c);int n=terrainStreamResident();if(n>maxResident)maxResident=n;
  CHECK(streamedElevation(c.x,c.y,&h),"no tile under aircraft at step %d",k);
 }
 printf("max resident %d/%d, failures %d\n",maxResident,ELEV_SLOTS,terrainStreamFailures());
 CHECK(maxResident<=ELEV_SLOTS&&terrainStreamFailures()==0,"resident %d failures %d",maxResident,terrainStreamFailures());
 freeTerrainStream();CHECK(!streamedElevation(phx.x,phx.y,&h),"visible after free");
 printf("%d checks, %d failures\n",checks,failures);
 return failures!=0;
}
