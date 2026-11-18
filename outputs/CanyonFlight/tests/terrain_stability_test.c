#include "scenery.h"
#include "terrain.h"
#include "imagery.h"
#include "weather_visual.h"
#include <stdint.h>
#include <string.h>
int checks,failures,firstFailure,boundaryVertices,largestJob,skirts,sharedJoins;
/* v31.6 watertight joins: every vertex on a finer ring's outer edge must
   exist bit-identically in the next coarser ring, so no T-junction remains
   for the 3DS's float24 transform to open into a crack. */
static Vec fineEdge[2][8192],coarseLine[8192];
static WorldVertex vertices[SCENERY_PART_CAPACITY];
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
int main(void){
#ifndef BASELINE_V25
 WeatherPuff puffs[WEATHER_PUFF_CAPACITY];Flight weather={0};weather.pos.z=1800;
 CHECK(weatherPuffs(&weather,puffs,WEATHER_PUFF_CAPACITY)==0);
 for(int type=1;type<=2;type++)for(int location=-4;location<=4;location++){
  weather.weather=type;weather.pos.x=location*2400;weather.weatherTime=location*location*300;
  int n=weatherPuffs(&weather,puffs,WEATHER_PUFF_CAPACITY);CHECK(n>0&&n<=WEATHER_PUFF_CAPACITY);
  for(int i=0;i<n;i++)CHECK(isfinite(puffs[i].center.z)&&puffs[i].center.z>=3200&&puffs[i].width>0&&puffs[i].height>0&&puffs[i].opacity>=0&&puffs[i].opacity<=1);
  CHECK(weatherPuffs(&weather,puffs,1)==1);CHECK(weatherPuffs(&weather,puffs,0)==0);
 }
 weather.weather=2;weather.pos.z=2000;CHECK(weatherRain(&weather)==1);weather.pos.z=3500;CHECK(weatherRain(&weather)==0);
 weather.weatherTime=13.05f;CHECK(weatherFlash(&weather)>.9f&&weatherThunder(&weather)==0);
 weather.weatherTime=15.2f;CHECK(weatherFlash(&weather)==0&&weatherThunder(&weather)>0);
 weather.weather=0;CHECK(weatherRain(&weather)==0&&weatherFlash(&weather)==0&&weatherThunder(&weather)==0);
 float lastHaze=0;
 for(int d=0;d<=150000;d+=100){float f=sceneryHaze((float)d);CHECK(isfinite(f)&&f>=lastHaze&&f<=.95f);if(d<=3000)CHECK(f==0);lastHaze=f;}
 float horizon[3],zenith[3],below[3];scenerySkyColor(0,horizon);scenerySkyColor(1,zenith);scenerySkyColor(-1,below);
 for(int c=0;c<3;c++){CHECK(horizon[c]==below[c]);CHECK(zenith[c]<horizon[c]&&zenith[c]>=0);}
 /* Equal steepness/elevation isolates lighting from material selection. */
 float sunny[3],shady[3];
 scenerySurfaceColor(1500,1,1,sunny);scenerySurfaceColor(1500,-1,-1,shady);
 for(int c=0;c<3;c++)CHECK(sunny[c]>shady[c]*1.8f);
 for(int h=0;h<=4500;h+=150)for(int x=-4;x<=4;x++)for(int y=-4;y<=4;y++){
  float rgb[3];scenerySurfaceColor((float)h,x*.5f,y*.5f,rgb);
  for(int c=0;c<3;c++)CHECK(isfinite(rgb[c])&&rgb[c]>=0&&rgb[c]<=1);
 }
#endif
 const Vec positions[]={{-257,-1,2400},{-1,-257,2400},{0,0,2400},{255,255,2400},{256,256,2400},{10631,32230,2800},{-32760,122732,2800},{-32769,128001,2800}};
 for(unsigned test=0;test<sizeof(positions)/sizeof(*positions);test++){
  Flight s={0};s.pos=positions[test];double area=0;float finalEast=0,finalNorth=0;int fineCount=0;float fineEast=0,fineNorth=0,fineHalf=0;
  for(int level=0;level<8;level++){int lineCount=0,nextFine=0;
   float east,north,cell=32*(1<<level);int ix,iy;
   sceneryLodLayout(s.pos,level,&east,&north,&ix,&iy);finalEast=east;finalNorth=north;
   CHECK(fmodf(east,2*cell)==0&&fmodf(north,2*cell)==0);
   if(level){
    float pe,pn;int px,py;sceneryLodLayout(s.pos,level-1,&pe,&pn,&px,&py);
    CHECK(east+ix*cell==pe-8*cell&&east+(ix+16)*cell==pe+8*cell);
    CHECK(north+iy*cell==pn-8*cell&&north+(iy+16)*cell==pn+8*cell);
    CHECK(ix>=-16&&ix+16<=16&&iy>=-16&&iy+16<=16);
   }
   for(int part=level*4;part<level*4+4;part++){
    int n=buildSceneryPart(&s,vertices,SCENERY_PART_CAPACITY,part);largestJob=n>largestJob?n:largestJob;
    CHECK(n>0&&n%3==0&&n<SCENERY_PART_CAPACITY);
    static unsigned char skirtTri[SCENERY_PART_CAPACITY/3];
    for(int i=0;i<n;i+=3){
     Vec a=vertices[i].p,b=vertices[i+1].p,c=vertices[i+2].p;
     double cross=((double)b.x-a.x)*((double)c.y-a.y)-((double)b.y-a.y)*((double)c.x-a.x);
     /* v31.5 skirts: vertical triangles hanging below a ring's outer edge.
        They add no horizontal coverage; every vertex lies exactly on that
        outer boundary and none rises above the stitched edge. */
     skirtTri[i/3]=cross==0;
     if(skirtTri[i/3]){
      skirts++;
      for(int j=0;j<3;j++){Vec p=vertices[i+j].p;
       CHECK(isfinite(p.z)&&(fabsf(p.x-east)==16*cell||fabsf(p.y-north)==16*cell));}
      continue;
     }
     CHECK(cross>0);area+=cross*.5;
    }
    for(int i=0;i<n;i++){
     if(skirtTri[i/3])continue;
     Vec p=vertices[i].p;CHECK(isfinite(p.z));
     if(level&&((fabsf(p.x-fineEast)==fineHalf&&fabsf(p.y-fineNorth)<=fineHalf)||(fabsf(p.y-fineNorth)==fineHalf&&fabsf(p.x-fineEast)<=fineHalf))&&lineCount<8192)coarseLine[lineCount++]=p;
     if(level<7&&(fabsf(p.x-east)==16*cell||fabsf(p.y-north)==16*cell)&&nextFine<8192)fineEdge[level&1][nextFine++]=p;
     CHECK(fmodf(p.x,cell*.5f)==0&&fmodf(p.y,cell*.5f)==0);
     if(level==7)continue;
     int vertical=fabsf(p.x-east)==16*cell,horizontal=fabsf(p.y-north)==16*cell;
     if(!vertical&&!horizontal)continue;
     float value=vertical?p.y:p.x,coarse=2*cell,lo=floorf(value/coarse)*coarse,t=(value-lo)/coarse;
     float a=vertical?sceneryGroundHeight(p.x,lo):sceneryGroundHeight(lo,p.y),b=vertical?sceneryGroundHeight(p.x,lo+coarse):sceneryGroundHeight(lo+coarse,p.y);
     float expected=a+t*(b-a);boundaryVertices++;
     CHECK(fabsf(p.z-expected)<.02f);
    }
   }
   if(level){
    for(int i=0;i<fineCount;i++){int found=0;
     for(int j=0;j<lineCount&&!found;j++)found=!memcmp(&fineEdge[(level-1)&1][i],&coarseLine[j],sizeof(Vec));
     CHECK(found);sharedJoins+=found;}
   }
   fineCount=nextFine;fineEast=east;fineNorth=north;fineHalf=16*cell;
  }
  (void)finalEast;(void)finalNorth;
  /* No gaps or overlapping horizontal coverage in the complete nested grid. */
  CHECK(fabs(area-131072.0*131072.0)<1);
 }
 /* Moving through a 256-m build boundary cannot slide distant terrain cells. */
 for(int level=4;level<8;level++){
  float ae,an,be,bn;int ax,ay,bx,by;
  sceneryLodLayout((Vec){255,255,0},level,&ae,&an,&ax,&ay);
  sceneryLodLayout((Vec){256,256,0},level,&be,&bn,&bx,&by);
  CHECK(ae==be&&an==bn);
 }
 /* Reverse edge traversal must produce bit-identical tile cut vertices. */
 setImageryWindows((Vec){-32768,122880,0},(Vec){-32768,122880,0});setImageryFineEnabled(1);
 for(int sample=0;sample<40;sample++){
  float shift=sample*.073f;
  WorldVertex a[3]={{{-35111.3f+shift,120559.7f,1900.3f},{.4f,.5f,.6f}},{{-30555.2f,121517.9f+shift,2456.7f},{.6f,.5f,.4f}},{{-32899.1f,125161.3f,2123.4f+shift},{.3f,.4f,.5f}}};
  WorldVertex b[3]={a[2],a[1],a[0]};ImageryVertex front[30],back[30];
  for(int region=0;region<IMAGERY_REGIONS;region++){
   int n=clipImageryTriangle(a,region,front,30,(Vec){-32768,122880,2500});
   int m=clipImageryTriangle(b,region,back,30,(Vec){-32768,122880,2500});CHECK(n==m&&n>=0);
   for(int i=0;i<n;i++){
    int found=0;for(int j=0;j<m;j++)if(!memcmp(&front[i],&back[j],sizeof(ImageryVertex)))found=1;
    CHECK(found);
   }
  }
 }
 return failures;
}
