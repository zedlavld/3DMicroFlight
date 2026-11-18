#include <3ds.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "city_lights.h"
#include "terrain.h"
#include "daylight.h"
#include "night_lights_bin.h"
typedef struct {int16_t cx,cy;uint32_t first;uint16_t n[3];} Cell;
typedef struct {int16_t dx,dy;uint8_t attr,brightness;} Point;
typedef struct {LightPoint *gpu;uint16_t filled,flushed;unsigned lastUse;} CellState;
static const unsigned char *table;static const Point *points;
static int cellSize,cellCount,pointCount;
static CellState *state;
static int statCells,statDrawn,statPending;
/* Traffic pieces: int16 cx, cy, uint32 first, uint16 count, uint16 primary. */
static const unsigned char *pieces,*piecePoints;static int pieceCount,trafficPoints;
static float *pieceZ,*pieceS;static unsigned char *pieceReady;
static int rd32(const unsigned char *p){return p[0]|p[1]<<8|p[2]<<16|p[3]<<24;}
static Cell cellAt(int i){
 const unsigned char *p=table+14*i;Cell c;
 c.cx=(int16_t)(p[0]|p[1]<<8);c.cy=(int16_t)(p[2]|p[3]<<8);c.first=(uint32_t)rd32(p+4);
 for(int k=0;k<3;k++)c.n[k]=(uint16_t)(p[8+2*k]|p[9+2*k]<<8);
 return c;
}
int initCityLights(void){
 const unsigned char *b=night_lights_bin;
 if(night_lights_bin_size<20||memcmp(b,"CFLITE02",8))return 0;
 cellSize=rd32(b+8);cellCount=rd32(b+12);pointCount=rd32(b+16);
 if((unsigned)(20+14*cellCount+6*pointCount)>night_lights_bin_size)return 0;
 table=b+20;points=(const Point*)(b+20+14*cellCount);
 if(((uintptr_t)points)&1)return 0;
 state=calloc(cellCount,sizeof(CellState));
 const unsigned char *tr=b+20+14*cellCount+6*pointCount;
 if((unsigned)(tr+8-b)<=night_lights_bin_size){
  pieceCount=rd32(tr);trafficPoints=rd32(tr+4);pieces=tr+8;piecePoints=pieces+12*pieceCount;
  if((unsigned)(piecePoints+4*trafficPoints-b)>night_lights_bin_size)pieceCount=trafficPoints=0;
  else {pieceZ=malloc(sizeof(float)*trafficPoints);pieceS=malloc(sizeof(float)*trafficPoints);pieceReady=calloc(pieceCount,1);if(!pieceZ||!pieceS||!pieceReady)pieceCount=0;}
 }
 return state!=NULL;
}
void freeCityLights(void){
 if(state)for(int i=0;i<cellCount;i++)if(state[i].gpu)linearFree(state[i].gpu);
 free(state);state=NULL;cellCount=0;
 free(pieceZ);free(pieceS);free(pieceReady);pieceZ=pieceS=NULL;pieceReady=NULL;pieceCount=0;
}
static int lowerBound(int cy,int cx){
 int lo=0,hi=cellCount;
 while(lo<hi){int mid=(lo+hi)/2;Cell c=cellAt(mid);if(c.cy<cy||(c.cy==cy&&c.cx<cx))lo=mid+1;else hi=mid;}
 return lo;
}
static const float palette[4][3]={{1,.62f,.26f},{1,.84f,.58f},{.86f,.9f,1},{1,.56f,.2f}};
static const float minPixels[3]={2.2f,1.6f,1.25f};
static void fill(int i,const Cell *c,int n){
 CellState *st=&state[i];float x0=(float)c->cx*cellSize,y0=(float)c->cy*cellSize;
 for(int j=st->filled;j<st->filled+n;j++){
  const Point *p=&points[c->first+j];int level=(p->attr>>2)&3,colour=p->attr&3;float b=(float)p->brightness;
  float lift=(p->attr&16)?9:5;
  st->gpu[j]=(LightPoint){p->dx,p->dy,rawElevation(x0+p->dx,y0+p->dy)+lift,
   (unsigned char)(palette[colour][0]*b),(unsigned char)(palette[colour][1]*b),(unsigned char)(palette[colour][2]*b),
   (unsigned char)(minPixels[level<3?level:2]*40)};
 }
 st->filled+=n;
}
void cityLights(const CameraPose *camera,unsigned frame,CityDraw draw){
 statCells=statDrawn=statPending=0;
 if(!state)return;
 Vec eye=camera->eye;int budget=600;
 const float reach[3]={42000,10000,3500};
 int r=(int)ceilf(reach[0]/cellSize);int ex=(int)floorf(eye.x/cellSize),ey=(int)floorf(eye.y/cellSize);
 for(int pass=0;pass<2;pass++){  /* pass 0 fills cells within 10 km only; pass 1 fills the rest and draws */
  for(int cy=ey-r;cy<=ey+r;cy++){
   for(int i=lowerBound(cy,ex-r);i<cellCount;i++){
    Cell c=cellAt(i);if(c.cy!=cy||c.cx>ex+r)break;
    CellState *st=&state[i];
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
    if(!st->gpu){st->gpu=linearAlloc(sizeof(LightPoint)*c.n[2]);if(!st->gpu)continue;st->filled=st->flushed=0;}
    if(st->filled<count&&budget>0){int n=count-st->filled;if(n>budget)n=budget;fill(i,&c,n);budget-=n;}
    if(pass==0)continue;
    if(st->flushed<st->filled){GSPGPU_FlushDataCache(st->gpu+st->flushed,(st->filled-st->flushed)*sizeof(LightPoint));st->flushed=st->filled;}
    int n=st->filled<count?st->filled:count;if(st->filled<count)statPending+=count-st->filled;
    if(n){draw(st->gpu,(Vec){x0,y0,0},n);statCells++;statDrawn+=n;}
   }
  }
 }
 /* Free buffers of cells not drawn for ~20 s (keeps linear memory bounded). */
 static int sweep;
 for(int k=0;k<64&&cellCount;k++){sweep=(sweep+1)%cellCount;CellState *st=&state[sweep];
  if(st->gpu&&frame-st->lastUse>1200){linearFree(st->gpu);st->gpu=NULL;st->filled=st->flushed=0;}}
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
