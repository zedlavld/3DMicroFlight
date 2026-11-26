/* v1.1p20 flyable US runways and the runway spatial index; see include/airport_index.h. */
#include "airport_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define PI 3.14159265359f
/* Card record (tools/build_us_runways.py), 76 bytes, little endian. */
#define RECORD_BYTES 76
typedef struct {char id[8],name[24],le[3],he[3],state[3];} AirportText;
static Airport *loaded;static AirportText *loadedText;
/* Index over airports[0..airportCount). */
static int built,gridW,gridH;static float gridE0,gridN0;
static uint32_t *cellFirst;static uint16_t *cellList;
static float *axisSin,*axisCos,*reachSq;
const float *airportAxisSin,*airportAxisCos,*airportReachSquared;
static int *starts,startCount,*startPos;
static void freeIndex(void){
 free(cellFirst);free(cellList);free(axisSin);free(axisCos);free(reachSq);free(starts);free(startPos);
 cellFirst=0;cellList=0;axisSin=axisCos=reachSq=0;starts=startPos=0;startCount=0;built=0;
 airportAxisSin=airportAxisCos=airportReachSquared=0;
}
/* Registration radius: the flattening reach, or the scatter keep-out of scenery.c (0.6 of the
   length along, 310 m across), whichever is larger. */
static float registerRadius(const Airport *a){
 float reach=airportReach(a),keep=sqrtf(a->length*.6f*a->length*.6f+310*310);
 return reach>keep?reach:keep;
}
static int cellX(float e){return (int)floorf((e-gridE0)/AIRPORT_INDEX_CELL);}
static int cellY(float n){return (int)floorf((n-gridN0)/AIRPORT_INDEX_CELL);}
static const char *stateOf(int i){const char *s=airports[i].state;return s?s:"";}
static int startOrder(const void *a,const void *b){
 int i=*(const int*)a,j=*(const int*)b,c=strcmp(stateOf(i),stateOf(j));
 return c?c:i-j;   /* compiled records first, then the card's (sorted by class and name) */
}
static int buildIndex(void){
 freeIndex();
 int n=airportCount;
 axisSin=malloc(n*sizeof(float));axisCos=malloc(n*sizeof(float));reachSq=malloc(n*sizeof(float));
 starts=malloc(n*sizeof(int));startPos=malloc(n*sizeof(int));
 if(!axisSin||!axisCos||!reachSq||!starts||!startPos){freeIndex();return 0;}
 float e0=1e30f,n0=1e30f,e1=-1e30f,n1=-1e30f;
 for(int i=0;i<n;i++){
  const Airport *a=&airports[i];float h=a->heading*PI/180,r=airportReach(a),g=registerRadius(a);
  axisSin[i]=sinf(h);axisCos[i]=cosf(h);reachSq[i]=r*r;
  if(a->east-g<e0)e0=a->east-g;if(a->east+g>e1)e1=a->east+g;
  if(a->north-g<n0)n0=a->north-g;if(a->north+g>n1)n1=a->north+g;
 }
 gridE0=e0;gridN0=n0;gridW=(int)((e1-e0)/AIRPORT_INDEX_CELL)+1;gridH=(int)((n1-n0)/AIRPORT_INDEX_CELL)+1;
 cellFirst=calloc((size_t)gridW*gridH+1,sizeof(uint32_t));
 if(!cellFirst){freeIndex();return 0;}
 /* Two passes: count per cell, then fill. */
 size_t total=0;
 for(int pass=0;pass<2;pass++){
  for(int i=0;i<n;i++){
   const Airport *a=&airports[i];float g=registerRadius(a);
   int x0=cellX(a->east-g),x1=cellX(a->east+g),y0=cellY(a->north-g),y1=cellY(a->north+g);
   for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
    size_t c=(size_t)y*gridW+x;
    if(!pass){cellFirst[c+1]++;total++;}else cellList[cellFirst[c]++]=(uint16_t)i;
   }
  }
  if(!pass){
   for(size_t c=0;c<(size_t)gridW*gridH;c++)cellFirst[c+1]+=cellFirst[c];
   cellList=malloc((total?total:1)*sizeof(uint16_t));if(!cellList){freeIndex();return 0;}
  }else{ /* fill advanced each start to the next cell's: shift back */
   for(size_t c=(size_t)gridW*gridH;c>0;c--)cellFirst[c]=cellFirst[c-1];
   cellFirst[0]=0;
  }
 }
 startCount=0;
 for(int i=0;i<n;i++){startPos[i]=-1;if(airports[i].primary)starts[startCount++]=i;}
 qsort(starts,startCount,sizeof(int),startOrder);
 for(int k=0;k<startCount;k++)startPos[starts[k]]=k;
 airportAxisSin=axisSin;airportAxisCos=axisCos;airportReachSquared=reachSq;
 built=1;return 1;
}
static int ready(void){return built||buildIndex();}
int airportCellRunways(float e,float n,const uint16_t **list){
 if(!ready())return 0;
 int x=cellX(e),y=cellY(n);
 if(x<0||y<0||x>=gridW||y>=gridH)return 0;
 size_t c=(size_t)y*gridW+x;*list=cellList+cellFirst[c];return (int)(cellFirst[c+1]-cellFirst[c]);
}
/* Records with their centre inside the box, each once: visited only in their home cell. */
typedef struct {float d;int i;} Hit;
static int byDistance(const void *a,const void *b){float x=((const Hit*)a)->d,y=((const Hit*)b)->d;return x<y?-1:x>y;}
#define NEAR_SCRATCH 2048
int airportsNear(float e,float n,float radius,int *out,int max){
 if(!ready()||max<=0)return 0;
 static Hit *hits;if(!hits&&!(hits=malloc(NEAR_SCRATCH*sizeof(Hit))))return 0; /* heap: the program image is near 32 MB */
 int count=0;
 int x0=cellX(e-radius),x1=cellX(e+radius),y0=cellY(n-radius),y1=cellY(n+radius);
 if(x0<0)x0=0;if(y0<0)y0=0;if(x1>=gridW)x1=gridW-1;if(y1>=gridH)y1=gridH-1;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  size_t c=(size_t)y*gridW+x;
  for(uint32_t k=cellFirst[c];k<cellFirst[c+1];k++){
   int i=cellList[k];const Airport *a=&airports[i];
   if(cellX(a->east)!=x||cellY(a->north)!=y)continue;
   float dx=a->east-e,dy=a->north-n,d=dx*dx+dy*dy;
   if(d>radius*radius)continue;
   if(count<NEAR_SCRATCH)hits[count++]=(Hit){d,i};
  }
 }
 qsort(hits,count,sizeof(Hit),byDistance);
 if(count>max)count=max;
 for(int k=0;k<count;k++)out[k]=hits[k].i;
 return count;
}
int airportsInBox(float e0,float n0,float e1,float n1,uint16_t *out,int max){
 if(!ready())return 0;
 float g=6000; /* largest registration radius is ~3 km: centres within the box grown by it */
 int x0=cellX(e0-g),x1=cellX(e1+g),y0=cellY(n0-g),y1=cellY(n1+g),count=0;
 if(x0<0)x0=0;if(y0<0)y0=0;if(x1>=gridW)x1=gridW-1;if(y1>=gridH)y1=gridH-1;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  size_t c=(size_t)y*gridW+x;
  for(uint32_t k=cellFirst[c];k<cellFirst[c+1];k++){
   int i=cellList[k];const Airport *a=&airports[i];
   if(cellX(a->east)!=x||cellY(a->north)!=y)continue;
   float r=sqrtf(reachSq[i]);
   if(a->east+r>=e0&&a->east-r<=e1&&a->north+r>=n0&&a->north-r<=n1&&count<max)out[count++]=(uint16_t)i;
  }
 }
 return count;
}
void airportGroup(int i,int *first,int *count){
 int a=i,b=i;
 while(a>0&&!strcmp(airports[a-1].id,airports[i].id)&&!airports[a].primary)a--;
 while(b+1<airportCount&&!strcmp(airports[b+1].id,airports[i].id)&&!airports[b+1].primary)b++;
 *first=a;*count=b-a+1;
}
int airportFind(const char *id){
 if(!ready())return -1;
 for(int k=0;k<startCount;k++)if(!strcasecmp(airports[starts[k]].id,id))return starts[k];
 return -1;
}
int airportNearest(float e,float n,float skip){
 int best=-1;float bestD=1e30f;
 for(float radius=40000;radius<=5e6f&&best<0;radius*=4){
  static int near[256];int count=airportsNear(e,n,radius,near,256);
  for(int k=0;k<count;k++){const Airport *a=&airports[near[k]];if(!a->primary)continue;
   float d=hypotf(a->east-e,a->north-n);if(d<skip)continue;
   if(d<bestD){bestD=d;best=near[k];}}
 }
 return best;
}
int airportStartCount(void){return ready()?startCount:0;}
int airportStartAt(int k){return ready()&&startCount?starts[((k%startCount)+startCount)%startCount]:0;}
int nextAirportStart(int ap,int dir){
 if(!ready()||!startCount)return 0;
 int k=ap>=0&&ap<airportCount&&startPos[ap]>=0?startPos[ap]:0;
 return starts[(k+(dir<0?startCount-1:1))%startCount];
}
int airportNextState(int ap,int dir){
 if(!ready()||!startCount)return 0;
 int k=ap>=0&&ap<airportCount&&startPos[ap]>=0?startPos[ap]:0;
 #define ST(k) stateOf(starts[((k)%startCount+startCount)%startCount])
 int first=k;while(first>0&&!strcmp(ST(first-1),ST(k)))first--;   /* start of this state */
 if(dir>0){int j=first;int steps=0;while(steps<startCount&&!strcmp(ST(j),ST(first))){j++;steps++;}return starts[j%startCount];}
 int j=(first-1+startCount)%startCount;   /* last airport of the previous state */
 while(j>0&&!strcmp(ST(j-1),ST(j)))j--;
 #undef ST
 return starts[j];
}
uint32_t airportIdHash(const char *id){
 uint32_t h=2166136261u;for(const char *c=id;*c;c++){h^=(unsigned char)(*c>='a'&&*c<='z'?*c-32:*c);h*=16777619u;}
 return h?h:1;
}
int airportFindHash(uint32_t h){
 if(!ready())return -1;
 for(int k=0;k<startCount;k++)if(airportIdHash(airports[starts[k]].id)==h)return starts[k];
 return -1;
}
static float rdf(const unsigned char *p){uint32_t u=p[0]|p[1]<<8|p[2]<<16|(uint32_t)p[3]<<24;float f;memcpy(&f,&u,4);return f;}
static int rd32(const unsigned char *p){return (int)(p[0]|p[1]<<8|p[2]<<16|(uint32_t)p[3]<<24);}
static void text(char *out,const unsigned char *p,int n,int size){memcpy(out,p,n);out[n<size?n:size-1]=0;out[size-1]=0;}
int airportsLoad(const char *path){
 FILE *f=fopen(path,"rb");if(!f){buildIndex();return 0;}
 unsigned char head[20];int count=0,ok=fread(head,1,20,f)==20&&!memcmp(head,"CFRWY001",8);
 if(ok){count=rd32(head+8);ok=count>0&&count<65000-AIRPORT_COUNT&&rd32(head+12)==RECORD_BYTES;}
 unsigned char *raw=ok?malloc((size_t)count*RECORD_BYTES):0;
 ok=ok&&raw&&fread(raw,RECORD_BYTES,count,f)==(size_t)count;
 fclose(f);
 Airport *table=ok?malloc((AIRPORT_COUNT+(size_t)count)*sizeof(Airport)):0;
 AirportText *names=ok?malloc((size_t)count*sizeof(AirportText)):0;
 if(!ok||!table||!names){free(raw);free(table);free(names);buildIndex();return 0;}
 memcpy(table,builtinAirports,sizeof(builtinAirports));
 int n=AIRPORT_COUNT;
 for(int k=0;k<count;k++){
  const unsigned char *p=raw+(size_t)k*RECORD_BYTES;Airport *a=&table[n];AirportText *t=&names[k];
  float v[8];int finite=1;for(int j=0;j<8;j++){v[j]=rdf(p+4*j);if(!isfinite(v[j]))finite=0;}
  if(!finite||v[4]<100||v[4]>6000||v[5]<5||v[5]>120||fabsf(v[0])>5e6f||fabsf(v[1])>5e6f)continue;
  text(t->id,p+32,8,8);text(t->name,p+40,24,24);text(t->le,p+64,3,3);text(t->he,p+67,3,3);text(t->state,p+70,2,3);
  if(!t->id[0]||strlen(t->le)!=2||strlen(t->he)!=2)continue;
  *a=(Airport){t->id,v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],t->name,t->le,t->he,p[72]?1:0,(signed char)p[73],t->state,p[74]};
  if(a->apronSide<-1||a->apronSide>1)a->apronSide=0;
  /* a secondary record must follow its primary */
  if(!a->primary&&(n==AIRPORT_COUNT||strcmp(table[n-1].id,a->id)))continue;
  n++;
 }
 free(raw);
 if(n==AIRPORT_COUNT){free(table);free(names);buildIndex();return 0;}
 Airport *oldTable=loaded;AirportText *oldText=loadedText;
 loaded=table;loadedText=names;airports=table;airportCount=n;
 free(oldTable);free(oldText);
 if(!buildIndex()){airports=builtinAirports;airportCount=AIRPORT_COUNT;free(loaded);free(loadedText);loaded=0;loadedText=0;buildIndex();return 0;}
 return n-AIRPORT_COUNT;
}
