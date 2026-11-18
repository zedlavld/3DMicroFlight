/* Host audit: fly the hands-off autotest cruise and, every half second,
   count partly visible world triangles that the v31.2 shader guard distorts
   (a vertex more than guard.y m in front whose |clip x| or |clip y| exceeds
   G*w). Reports where on the 400x240 screen the distorted triangles show.
   usage: audit <start index> <phase> <exterior> <G> <seconds> */
#include "scenery.h"
#include "camera.h"
#include <stdio.h>
#include <stdlib.h>
static WorldVertex v[SCENERY_PART_CAPACITY];
int main(int argc,char **argv){
 int k=argc>1?atoi(argv[1]):3,phase=argc>2?atoi(argv[2]):PHASE_CRUISE,exterior=argc>3?atoi(argv[3]):1;
 float G=argc>4?(float)atof(argv[4]):4,seconds=argc>5?(float)atof(argv[5]):20;
 Flight s;resetFlightPhase(&s,1,airportStarts[k],phase);
 View view={0};resetView(&view);view.exterior=exterior;
 const float tx=200.f/220,ty=120.f/220;long total=0;
 for(float t=0;t<=seconds;t+=.5f){
  CameraPose cam=makeCamera(&s,&view);long distorted=0;float sx0=1e9,sy0=1e9,sx1=-1e9,sy1=-1e9;
  for(int part=0;part<SCENERY_PARTS;part++){
   int n=buildSceneryPart(&s,v,SCENERY_PART_CAPACITY,part);
   for(int i=0;i+2<n;i+=3){
    int onScreen=0,clamped=0;float px[3],py[3];int front=0;
    for(int j=0;j<3;j++){Vec c=projectCamera(&cam,v[i+j].p);if(c.z<=0)continue;
     float nx=c.x/c.z/tx,ny=c.y/c.z/ty;px[front]=nx;py[front++]=ny;
     if(fabsf(nx)<=1&&fabsf(ny)<=1)onScreen=1;
     if(c.z>8&&(fabsf(nx)>G||fabsf(ny)>G))clamped=1;}
    if(onScreen&&clamped){distorted++;
     for(int j=0;j<front;j++)if(fabsf(px[j])<=1&&fabsf(py[j])<=1){
      float x=(px[j]+1)*200,y=(1-py[j])*120;sx0=fminf(sx0,x);sy0=fminf(sy0,y);sx1=fmaxf(sx1,x);sy1=fmaxf(sy1,y);}}
   }
  }
  total+=distorted;
  if(distorted)printf("t=%4.1f distorted %ld, visible vertices in x %.0f..%.0f y %.0f..%.0f\n",t,distorted,sx0,sx1,sy0,sy1);
  for(int step=0;step<15;step++)stepFlight(&s,1.f/30,0,0,0);
 }
 printf("%s %s G=%.0f: %ld distorted triangle-frames over %.0f s\n",airports[airportStarts[k]].id,exterior?"ext":"cockpit",G,total,seconds);
 return 0;
}
