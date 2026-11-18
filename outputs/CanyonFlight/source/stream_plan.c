#include <math.h>
#include <stdint.h>
#include <string.h>
#include "stream_plan.h"
int streamKeyEqual(StreamKey a,StreamKey b){return a.tier==b.tier&&a.x==b.x&&a.y==b.y;}
/* v32: ETC1 mip chains, 1024 px to 8 px (fine) and 512 px to 8 px (coarse). */
unsigned streamTileBytes(StreamKey k){return k.tier?174752:699040;}
void streamWindow(Vec p,StreamWindow *w){
 w->detailed=1;
 int fx=(int)floorf(p.x/2048+.5f),fy=(int)floorf(p.y/2048+.5f);
 int cx=(int)floorf(p.x/8192+.5f),cy=(int)floorf(p.y/8192+.5f);
 w->fine=(Vec){fx*2048.f,fy*2048.f,0};w->coarse=(Vec){cx*8192.f,cy*8192.f,0};
 for(int i=0;i<8;i++){int fine=i<4,j=i%4;w->keys[i]=(StreamKey){fine?0:1,(fine?fx:cx)-1+j%2,(fine?fy:cy)-1+j/2};w->slots[i]=-1;}
}
static int compare(StreamKey a,StreamKey b){if(a.tier!=b.tier)return a.tier<b.tier?-1:1;if(a.x!=b.x)return a.x<b.x?-1:1;return a.y==b.y?0:(a.y<b.y?-1:1);}
int streamCatalogHas(const unsigned char *data,unsigned bytes,StreamKey key){
 uint32_t version,count;if(bytes<16||memcmp(data,"CFINDEX5",8))return 0;
 memcpy(&version,data+8,4);memcpy(&count,data+12,4);
 if(version!=1||count>(bytes-16)/12||16+count*12!=bytes)return 0;
 unsigned lo=0,hi=count;while(lo<hi){unsigned mid=lo+(hi-lo)/2;StreamKey found;memcpy(&found,data+16+mid*12,12);int c=compare(found,key);if(!c)return 1;if(c<0)lo=mid+1;else hi=mid;}return 0;
}

int streamHeaderValid(const unsigned char *header,unsigned bytes,StreamKey key){
 int32_t h[6];if(bytes!=32||memcmp(header,"CFSTRM16",8)||key.tier<0||key.tier>1)return 0;
 memcpy(h,header+8,24);return h[0]==key.x&&h[1]==key.y&&h[2]==(key.tier?512:1024)&&h[3]==(key.tier?6:7)&&h[4]==(int)streamTileBytes(key)&&h[5]==0;
}
