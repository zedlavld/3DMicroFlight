/* Host audit: out-of-rasterizer-range triangles that actually reach the GPU,
   i.e. inside draw ranges that pass cameraSphereVisible(), comparing
   (a) today's ranges: consecutive 768-vertex chunks of each part, and
   (b) compact ranges: each part's triangles grouped into spatial blocks.
   "front" = all vertices in front of the near plane (fixable by a vertex-
   shader clamp); "cross" = straddles the near plane (hardware-clipped).
   usage: audit <start index> <phase> <exterior> <block cells> */
#include "scenery.h"
#include "camera.h"
#include "terrain.h"
#include <stdio.h>
#include <stdlib.h>
static WorldVertex v[SCENERY_PART_CAPACITY];
static int order[SCENERY_PART_CAPACITY/3];static long key[SCENERY_PART_CAPACITY/3];
static CameraPose cam;static float nearPlane;
static const float F=220,LIMIT=1800;
/* 0 ok, 1 out-of-range wholly in front, 2 crosses near plane with an out-of-range part, 3 wholly behind */
static int classify(const WorldVertex *t){
 int front=0,behind=0,bad=0;
 for(int j=0;j<3;j++){Vec c=projectCamera(&cam,t[j].p);
  if(c.z<nearPlane){behind++;continue;}front++;
  if(fabsf(F*c.x/c.z)>LIMIT||fabsf(F*c.y/c.z)>LIMIT)bad=1;}
 if(!front)return 3;
 if(behind)return 2; /* conservative: crossing triangles may clip far out */
 return bad?1:0;
}
static int sphereOf(const WorldVertex *t,int n,Vec *center,float *radius){
 Vec lo=t[0].p,hi=lo;for(int i=1;i<n;i++){Vec p=t[i].p;lo.x=fminf(lo.x,p.x);lo.y=fminf(lo.y,p.y);lo.z=fminf(lo.z,p.z);hi.x=fmaxf(hi.x,p.x);hi.y=fmaxf(hi.y,p.y);hi.z=fmaxf(hi.z,p.z);}
 *center=(Vec){(lo.x+hi.x)/2,(lo.y+hi.y)/2,(lo.z+hi.z)/2};float dx=hi.x-center->x,dy=hi.y-center->y,dz=hi.z-center->z;*radius=sqrtf(dx*dx+dy*dy+dz*dz);return 1;
}
int main(int argc,char **argv){
 int k=argc>1?atoi(argv[1]):3,phase=argc>2?atoi(argv[2]):PHASE_CRUISE,exterior=argc>3?atoi(argv[3]):1,blockCells=argc>4?atoi(argv[4]):8;
 Flight s;resetFlightPhase(&s,1,airportStarts[k],phase);
 View view={0};resetView(&view);view.exterior=exterior;cam=makeCamera(&s,&view);nearPlane=exterior?6:2;
 long cur[4]={0},blk[4]={0},curRanges=0,blkRanges=0,curDrawn=0,blkDrawn=0;
 for(int part=0;part<SCENERY_PARTS;part++){
  int n=buildSceneryPart(&s,v,SCENERY_PART_CAPACITY,part),tris=n/3;if(!tris)continue;
  /* (a) current: consecutive 768-vertex chunks */
  for(int start=0;start<n;start+=768){
   int len=n-start<768?n-start:768;Vec c;float r;sphereOf(v+start,len,&c,&r);curRanges++;
   if(!cameraSphereVisible(&cam,c,r,0,32))continue;curDrawn+=len/3;
   for(int i=start;i<start+len;i+=3)cur[classify(v+i)]++;
  }
  /* (b) compact: sort triangles by spatial block, one range per block (<=768 verts) */
  float cell=part<32?32.f*(1<<(part/4)):64.f,size=cell*blockCells;
  for(int t=0;t<tris;t++){Vec p=v[t*3].p;order[t]=t;key[t]=((long)floorf(p.x/size)+100000)*200000+((long)floorf(p.y/size)+100000);}
  for(int a=1;a<tris;a++){int o=order[a];long kk=key[o];int b=a;while(b>0&&key[order[b-1]]>kk){order[b]=order[b-1];b--;}order[b]=o;}
  for(int a=0;a<tris;){
   int b=a;while(b<tris&&key[order[b]]==key[order[a]]&&b-a<256)b++;
   static WorldVertex group[768];int g=0;for(int t=a;t<b;t++)for(int j=0;j<3;j++)group[g++]=v[order[t]*3+j];
   Vec c;float r;sphereOf(group,g,&c,&r);blkRanges++;
   if(cameraSphereVisible(&cam,c,r,0,32)){blkDrawn+=g/3;for(int t=0;t<g;t+=3)blk[classify(group+t)]++;}
   a=b;
  }
 }
 printf("%s %s %.0f m AGL, blocks of %d cells\n",airports[airportStarts[k]].id,exterior?"exterior":"cockpit",s.pos.z-terrain(s.pos.x,s.pos.y),blockCells);
 printf("  current : %4ld ranges, %5ld tris drawn; out-of-range in front %4ld, crossing %4ld\n",curRanges,curDrawn,cur[1],cur[2]);
 printf("  compact : %4ld ranges, %5ld tris drawn; out-of-range in front %4ld, crossing %4ld\n",blkRanges,blkDrawn,blk[1],blk[2]);
 return 0;
}
