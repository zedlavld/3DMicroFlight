/* Statewide 100 m elevation streaming (v31). See include/terrain_stream.h.
   On the 3DS a low-priority worker performs SD reads, one tile at a time,
   into a slot that is not yet visible; the main thread polls completion and
   owns the published tile table. Host builds read synchronously. */
#include "terrain_stream.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __3DS__
#include <3ds.h>
#endif

enum {SLOT_EMPTY,SLOT_LOADING,SLOT_PENDING,SLOT_PUBLISHED};
typedef struct {int i,j,state;unsigned char *file;} Slot;
static Slot slots[ELEV_SLOTS];
static unsigned char table[ELEV_TY][ELEV_TX];  /* published slot + 1; 0 = none */
static unsigned char failed[ELEV_TY][ELEV_TX]; /* missing or invalid this session */
static int enabled,failures,syncReads;static unsigned generation;
/* v34.04: per-tile change counters, so derived caches only drop what lies
   over a tile that was published or released (terrainStreamRegionVersion). */
static unsigned tileVersion[ELEV_TY][ELEV_TX],resets;
static char rootPath[160];
static int jobState,jobSlot,jobValid; /* jobState: 0 idle, 1 reading, 2 complete */
#ifdef __3DS__
static Thread worker;static LightEvent wake;static int quit;
#endif

static unsigned crcTable[256];static int crcReady;
unsigned elevationCrc32(const unsigned char *data,unsigned bytes){
 if(!crcReady){for(unsigned n=0;n<256;n++){unsigned c=n;for(int k=0;k<8;k++)c=c&1?0xedb88320u^(c>>1):c>>1;crcTable[n]=c;}crcReady=1;}
 unsigned c=0xffffffffu;for(unsigned i=0;i<bytes;i++)c=crcTable[(c^data[i])&255]^(c>>8);
 return c^0xffffffffu;
}
static unsigned le16(const unsigned char *p){return p[0]|p[1]<<8;}
int elevationTileValid(const unsigned char *f,unsigned bytes,int i,int j){
 if(!f||bytes!=ELEV_HEADER+ELEV_PAYLOAD||memcmp(f,"AZEL",4))return 0;
 unsigned crc=f[12]|f[13]<<8|f[14]<<16|(unsigned)f[15]<<24;
 return le16(f+4)==1&&le16(f+6)==(unsigned)i&&le16(f+8)==(unsigned)j&&le16(f+10)==ELEV_SAMPLES&&
        elevationCrc32(f+ELEV_HEADER,ELEV_PAYLOAD)==crc;
}
static int readTile(int i,int j,unsigned char *dst){
 char path[200];snprintf(path,sizeof(path),"%s/e_%d_%d.bin",rootPath,i,j);
 FILE *f=fopen(path,"rb");if(!f)return 0;
 size_t n=fread(dst,1,ELEV_HEADER+ELEV_PAYLOAD,f);int extra=fgetc(f);fclose(f);
 return n==ELEV_HEADER+ELEV_PAYLOAD&&extra==EOF&&elevationTileValid(dst,(unsigned)n,i,j);
}
#ifdef __3DS__
static void diskWorker(void *unused){
 (void)unused;
 for(;;){
  LightEvent_Wait(&wake);if(__atomic_load_n(&quit,__ATOMIC_ACQUIRE))return;
  Slot *s=&slots[jobSlot];jobValid=readTile(s->i,s->j,s->file);
  __atomic_store_n(&jobState,2,__ATOMIC_RELEASE);
 }
}
#endif

int streamedElevation(float e,float n,float *height){
 if(!enabled)return 0;
 float fx=(e-ELEV_E0)*(1/ELEV_SPACING),fy=(n-ELEV_N0)*(1/ELEV_SPACING);
 if(!(fx>=0&&fy>=0))return 0;                /* also rejects NaN */
 int ix=(int)fx,iy=(int)fy,ti=ix>>8,tj=iy>>8;
 if(ti>=ELEV_TX||tj>=ELEV_TY)return 0;
 int slot=table[tj][ti];if(!slot)return 0;
 const int16_t *d=(const int16_t*)(slots[slot-1].file+ELEV_HEADER);
 float x=fx-ti*ELEV_TILE,y=fy-tj*ELEV_TILE;int cx=(int)x,cy=(int)y;
 if(cx>ELEV_TILE-1)cx=ELEV_TILE-1;
 if(cy>ELEV_TILE-1)cy=ELEV_TILE-1;
 float ax=x-cx,ay=y-cy;const int16_t *p=d+cy*ELEV_SAMPLES+cx;
 *height=((1-ax)*p[0]+ax*p[1])*(1-ay)+((1-ax)*p[ELEV_SAMPLES]+ax*p[ELEV_SAMPLES+1])*ay;
 return 1;
}

static float tileDistance(int i,int j,Vec p){
 float w=ELEV_E0+i*ELEV_TILE_METRES,s=ELEV_N0+j*ELEV_TILE_METRES;
 float dx=fmaxf(fmaxf(w-p.x,p.x-(w+ELEV_TILE_METRES)),0),dy=fmaxf(fmaxf(s-p.y,p.y-(s+ELEV_TILE_METRES)),0);
 return fmaxf(dx,dy);
}
static void tileRange(Vec p,float radius,int *i0,int *i1,int *j0,int *j1){
 *i0=(int)floorf((p.x-radius-ELEV_E0)/ELEV_TILE_METRES);*i1=(int)floorf((p.x+radius-ELEV_E0)/ELEV_TILE_METRES);
 *j0=(int)floorf((p.y-radius-ELEV_N0)/ELEV_TILE_METRES);*j1=(int)floorf((p.y+radius-ELEV_N0)/ELEV_TILE_METRES);
 *i0=*i0<0?0:*i0;*j0=*j0<0?0:*j0;
 *i1=*i1>ELEV_TX-1?ELEV_TX-1:*i1;*j1=*j1>ELEV_TY-1?ELEV_TY-1:*j1;
}
static int findTile(int i,int j){for(int s=0;s<ELEV_SLOTS;s++)if(slots[s].state!=SLOT_EMPTY&&slots[s].i==i&&slots[s].j==j)return s;return -1;}
static int freeSlot(void){for(int s=0;s<ELEV_SLOTS;s++)if(slots[s].state==SLOT_EMPTY)return s;return -1;}
static void finishJob(void){
 Slot *s=&slots[jobSlot];
 if(jobValid)s->state=SLOT_PENDING;
 else{failed[s->j][s->i]=1;failures++;s->state=SLOT_EMPTY;}
 jobState=0;
}
static void pollJob(void){if(__atomic_load_n(&jobState,__ATOMIC_ACQUIRE)==2)finishJob();}
static void waitIdle(void){
#ifdef __3DS__
 while(__atomic_load_n(&jobState,__ATOMIC_ACQUIRE)==1)svcSleepThread(1000000);
#endif
 pollJob();
}
static void startJob(int slot,int i,int j){
 slots[slot].i=i;slots[slot].j=j;slots[slot].state=SLOT_LOADING;jobSlot=slot;
#ifdef __3DS__
 __atomic_store_n(&jobState,1,__ATOMIC_RELEASE);LightEvent_Signal(&wake);
#else
 jobValid=readTile(i,j,slots[slot].file);jobState=2;
#endif
}

int initTerrainStream(const char *root){
 freeTerrainStream();
 snprintf(rootPath,sizeof(rootPath),"%s",root?root:ELEV_FOLDER);
 /* Absent dataset: stay disabled and use the embedded 500 m base. */
 char probe[200];snprintf(probe,sizeof(probe),"%s/e_30_15.bin",rootPath);
 FILE *f=fopen(probe,"rb");if(!f)return 0;fclose(f);
 for(int s=0;s<ELEV_SLOTS;s++){
  slots[s].file=malloc(ELEV_HEADER+ELEV_PAYLOAD);
  if(!slots[s].file){freeTerrainStream();return 0;}
  slots[s].state=SLOT_EMPTY;
 }
 memset(table,0,sizeof(table));memset(failed,0,sizeof(failed));failures=0;jobState=0;
#ifdef __3DS__
 quit=0;LightEvent_Init(&wake,RESET_ONESHOT);
 worker=threadCreate(diskWorker,NULL,8192,0x38,-2,false);
 if(!worker){freeTerrainStream();return 0;}
#endif
 enabled=1;generation++;resets++;return 1;
}
void freeTerrainStream(void){
#ifdef __3DS__
 if(worker){__atomic_store_n(&quit,1,__ATOMIC_RELEASE);LightEvent_Signal(&wake);threadJoin(worker,U64_MAX);threadFree(worker);worker=NULL;}
#endif
 for(int s=0;s<ELEV_SLOTS;s++){free(slots[s].file);slots[s].file=NULL;slots[s].state=SLOT_EMPTY;}
 memset(table,0,sizeof(table));if(enabled)generation++;resets++;enabled=0;jobState=0;
}

int terrainStreamPublish(Vec pos){
 if(!enabled)return 0;
 pollJob();int changed=0;
 for(int s=0;s<ELEV_SLOTS;s++){
  Slot *t=&slots[s];
  if(t->state==SLOT_PENDING){table[t->j][t->i]=(unsigned char)(s+1);t->state=SLOT_PUBLISHED;changed=1;tileVersion[t->j][t->i]++;}
  else if(t->state==SLOT_PUBLISHED&&tileDistance(t->i,t->j,pos)>ELEV_KEEP_RADIUS){table[t->j][t->i]=0;t->state=SLOT_EMPTY;changed=1;tileVersion[t->j][t->i]++;}
 }
 if(changed)generation++;
 return changed;
}
int terrainStreamUpdate(Vec pos){
 if(!enabled||!isfinite(pos.x)||!isfinite(pos.y))return 0;
 pollJob();int changed=0,i0,i1,j0,j1;
 /* Nearby terrain must never be the 500 m stand-in under the aircraft
    (teleports, new flights, loaded saves): fetch it now. */
 tileRange(pos,ELEV_BLOCK_RADIUS,&i0,&i1,&j0,&j1);
 int missing=0;
 for(int j=j0;j<=j1;j++)for(int i=i0;i<=i1;i++)if(!table[j][i]&&!failed[j][i])missing=1;
 if(missing){
  waitIdle();terrainStreamPublish(pos); /* frees distant slots first */
  for(int j=j0;j<=j1;j++)for(int i=i0;i<=i1;i++){
   if(table[j][i]||failed[j][i]||findTile(i,j)>=0)continue;
   int s=freeSlot();if(s<0)break;
   slots[s].i=i;slots[s].j=j;
   syncReads++;if(readTile(i,j,slots[s].file))slots[s].state=SLOT_PENDING;else{failed[j][i]=1;failures++;}
  }
  changed=terrainStreamPublish(pos);
 }
 /* Background: nearest missing tile inside the request radius. */
 if(jobState==0){
  tileRange(pos,ELEV_NEED_RADIUS,&i0,&i1,&j0,&j1);
  int bi=-1,bj=-1;float best=1e30f;
  for(int j=j0;j<=j1;j++)for(int i=i0;i<=i1;i++){
   if(table[j][i]||failed[j][i]||findTile(i,j)>=0)continue;
   float d=tileDistance(i,j,pos);if(d<best){best=d;bi=i;bj=j;}
  }
  int s=bi>=0?freeSlot():-1;
  if(s>=0)startJob(s,bi,bj);
 }
 return changed;
}
int terrainStreamPending(void){
 if(!enabled)return 0;
 pollJob();for(int s=0;s<ELEV_SLOTS;s++)if(slots[s].state==SLOT_PENDING)return 1;
 return 0;
}
unsigned terrainStreamGeneration(void){return generation;}
unsigned terrainStreamRegionVersion(float e0,float n0,float e1,float n1){
 int i0=(int)floorf((e0-ELEV_E0)/ELEV_TILE_METRES),i1=(int)floorf((e1-ELEV_E0)/ELEV_TILE_METRES);
 int j0=(int)floorf((n0-ELEV_N0)/ELEV_TILE_METRES),j1=(int)floorf((n1-ELEV_N0)/ELEV_TILE_METRES);
 i0=i0<0?0:i0;j0=j0<0?0:j0;i1=i1>ELEV_TX-1?ELEV_TX-1:i1;j1=j1>ELEV_TY-1?ELEV_TY-1:j1;
 unsigned v=resets*2654435761u;
 for(int j=j0;j<=j1;j++)for(int i=i0;i<=i1;i++)v+=tileVersion[j][i];
 return v;
}
int terrainStreamResident(void){int n=0;for(int s=0;s<ELEV_SLOTS;s++)n+=slots[s].state==SLOT_PUBLISHED;return n;}
int terrainStreamFailures(void){return failures;}
int terrainStreamSyncReads(void){return syncReads;}
