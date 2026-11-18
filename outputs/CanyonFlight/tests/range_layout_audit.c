/* Host audit: replicate the renderer's world upload (8-cell block sort,
   photo clipping per material, 768-vertex ranges) for the hands-off cruise
   and report which range / position within the range covers a pixel, and
   whether that range passes the per-range visibility test.
   usage: layout <start index> <phase> <px> <py> <t> <detailed 0/1> */
#include "scenery.h"
#include "imagery.h"
#include "camera.h"
#include "stream_plan.h"
#include <stdio.h>
#include <stdlib.h>
#define CAP 90000
#define RANGES 3072
#define RANGE_VERTICES 768
#define BLOCK_BUCKETS 96
typedef struct {int first,count,tile;Vec center;float radius;} Range;
static ImageryVertex vbo[CAP];static Range ranges[RANGES];static int vcount,rcount;
static WorldVertex worldPart[SCENERY_PART_CAPACITY],sortedPart[SCENERY_PART_CAPACITY];static signed char sortedMaterial[SCENERY_PART_CAPACITY];
static int blockStart[BLOCK_BUCKETS+1];static ImageryVertex photoPart[SCENERY_PART_CAPACITY*2];
static void append(const ImageryVertex *v,int count,int tile){
 if(!count)return;
 int first=vcount;for(int i=0;i<count;i++)vbo[vcount++]=v[i];
 for(int start=0;start<count;start+=RANGE_VERTICES){
  int length=count-start;if(length>RANGE_VERTICES)length=RANGE_VERTICES;
  Vec lo=v[start].p,hi=lo;
  for(int i=start+1;i<start+length;i++){Vec p=v[i].p;lo.x=fminf(lo.x,p.x);lo.y=fminf(lo.y,p.y);lo.z=fminf(lo.z,p.z);hi.x=fmaxf(hi.x,p.x);hi.y=fmaxf(hi.y,p.y);hi.z=fmaxf(hi.z,p.z);}
  Vec c={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f};float dx=hi.x-c.x,dy=hi.y-c.y,dz=hi.z-c.z;
  ranges[rcount++]=(Range){first+start,length,tile,c,sqrtf(dx*dx+dy*dy+dz*dz)};
 }
}
static int sortIntoBlocks(int count,int part){
 int tris=count/3;float cell=part<32?32.f*(1<<(part/4)):128.f,size=cell*8;
 static int keys[BLOCK_BUCKETS];static unsigned char bucketOf[SCENERY_PART_CAPACITY/3];int buckets=0,sizes[BLOCK_BUCKETS]={0};
 for(int t=0;t<tris;t++){
  const WorldVertex *v=worldPart+t*3;
  int key=((int)floorf((v[0].p.x+v[1].p.x+v[2].p.x)/(3*size))&0xffff)<<16|((int)floorf((v[0].p.y+v[1].p.y+v[2].p.y)/(3*size))&0xffff);
  int b=0;while(b<buckets&&keys[b]!=key)b++;
  if(b==buckets){if(buckets<BLOCK_BUCKETS)keys[buckets++]=key;else b=BLOCK_BUCKETS-1;}
  bucketOf[t]=(unsigned char)b;sizes[b]++;
 }
 blockStart[0]=0;for(int b=0;b<buckets;b++)blockStart[b+1]=blockStart[b]+sizes[b]*3;
 int fill[BLOCK_BUCKETS];for(int b=0;b<buckets;b++)fill[b]=blockStart[b];
 for(int t=0;t<tris;t++){int at=fill[bucketOf[t]];fill[bucketOf[t]]+=3;
  for(int j=0;j<3;j++){sortedPart[at+j]=worldPart[t*3+j];sortedMaterial[at+j]=(signed char)sceneryVertexMaterial(t*3+j);}}
 return buckets;
}
int main(int argc,char **argv){
 int k=atoi(argv[1]),phase=atoi(argv[2]);float PX=atof(argv[3]),PY=atof(argv[4]),T=atof(argv[5]);int detailed=atoi(argv[6]);
 Flight s;resetFlightPhase(&s,1,airportStarts[k],phase);
 for(float t=0;t<T;t+=1.f/30)stepFlight(&s,1.f/30,0,0,0);
 Flight build=s;build.pos.x=floorf(s.pos.x/256)*256;build.pos.y=floorf(s.pos.y/256)*256;
 StreamWindow w;streamWindow(s.pos,&w);setImageryWindows(w.fine,w.coarse);setImageryFineEnabled(detailed);
 printf("pos %.0f %.0f; fine window %.0f %.0f, coarse %.0f %.0f, detailed %d\n",s.pos.x,s.pos.y,w.fine.x,w.fine.y,w.coarse.x,w.coarse.y,detailed);
 for(int part=0;part<SCENERY_PARTS;part++){
  int count=buildSceneryPart(&build,worldPart,SCENERY_PART_CAPACITY,part);
  int blocks=count>0?sortIntoBlocks(count,part):0;
  int photo=part<32&&count>0;
  if(photo){Vec lo=worldPart[0].p,hi=lo;for(int i=1;i<count;i++){Vec p=worldPart[i].p;lo.x=fminf(lo.x,p.x);lo.y=fminf(lo.y,p.y);hi.x=fmaxf(hi.x,p.x);hi.y=fmaxf(hi.y,p.y);}
   Vec o=getImageryCoarseOrigin();photo=!(hi.x<=o.x-8192||lo.x>=o.x+8192||hi.y<=o.y-8192||lo.y>=o.y+8192);}
  for(int block=0;block<blocks;block++){int b0=blockStart[block],b1=blockStart[block+1];
   if(photo){
    for(int material=-1;material<IMAGERY_TILES;material++){int written=0,first,last;imageryMaterialRegions(material,&first,&last);
     for(int i=b0;i<b1;i+=3)for(int region=first;region<last;region++){int n=clipImageryTriangle(sortedPart+i,region,photoPart+written,SCENERY_PART_CAPACITY*2-written,build.pos);if(n<0){printf("clip overflow\n");return 1;}written+=n;}
     int r0=rcount;append(photoPart,written,material);
     (void)r0;
    }
   }else{
    for(int i=b0;i<b1;i++){WorldVertex p=sortedPart[i];photoPart[i]=(ImageryVertex){p.p,{0,0},{p.color[0],p.color[1],p.color[2],1}};}
    for(int first=b0;first<b1;){int tile=sortedMaterial[first],last=first+3;while(last<b1&&sortedMaterial[last]==tile)last+=3;append(photoPart+first,last-first,tile);first=last;}
   }
  }
  if(part==14)printf("after part 14: %d vertices, %d ranges\n",vcount,rcount);
 }
 printf("scene: %d vertices, %d ranges\n",vcount,rcount);
 View view={0};resetView(&view);view.exterior=1;CameraPose cam=makeCamera(&s,&view);
 const float tx=200.f/220,ty=120.f/220;float qx=PX/200-1,qy=1-PY/120;
 int visibleRanges=0;for(int r=0;r<rcount;r++)visibleRanges+=cameraSphereVisible(&cam,ranges[r].center,ranges[r].radius,0,40);
 printf("visible ranges %d\n",visibleRanges);
 for(int r=0;r<rcount;r++)for(int i=0;i+2<ranges[r].count;i+=3){
  float x[3],y[3];int ok=1;
  for(int j=0;j<3;j++){Vec c=projectCamera(&cam,vbo[ranges[r].first+i+j].p);if(c.z<=0){ok=0;break;}x[j]=c.x/c.z/tx;y[j]=c.y/c.z/ty;}
  if(!ok)continue;
  float d0=(x[1]-x[0])*(qy-y[0])-(y[1]-y[0])*(qx-x[0]),d1=(x[2]-x[1])*(qy-y[1])-(y[2]-y[1])*(qx-x[1]),d2=(x[0]-x[2])*(qy-y[2])-(y[0]-y[2])*(qx-x[2]);
  if(!((d0>=0&&d1>=0&&d2>=0)||(d0<=0&&d1<=0&&d2<=0)))continue;
  printf("range %d (tile %d, first %d, count %d, visible %d): triangle %d of %d [",r,ranges[r].tile,ranges[r].first,ranges[r].count,cameraSphereVisible(&cam,ranges[r].center,ranges[r].radius,0,40),i/3,ranges[r].count/3);
  for(int j=0;j<3;j++){Vec p=vbo[ranges[r].first+i+j].p;printf(" (%.1f,%.1f,%.1f)",p.x,p.y,p.z);}
  printf(" ] screen");for(int j=0;j<3;j++)printf(" (%.1f,%.1f w%.0f)",(x[j]+1)*200,(1-y[j])*120,projectCamera(&cam,vbo[ranges[r].first+i+j].p).z);
  printf("\n");
 }
 return 0;
}
