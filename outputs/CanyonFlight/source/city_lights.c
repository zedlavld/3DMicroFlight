#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "city_lights.h"
#include "terrain.h"
#include "daylight.h"
#include "night_lights_bin.h"
typedef struct {int16_t cx,cy;uint32_t first;uint16_t n[3];} Cell;
typedef struct {int16_t dx,dy;uint8_t attr,brightness;} Point;
/* v1.1p14: gpu holds cap lights (grown as the camera comes closer, not n2 up front: dense
   eastern cells would otherwise take tens of MB of linear memory); ram holds a card cell's
   points read so far (the first ramCount, level 0 first). */
typedef struct {LightPoint *gpu;uint16_t filled,flushed,cap,ramCount;unsigned lastUse;Point *ram;} CellState;
/* Two stores with the same cell layout: the embedded Arizona/Nevada table (v34.02) and, from
   v1.1p14, scenery/night_lights_us.bin on the card for the rest of the lower 48 (OSM streets),
   whose points are read per cell by a low-priority worker. The stores never share a cell. */
typedef struct {const unsigned char *table;const Point *points;int cellCount;CellState *state;unsigned char *heapTable;} Store;
static Store stores[2];static int storeCount;
static int cellSize;
static int statCells,statDrawn,statPending;
/* Traffic pieces: int16 cx, cy, uint32 first, uint16 count, uint16 primary. */
static const unsigned char *pieces,*piecePoints;static int pieceCount,trafficPoints;
static float *pieceZ,*pieceS;static unsigned char *pieceReady;
static int rd32(const unsigned char *p){return p[0]|p[1]<<8|p[2]<<16|p[3]<<24;}
static Cell cellAt(const Store *s,int i){
 const unsigned char *p=s->table+14*i;Cell c;
 c.cx=(int16_t)(p[0]|p[1]<<8);c.cy=(int16_t)(p[2]|p[3]<<8);c.first=(uint32_t)rd32(p+4);
 for(int k=0;k<3;k++)c.n[k]=(uint16_t)(p[8+2*k]|p[9+2*k]<<8);
 return c;
}
/* ------------------------------------------------------------ card store and its reader */
#define US_LIGHTS_PATH "sdmc:/3ds/CanyonFlight/scenery/night_lights_us.bin"
static FILE *usFile;static long usPointsAt;static int usState; /* 0 not tried, 1 open, -1 absent */
static Thread usWorker;static LightEvent usWake;static int usQuit;
static int usJobState,usJobCell,usJobCount;static Point *usJobRam; /* 0 idle, 1 reading, 2 read */
static uint32_t usJobFirst;
static void usReader(void *unused){
 (void)unused;
 for(;;){
  LightEvent_Wait(&usWake);if(__atomic_load_n(&usQuit,__ATOMIC_ACQUIRE))return;
  Point *p=malloc(sizeof(Point)*usJobCount);
  if(p&&(fseek(usFile,usPointsAt+6L*usJobFirst,SEEK_SET)||fread(p,6,usJobCount,usFile)!=(size_t)usJobCount)){free(p);p=NULL;}
  usJobRam=p;__atomic_store_n(&usJobState,2,__ATOMIC_RELEASE);
 }
}
static void usOpen(void){
 usState=-1;FILE *f=fopen(US_LIGHTS_PATH,"rb");if(!f)return;
 unsigned char head[20];
 if(fread(head,1,20,f)!=20||memcmp(head,"CFLITE03",8)||rd32(head+8)!=cellSize){fclose(f);return;}
 int cells=rd32(head+12);
 Store *s=&stores[storeCount];memset(s,0,sizeof(*s));
 if(cells<=0||cells>400000||!(s->heapTable=malloc(14*cells))||fread(s->heapTable,14,cells,f)!=(size_t)cells||!(s->state=calloc(cells,sizeof(CellState)))){
  free(s->heapTable);free(s->state);memset(s,0,sizeof(*s));fclose(f);return;}
 s->table=s->heapTable;s->cellCount=cells;usFile=f;usPointsAt=20+14L*cells;
 LightEvent_Init(&usWake,RESET_ONESHOT);
 usWorker=threadCreate(usReader,NULL,8192,0x38,-2,false);
 if(!usWorker){free(s->heapTable);free(s->state);memset(s,0,sizeof(*s));fclose(f);usFile=NULL;return;}
 storeCount++;usState=1;
}
static void usStop(void){
 if(usWorker){__atomic_store_n(&usQuit,1,__ATOMIC_RELEASE);LightEvent_Signal(&usWake);threadJoin(usWorker,U64_MAX);threadFree(usWorker);usWorker=NULL;}
 usQuit=0;free(usJobRam);usJobRam=NULL;usJobState=0;
 if(usFile){fclose(usFile);usFile=NULL;}usState=0;
}
/* A finished read replaces the cell's points (the job always reads a longer prefix). */
static void usCollect(void){
 if(__atomic_load_n(&usJobState,__ATOMIC_ACQUIRE)!=2)return;
 Store *s=&stores[storeCount-1];CellState *st=&s->state[usJobCell];
 if(usJobRam&&usJobCount>st->ramCount){free(st->ram);st->ram=usJobRam;st->ramCount=(uint16_t)usJobCount;}else free(usJobRam);
 usJobRam=NULL;__atomic_store_n(&usJobState,0,__ATOMIC_RELEASE);
}
static void usRequest(int i,const Cell *c,int count){
 if(usJobState)return;
 usJobCell=i;usJobFirst=c->first;usJobCount=count;usJobRam=NULL;
 __atomic_store_n(&usJobState,1,__ATOMIC_RELEASE);LightEvent_Signal(&usWake);
}
int initCityLights(void){
 const unsigned char *b=night_lights_bin;storeCount=0;
 if(night_lights_bin_size<20||memcmp(b,"CFLITE02",8))return 0;
 cellSize=rd32(b+8);int cellCount=rd32(b+12),pointCount=rd32(b+16);
 if((unsigned)(20+14*cellCount+6*pointCount)>night_lights_bin_size)return 0;
 Store *s=&stores[0];memset(s,0,sizeof(*s));
 s->table=b+20;s->points=(const Point*)(b+20+14*cellCount);s->cellCount=cellCount;
 if(((uintptr_t)s->points)&1)return 0;
 s->state=calloc(cellCount,sizeof(CellState));if(!s->state)return 0;storeCount=1;
 const unsigned char *tr=b+20+14*cellCount+6*pointCount;
 if((unsigned)(tr+8-b)<=night_lights_bin_size){
  pieceCount=rd32(tr);trafficPoints=rd32(tr+4);pieces=tr+8;piecePoints=pieces+12*pieceCount;
  if((unsigned)(piecePoints+4*trafficPoints-b)>night_lights_bin_size)pieceCount=trafficPoints=0;
  else {pieceZ=malloc(sizeof(float)*trafficPoints);pieceS=malloc(sizeof(float)*trafficPoints);pieceReady=calloc(pieceCount,1);if(!pieceZ||!pieceS||!pieceReady)pieceCount=0;}
 }
 return 1;
}
void freeCityLights(void){
 usStop();
 for(int k=0;k<storeCount;k++){Store *s=&stores[k];
  if(s->state)for(int i=0;i<s->cellCount;i++){if(s->state[i].gpu)linearFree(s->state[i].gpu);free(s->state[i].ram);}
  free(s->state);free(s->heapTable);memset(s,0,sizeof(*s));}
 storeCount=0;
 free(pieceZ);free(pieceS);free(pieceReady);pieceZ=pieceS=NULL;pieceReady=NULL;pieceCount=0;
}
static int lowerBound(const Store *s,int cy,int cx){
 int lo=0,hi=s->cellCount;
 while(lo<hi){int mid=(lo+hi)/2;Cell c=cellAt(s,mid);if(c.cy<cy||(c.cy==cy&&c.cx<cx))lo=mid+1;else hi=mid;}
 return lo;
}
static const float palette[4][3]={{1,.62f,.26f},{1,.84f,.58f},{.86f,.9f,1},{1,.56f,.2f}};
static const float minPixels[3]={2.2f,1.6f,1.25f};
static void fill(CellState *st,const Cell *c,const Point *src,int n){
 float x0=(float)c->cx*cellSize,y0=(float)c->cy*cellSize;
 for(int j=st->filled;j<st->filled+n;j++){
  const Point *p=&src[j];int level=(p->attr>>2)&3,colour=p->attr&3;float b=(float)p->brightness;
  float lift=(p->attr&16)?9:5;
  st->gpu[j]=(LightPoint){p->dx,p->dy,rawElevation(x0+p->dx,y0+p->dy)+lift,
   (unsigned char)(palette[colour][0]*b),(unsigned char)(palette[colour][1]*b),(unsigned char)(palette[colour][2]*b),
   (unsigned char)(minPixels[level<3?level:2]*40)};
 }
 st->filled+=n;
}
/* Grows the cell's GPU buffer to hold count lights, keeping those already filled. */
static int ensureCapacity(CellState *st,int count){
 if(st->gpu&&st->cap>=count)return 1;
 LightPoint *g=linearAlloc(sizeof(LightPoint)*count);if(!g)return 0;
 if(st->gpu){memcpy(g,st->gpu,sizeof(LightPoint)*st->filled);linearFree(st->gpu);}
 st->gpu=g;st->cap=(uint16_t)count;st->flushed=0;return 1;
}
void cityLights(const CameraPose *camera,unsigned frame,CityDraw draw){
 statCells=statDrawn=statPending=0;
 if(!storeCount)return;
 if(!usState)usOpen();
 if(usState>0)usCollect();
 Vec eye=camera->eye;int budget=600;
 const float reach[3]={42000,10000,3500};
 int r=(int)ceilf(reach[0]/cellSize);int ex=(int)floorf(eye.x/cellSize),ey=(int)floorf(eye.y/cellSize);
 for(int k=0;k<storeCount;k++){Store *s=&stores[k];int onCard=!s->points;
 for(int pass=0;pass<2;pass++){  /* pass 0 fills cells within 10 km only; pass 1 fills the rest and draws */
  for(int cy=ey-r;cy<=ey+r;cy++){
   for(int i=lowerBound(s,cy,ex-r);i<s->cellCount;i++){
    Cell c=cellAt(s,i);if(c.cy!=cy||c.cx>ex+r)break;
    CellState *st=&s->state[i];
    float x0=(float)c.cx*cellSize,y0=(float)c.cy*cellSize;
    float dx=x0+cellSize*.5f-eye.x,dy=y0+cellSize*.5f-eye.y,d=sqrtf(dx*dx+dy*dy),half=cellSize*.71f;
    if(d-half>reach[0])continue;
    {/* Whole-cell view test: bounding sphere (half-diagonal plus relief)
        against the 400x240 frustum at 220 px focal length. */
     float cz=st->filled?st->gpu[0].z:eye.z-600,rad=half+400;
     float rz=cz-eye.z,depth=dx*camera->forward.x+dy*camera->forward.y+rz*camera->forward.z;
     if(depth<-rad)continue;
     float sx=dx*camera->right.x+dy*camera->right.y+rz*camera->right.z,sy=dx*camera->up.x+dy*camera->up.y+rz*camera->up.z,e=fmaxf(depth,0);
     if(fabsf(sx)>1.05f*e+rad*1.46f||fabsf(sy)>.66f*e+rad*1.2f)continue;
    }
    int count=d-half<reach[2]?c.n[2]:d-half<reach[1]?c.n[1]:c.n[0];
    if(!count)continue;
    if(pass==0&&d-half>=reach[1])continue;
    st->lastUse=frame;
    const Point *src=s->points?s->points+c.first:st->ram;
    int have=onCard?st->ramCount:count;
    if(onCard&&have<count)usRequest(i,&c,count);
    int want=count<have?count:have;
    if(want>st->filled&&budget>0){
     if(!ensureCapacity(st,want))continue;
     int n=want-st->filled;if(n>budget)n=budget;fill(st,&c,src,n);budget-=n;}
    if(pass==0)continue;
    if(!st->gpu)continue;
    if(st->flushed<st->filled){GSPGPU_FlushDataCache(st->gpu+st->flushed,(st->filled-st->flushed)*sizeof(LightPoint));st->flushed=st->filled;}
    int n=st->filled<count?st->filled:count;if(st->filled<count)statPending+=count-st->filled;
    if(n){draw(st->gpu,(Vec){x0,y0,0},n);statCells++;statDrawn+=n;}
   }
  }
 }
 /* Free buffers of cells not drawn for ~20 s (keeps linear memory bounded). */
 {static int sweep[2];
  for(int j=0;j<64&&s->cellCount;j++){sweep[k]=(sweep[k]+1)%s->cellCount;CellState *st=&s->state[sweep[k]];
   if((st->gpu||st->ram)&&frame-st->lastUse>1200&&!(onCard&&usJobState&&usJobCell==sweep[k])){
    if(st->gpu){linearFree(st->gpu);}free(st->ram);st->gpu=NULL;st->ram=NULL;st->filled=st->flushed=st->cap=st->ramCount=0;}}}
 }
}
void cityLightStats(int *cells,int *drawn,int *pending){*cells=statCells;*drawn=statDrawn;*pending=statPending;}
/* Moving car lights on primary and secondary roads within 12 km: one lamp
   per car, white when it drives toward the viewer, red when away; right-hand
   traffic 6 m either side of the centreline. */
static int sv16(const unsigned char *p){return (int16_t)(p[0]|p[1]<<8);}
static int pieceLower(int cy,int cx){
 int lo=0,hi=pieceCount;
 while(lo<hi){int mid=(lo+hi)/2;const unsigned char *p=pieces+12*mid;int y=sv16(p+2),x=sv16(p);
  if(y<cy||(y==cy&&x<cx))lo=mid+1;else hi=mid;}
 return lo;
}
void trafficLights(Vec eye,Vec forward,float time,float night,LightEmit emit){
 float k=daylightLampLevel(night);
 if(!pieceCount||k<=0)return;
 const float reach=10000;
 int r=(int)ceilf(reach/cellSize)+1,ex=(int)floorf(eye.x/cellSize),ey=(int)floorf(eye.y/cellSize),budget=40;
 for(int cy=ey-r;cy<=ey+r;cy++)
  for(int i=pieceLower(cy,ex-r);i<pieceCount;i++){
   const unsigned char *pc=pieces+12*i;int cx=sv16(pc),y=sv16(pc+2);if(y!=cy||cx>ex+r)break;
   int first=rd32(pc+4),n=pc[8]|pc[9]<<8,primary=pc[10];
   float x0=(float)cx*cellSize,y0=(float)cy*cellSize;
   const unsigned char *q=piecePoints+4*first;
   float ax=x0+sv16(q),ay=y0+sv16(q+2),dx=ax-eye.x,dy=ay-eye.y;
   if(dx*dx+dy*dy>reach*reach)continue;
   if(dx*forward.x+dy*forward.y<-800)continue;   /* pieces are at most ~1 km long behind their first point */
   if(!pieceReady[i]){if(budget--<=0)continue;
    float acc=0;
    for(int j=0;j<n;j++){pieceZ[first+j]=rawElevation(x0+sv16(q+4*j),y0+sv16(q+4*j+2))+1.2f;
     if(j){float sx=sv16(q+4*j)-sv16(q+4*j-4),sy=sv16(q+4*j+2)-sv16(q+4*j-2);acc+=sqrtf(sx*sx+sy*sy);}pieceS[first+j]=acc;}
    pieceReady[i]=1;}
   const float *S=pieceS+first,*Z=pieceZ+first;float len=S[n-1];
   float spacing=primary?700:1600,speed=primary?31:24;
   unsigned h=(unsigned)i*2654435761u;
   for(int dir=-1;dir<=1;dir+=2){
    float phase=(float)((h>>(dir>0?8:20))&1023)/1024*spacing;
    float s0=fmodf(phase+dir*time*speed,spacing);if(s0<0)s0+=spacing;
    int j=0;
    for(float s=s0;s<len;s+=spacing){
     while(j+2<n&&S[j+1]<s)j++;
     float sl=S[j+1]-S[j];if(sl<1)continue;
     float sx=sv16(q+4*j+4)-sv16(q+4*j),sy=sv16(q+4*j+6)-sv16(q+4*j+2),u=(s-S[j])/sl;if(u>1)u=1;
     float tx=sx/sl*dir,ty=sy/sl*dir; /* travel direction */
     Vec p={x0+sv16(q+4*j)+sx*u+ty*6,y0+sv16(q+4*j+2)+sy*u-tx*6,Z[j]+(Z[j+1]-Z[j])*u};
     float toward=(eye.x-p.x)*tx+(eye.y-p.y)*ty;
     if(toward>0)emit(p,1*k,.94f*k,.78f*k,.3f,1.35f);else emit(p,.85f*k,.07f*k,.04f*k,.3f,1.2f);
    }
   }
  }
}
