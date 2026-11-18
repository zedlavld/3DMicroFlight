/* SD reads belong to a low-priority worker. Only the render thread owns GPU
   allocation, eviction and incremental uploads. Published job state uses ARM
   acquire/release atomics; one immutable job and one staging buffer at a time. */
#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "scenery_stream.h"
#include "corridor_index_bin.h"
typedef struct {StreamKey key;C3D_Tex texture;int state;unsigned used,failedAt;} Slot;
static Slot slots[STREAM_SLOTS];static unsigned tick,resident;static int enabled,blocked,ioFailures;
static Thread worker;static LightEvent wake;static unsigned char *staging;
static int blockCounts[3],poolInfo[4],poolMode,boost;
/* Linear memory left free after the pool: world rebuilds, citro2d/citro3d
   and screenshots still allocate from it. */
#define STREAM_LINEAR_RESERVE (4u*1024u*1024u)
static int jobState,quit,jobSlot,jobValid;static StreamKey jobKey;static unsigned uploaded;
/* v35.03: tile read diagnostics. A failed read is retried twice before the slot is marked bad;
   stage 1 open, 2 short header, 3 header mismatch, 4 short payload, 5 file longer than expected,
   6 (v35.08) the tile's folder itself cannot be opened, so the fault is in the folder above it. */
static int ioRetried,ioRecovered,ioRequeued,lastStage,lastErrno,lastGot;static StreamKey lastKey;
static int readTile(const char *path,StreamKey key,int *stage,int *err,int *got){
 unsigned bytes=streamTileBytes(key);unsigned char header[32];*got=0;
 errno=0;FILE *f=fopen(path,"rb");
 if(!f){*stage=1;*err=errno;char dir[160];snprintf(dir,sizeof(dir),"%s",path);char *slash=strrchr(dir,'/');
  if(slash){*slash=0;struct stat st;if(stat(dir,&st)!=0)*stage=6;}return 0;}
 int n=fread(header,1,32,f);*got=n;
 if(n!=32){*stage=2;*err=errno;fclose(f);return 0;}
 if(!streamHeaderValid(header,32,key)){*stage=3;*err=0;fclose(f);return 0;}
 n=fread(staging,1,bytes,f);*got=n;
 if(n!=(int)bytes){*stage=4;*err=errno;fclose(f);return 0;}
 if(fgetc(f)!=EOF){*stage=5;*err=0;fclose(f);return 0;}
 fclose(f);return 1;
}
/* jobState: 0 idle, 1 requested/reading, 2 complete. Worker never changes slots. */
static void diskWorker(void *unused){
 (void)unused;
 for(;;){LightEvent_Wait(&wake);if(__atomic_load_n(&quit,__ATOMIC_ACQUIRE))return;
  /* v35.07: tiles live in 16 x 16 subfolders (stream32/c-7_3/c_-100_50.cft). One flat folder of
     31,000 long names used ~63,700 FAT32 entries; the 3DS failed to open files past ~60,000
     (FS 0xC8A04573) although Windows and Azahar read them. Floor division for negative keys. */
  StreamKey key=jobKey;char path[160];char t=key.tier?'c':'f';int bx=key.x>=0?key.x/16:-((15-key.x)/16),by=key.y>=0?key.y/16:-((15-key.y)/16);
  snprintf(path,sizeof(path),"sdmc:/3ds/CanyonFlight/scenery/stream32/%c%d_%d/%c_%d_%d.cft",t,bx,by,t,key.x,key.y);
  int stage=0,err=0,got=0,valid=readTile(path,key,&stage,&err,&got);
  for(int attempt=0;!valid&&attempt<2&&!__atomic_load_n(&quit,__ATOMIC_ACQUIRE);attempt++){
   int s0=stage,e0=err,g0=got;lastStage=s0;lastErrno=e0;lastGot=g0;lastKey=key;ioRetried++;
   svcSleepThread(attempt?400000000LL:100000000LL);valid=readTile(path,key,&stage,&err,&got);if(valid)ioRecovered++; /* v35.06: 100 ms, then 400 ms */
  }
  if(!valid){lastStage=stage;lastErrno=err;lastGot=got;lastKey=key;}
  jobValid=valid;__atomic_store_n(&jobState,2,__ATOMIC_RELEASE);
 }
}
static int lookup(StreamKey key){for(int i=0;i<STREAM_SLOTS;i++)if(slots[i].state&&streamKeyEqual(slots[i].key,key))return i;return -1;}
static int slotTier(int i){return i>=STREAM_FINE_SLOTS;}
/* Evicting forgets the tile but keeps the slot's texture memory. */
static void evict(int i){memset(&slots[i].key,0,sizeof(slots[i].key));slots[i].state=0;slots[i].used=0;}
static void releaseTexture(int i){if(slots[i].texture.data){resident-=streamTileBytes((StreamKey){slotTier(i),0,0});C3D_TexDelete(&slots[i].texture);memset(&slots[i].texture,0,sizeof(slots[i].texture));}}
static int allocateTexture(int i){
 if(slots[i].texture.data)return 1;
 int size=slotTier(i)?512:1024;if(!C3D_TexInitMipmap(&slots[i].texture,size,size,GPU_ETC1))return 0;
 if(C3D_TexCalcTotalSize(slots[i].texture.size,slots[i].texture.maxLevel)!=streamTileBytes((StreamKey){slotTier(i),0,0})){C3D_TexDelete(&slots[i].texture);memset(&slots[i].texture,0,sizeof(slots[i].texture));return 0;}
 resident+=streamTileBytes((StreamKey){slotTier(i),0,0});
 C3D_TexSetFilter(&slots[i].texture,GPU_LINEAR,GPU_LINEAR);C3D_TexSetFilterMipmap(&slots[i].texture,GPU_LINEAR);C3D_TexSetWrap(&slots[i].texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
 return 1;
}
static int oldest(unsigned pinned,int tier){int result=-1;for(int i=0;i<STREAM_SLOTS;i++)if(slotTier(i)==tier&&slots[i].state&&!(pinned&(1u<<i))&&slots[i].state!=1&&(result<0||slots[i].used<slots[result].used))result=i;return result;}
int initSceneryStream(void){
 FILE *f=fopen("sdmc:/3ds/CanyonFlight/scenery/stream32/manifest.json","rb");if(!f)return 0;
 char marker[1024]={0};size_t n=fread(marker,1,sizeof(marker)-1,f);fclose(f);
 if(n==sizeof(marker)-1||!strstr(marker,"\"version\": 17")||!strstr(marker,"\"complete\": true"))return 0;
 quit=jobState=jobValid=blocked=ioFailures=ioRetried=ioRecovered=ioRequeued=lastStage=0;tick=resident=0;
 staging=malloc(streamTileBytes((StreamKey){0,0,0}));if(!staging)return 0;LightEvent_Init(&wake,RESET_ONESHOT);
 worker=threadCreate(diskWorker,NULL,16384,0x38,-2,false);if(!worker){free(staging);staging=NULL;return 0;}
 /* Whole pool now, while the linear heap is unfragmented, but only what fits
    above the reserve: a window's four coarse and four fine tiles first, then
    spare fine slots, then spare coarse. Slots left without memory are unused. */
 /* A window's four coarse tiles first, then the fine slots, then spare coarse. */
 int order[STREAM_SLOTS],placed=0;
 for(int i=0;i<4;i++)order[placed++]=STREAM_FINE_SLOTS+i;
 for(int i=0;i<STREAM_FINE_SLOTS;i++)order[placed++]=i;
 for(int i=STREAM_FINE_SLOTS+4;i<STREAM_SLOTS;i++)order[placed++]=i;
 poolInfo[2]=(int)(linearSpaceFree()/1024);
 for(int k=0;k<STREAM_SLOTS;k++){int i=order[k];
  if(linearSpaceFree()<streamTileBytes((StreamKey){slotTier(i),0,0})+STREAM_LINEAR_RESERVE)continue;
  if(allocateTexture(i))poolInfo[slotTier(i)]++;
 }
 poolInfo[3]=(int)(linearSpaceFree()/1024);poolMode=1;
 enabled=1;return 1;
}
void freeSceneryStream(void){
 if(worker){__atomic_store_n(&quit,1,__ATOMIC_RELEASE);LightEvent_Signal(&wake);threadJoin(worker,U64_MAX);threadFree(worker);worker=NULL;}
 for(int i=0;i<STREAM_SLOTS;i++){evict(i);releaseTexture(i);}
 free(staging);staging=NULL;enabled=0;poolMode=0;memset(poolInfo,0,sizeof(poolInfo));
}
static int requestWindow(StreamWindow *w,unsigned pinned){
 unsigned protect=pinned;int missing=-1;
 int detailed=1;for(int i=0;i<4;i++)if(!streamCatalogHas(corridor_index_bin,corridor_index_bin_size,w->keys[i]))detailed=0;
 for(int j=0;j<(detailed?8:4);j++){int i=(j+4)%8;
  if(!streamCatalogHas(corridor_index_bin,corridor_index_bin_size,w->keys[i]))return 0;
  int slot=lookup(w->keys[i]);if(slot>=0){protect|=1u<<slot;slots[slot].used=tick;if(slots[slot].state==3){if(tick-slots[slot].failedAt<180)return 0;evict(slot);ioRequeued++;if(missing<0)missing=i;}} /* v35.06: a failed tile is asked for again after about 3 s */else if(missing<0)missing=i;
 }
 if(missing<0)return 0;
 StreamKey key=w->keys[missing];
 int slot=-1;for(int i=0;i<STREAM_SLOTS;i++)if(slotTier(i)==key.tier&&!slots[i].state&&(!poolMode||slots[i].texture.data)){slot=i;break;}
 if(slot<0){slot=oldest(protect,key.tier);if(slot<0){blocked=1;blockCounts[1]++;return 0;}evict(slot);}
 slots[slot].key=key;slots[slot].state=1;slots[slot].used=tick;
 jobKey=key;jobSlot=slot;uploaded=0;__atomic_store_n(&jobState,1,__ATOMIC_RELEASE);LightEvent_Signal(&wake);return 1;
}
void updateSceneryStream(Vec pos,Vec velocity,unsigned pinned){
 if(!enabled)return;
 tick++;blocked=0;
 int state=__atomic_load_n(&jobState,__ATOMIC_ACQUIRE);
 if(state==2){
  Slot *slot=&slots[jobSlot];unsigned bytes=streamTileBytes(jobKey);
  /* Only a slot whose start-up allocation failed has no texture here. */
  if(jobValid&&!allocateTexture(jobSlot)){blocked=1;blockCounts[2]++;return;}
  if(jobValid){unsigned amount=bytes-uploaded;if(!boost&&amount>STREAM_UPLOAD_BUDGET)amount=STREAM_UPLOAD_BUDGET;
   memcpy((unsigned char*)slot->texture.data+uploaded,staging+uploaded,amount);GSPGPU_FlushDataCache((unsigned char*)slot->texture.data+uploaded,amount);uploaded+=amount;
   if(uploaded<bytes)return;
   slot->state=2;
  }else{slot->state=3;slot->failedAt=tick;ioFailures++;}
  __atomic_store_n(&jobState,0,__ATOMIC_RELEASE);
 }else if(state)return;
 StreamWindow current;streamWindow(pos,&current);if(requestWindow(&current,pinned))return;
 StreamWindow available;if(!readySceneryWindow(pos,&available))return;
 /* Current window is protected while fetching eight seconds ahead. */
 unsigned keep=pinned|sceneryWindowPins(&available);
 Vec ahead={pos.x+velocity.x*8,pos.y+velocity.y*8,pos.z};StreamWindow future;streamWindow(ahead,&future);requestWindow(&future,keep);blocked=0; /* Prefetch pressure never invalidates a usable scene. */
}
int readySceneryWindow(Vec pos,StreamWindow *w){
 streamWindow(pos,w);w->detailed=0;
 for(int i=4;i<8;i++){int slot=lookup(w->keys[i]);if(slot<0||slots[slot].state!=2)return 0;w->slots[i]=slot;}
 int fineSlots[4];for(int i=0;i<4;i++){fineSlots[i]=lookup(w->keys[i]);if(fineSlots[i]<0||slots[fineSlots[i]].state!=2)return 1;}
 for(int i=0;i<4;i++)w->slots[i]=fineSlots[i];
 w->detailed=1;return 1;
}
unsigned sceneryWindowPins(const StreamWindow *w){unsigned pins=0;for(int i=0;i<8;i++)if(w->slots[i]>=0)pins|=1u<<w->slots[i];return pins;}
C3D_Tex *sceneryStreamTexture(int slot){return slot>=0&&slot<STREAM_SLOTS&&slots[slot].state==2?&slots[slot].texture:NULL;}
int sceneryStreamBlocked(void){return blocked;}
void sceneryStreamBlockCounts(int counts[3]){for(int i=0;i<3;i++)counts[i]=blockCounts[i];}
void sceneryStreamPoolInfo(int info[4]){for(int i=0;i<4;i++)info[i]=poolInfo[i];}
void sceneryStreamBoost(int on){boost=on;}
int sceneryWindowCatalogued(Vec pos){StreamWindow w;streamWindow(pos,&w);for(int i=4;i<8;i++)if(!streamCatalogHas(corridor_index_bin,corridor_index_bin_size,w.keys[i]))return 0;return 1;}
int sceneryWindowCanDetail(Vec pos){StreamWindow w;streamWindow(pos,&w);for(int i=0;i<4;i++)if(!streamCatalogHas(corridor_index_bin,corridor_index_bin_size,w.keys[i]))return 0;return 1;}
int sceneryStreamDiagnostics(char *out,int size){
 if(!ioRetried&&!ioFailures)return 0;
 return snprintf(out,size,"tile reads: %d failed, %d retried (%d recovered), %d asked again, last %c_%d_%d stage %d code %08X got %d",
  ioFailures,ioRetried,ioRecovered,ioRequeued,lastKey.tier?'c':'f',lastKey.x,lastKey.y,lastStage,(unsigned)lastErrno,lastGot);
}
const char *sceneryStreamStatus(void){static char status[64];if(!enabled)return "REGIONAL PACK";int ready=0;for(int i=0;i<STREAM_SLOTS;i++)ready+=slots[i].state==2;snprintf(status,sizeof(status),"SD %d/%d %s%s",ready,STREAM_SLOTS,__atomic_load_n(&jobState,__ATOMIC_ACQUIRE)==1?"READ":(__atomic_load_n(&jobState,__ATOMIC_ACQUIRE)==2?"UPLOAD":"READY"),ioFailures?" / TILE ERROR":"");return status;}


