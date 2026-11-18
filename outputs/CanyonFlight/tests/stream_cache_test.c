/* Execute the production cache/uploader on ARM, substituting only the OS,
   SD completion and GPU allocation. Does not emulate SD latency or GPU time. */
#include "../source/scenery_stream.c"
static unsigned char gpuMemory[STREAM_SLOTS][699040];
static unsigned char stageMemory[699040];
int checks,failures,firstFailure,maxResident,maxFrameUpload,completedTiles,failureTiles,readyFrames,pressureTransitions;
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
static unsigned flushed;static int failAllocations=3;
Thread threadCreate(void(*e)(void*),void*a,size_t s,int p,int c,bool d){return NULL;}
void threadJoin(Thread t,u64 u){}void threadFree(Thread t){}
void LightEvent_Init(LightEvent*e,int r){}void LightEvent_Signal(LightEvent*e){}void LightEvent_Wait(LightEvent*e){}
void GSPGPU_FlushDataCache(void*p,size_t n){flushed+=n;}
u32 linearSpaceFree(void){return 64u*1024u*1024u;}
bool C3D_TexInitMipmap(C3D_Tex*t,int x,int y,int f){if(failAllocations){failAllocations--;return false;}int i=(int)(((char*)t-(char*)&slots[0].texture)/sizeof(Slot));t->data=gpuMemory[i];t->size=x==1024?699040:174752;t->maxLevel=0;return true;}
void C3D_TexDelete(C3D_Tex*t){t->data=NULL;}
unsigned C3D_TexCalcTotalSize(unsigned s,unsigned l){return s;}
void C3D_TexSetFilter(C3D_Tex*t,int a,int b){}void C3D_TexSetFilterMipmap(C3D_Tex*t,int a){}void C3D_TexSetWrap(C3D_Tex*t,int a,int b){}
static void frame(Vec pos,Vec velocity,unsigned pins,int injectFailure){
 StreamKey pinnedKeys[STREAM_SLOTS];for(int i=0;i<STREAM_SLOTS;i++)pinnedKeys[i]=slots[i].key;
 if(jobState==1){jobValid=!injectFailure;jobState=2;if(injectFailure)failureTiles++;}
 int before=0;for(int i=0;i<STREAM_SLOTS;i++)before+=slots[i].state==2;
 flushed=0;updateSceneryStream(pos,velocity,pins);
 CHECK(flushed<=STREAM_UPLOAD_BUDGET);CHECK(resident<=STREAM_GPU_BUDGET);
 if((int)resident>maxResident)maxResident=resident;if((int)flushed>maxFrameUpload)maxFrameUpload=flushed;
 for(int i=0;i<STREAM_SLOTS;i++)if(pins&(1u<<i)){CHECK(slots[i].state==2);CHECK(streamKeyEqual(pinnedKeys[i],slots[i].key));}
 int after=0;for(int i=0;i<STREAM_SLOTS;i++)after+=slots[i].state==2;if(after>before)completedTiles+=after-before;
}
int main(void){
 unsigned char header[32]="CFSTRM16";int32_t h[6]={-4,12,1024,7,699040,0};memcpy(header+8,h,24);StreamKey key={0,-4,12};
 CHECK(streamHeaderValid(header,32,key));
 for(int i=0;i<32;i++){header[i]^=0x80;CHECK(!streamHeaderValid(header,32,key));header[i]^=0x80;}
 CHECK(!streamHeaderValid(header,31,key));CHECK(!streamHeaderValid(header,32,(StreamKey){1,-4,12}));
 enabled=1;staging=stageMemory;Vec pos={0,0,2000},velocity={0,100,0};StreamWindow active;unsigned pins=0;
 for(int j=0;j<1500;j++)frame(pos,velocity,0,0);
 CHECK(readySceneryWindow(pos,&active)&&active.detailed);pins=sceneryWindowPins(&active);CHECK(pins!=0);
 /* Fly the whole corridor, retaining each scene's pins for 36 frames while
    its replacement builds, just as the renderer does. */
 Vec route[4]={{0,0,2000},{10631,32230,2500},{-32760,122732,2500},{-32768,135168,2500}};
 readyFrames=0;pressureTransitions=0;
 for(int leg=0;leg<3;leg++){
  Vec delta={route[leg+1].x-route[leg].x,route[leg+1].y-route[leg].y,0};float length=hypotf(delta.x,delta.y);velocity=(Vec){delta.x/length*150,delta.y/length*150,0};
  for(int k=0;k<(int)(length/150*60);k++){
   float t=k*150.f/60/length;pos=(Vec){route[leg].x+t*delta.x,route[leg].y+t*delta.y,2500};
   if(blocked){pins=0;pressureTransitions++;}
   frame(pos,velocity,pins,0);StreamWindow next;
   if(readySceneryWindow(pos,&next)){readyFrames++;if(k%36==0){active=next;pins=sceneryWindowPins(&active);}}
  }
 }
 CHECK(readyFrames>10000);
 CHECK(completedTiles>100);
 CHECK(pressureTransitions<100);
 /* Airport jump: release old scene, fill the new one without over-budget
    allocations; then verify reversing direction can reuse its active tiles. */
 pos=route[0];pins=0;for(int k=0;k<1800;k++)frame(pos,(Vec){0,-100,0},0,0);
 CHECK(readySceneryWindow(pos,&active));pins=sceneryWindowPins(&active);
 for(int k=0;k<500;k++)frame(pos,(Vec){0,100,0},pins,0);
 CHECK(readySceneryWindow(pos,&active));
 /* Missing/corrupt SD completion remains a failed entry and is not retried
    every frame. The active scene's pinned tiles remain untouched. */
 pos=route[2];pins=0;int injected=0;
 for(int k=0;k<1800;k++){int bad=jobState==1&&jobKey.tier==0&&!injected;frame(pos,(Vec){0},0,bad);if(bad)injected=1;}
 CHECK(injected&&failures==0);CHECK(failureTiles==1);CHECK(readySceneryWindow(pos,&active)&&!active.detailed);CHECK(jobState==0);
 /* A coarse read failure has no photo substitute and keeps procedural
    rendering available. Clear the simulated flight cache between scenarios. */
 for(int i=0;i<STREAM_SLOTS;i++)evict(i);jobState=0;injected=0;pos=route[0];
 for(int k=0;k<1800;k++){int bad=jobState==1&&jobKey.tier==1&&!injected;frame(pos,(Vec){0},0,bad);if(bad)injected=1;}
 CHECK(injected&&failureTiles==2);CHECK(!readySceneryWindow(pos,&active));CHECK(jobState==0);
 for(int i=0;i<STREAM_SLOTS;i++)evict(i);jobState=0;
 /* Each default eight-kilometre approach start has at least coarse imagery. */
 Vec starts[3]={{-5780,-5577,2400},{5383,26192,3000},{-37966,116657,3000}};
 for(int a=0;a<3;a++){for(int k=0;k<1800;k++)frame(starts[a],(Vec){0},0,0);CHECK(readySceneryWindow(starts[a],&active));}
 return failures;
}
