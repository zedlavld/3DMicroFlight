/* Host audit: partly visible world triangles the v31.2 shader guard would
   distort, i.e. with a vertex more than 8 m in front whose clip x or y
   exceeds G*w. Such a vertex is pulled inward, the triangle no longer covers
   its true screen area, and a hole appears (KPHX cruise, New 3DS, v31.5).
   usage: audit <start index> <phase> <exterior> <G> */
#include "scenery.h"
#include "camera.h"
#include <stdio.h>
#include <stdlib.h>
static WorldVertex v[SCENERY_PART_CAPACITY];
int main(int argc,char **argv){
 int k=argc>1?atoi(argv[1]):3,phase=argc>2?atoi(argv[2]):PHASE_CRUISE,exterior=argc>3?atoi(argv[3]):1;float G=argc>4?(float)atof(argv[4]):4;
 Flight s;resetFlightPhase(&s,1,airportStarts[k],phase);
 View view={0};resetView(&view);view.exterior=exterior;CameraPose cam=makeCamera(&s,&view);
 const float tx=200.f/220,ty=120.f/220; /* clip x,y per unit w at the screen edges */
 long distorted=0;float worst=0;
 for(int part=0;part<SCENERY_PARTS;part++){
  int n=buildSceneryPart(&s,v,SCENERY_PART_CAPACITY,part);
  for(int i=0;i+2<n;i+=3){
   int onScreen=0,clamped=0;
   for(int j=0;j<3;j++){Vec c=projectCamera(&cam,v[i+j].p);if(c.z<=0)continue;
    float nx=c.x/c.z/tx,ny=c.y/c.z/ty;   /* normalised device coordinates */
    if(fabsf(nx)<=1&&fabsf(ny)<=1)onScreen=1;
    if(c.z>8&&(fabsf(nx)>G||fabsf(ny)>G)){clamped=1;worst=fmaxf(worst,fmaxf(fabsf(nx),fabsf(ny)));}}
   if(onScreen&&clamped)distorted++;
  }
 }
 printf("%s %s G=%.0f: partly visible triangles distorted by the clamp: %ld (worst |ndc| %.0f)\n",airports[airportStarts[k]].id,exterior?"ext":"cockpit",G,distorted,worst);
 return 0;
}
