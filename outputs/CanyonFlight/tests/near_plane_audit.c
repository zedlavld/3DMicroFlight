/* Host audit: world triangles that cross the 2 m world near plane for a
   given start, airport and view. The PICA200 clips in float24; triangles
   spanning from far ahead to behind the camera are the ones that produced
   single-frame wedges on hardware (v31 video IMG_7637).
   gcc -O2 -Iinclude tests/near_plane_audit.c source/scenery.c source/camera.c
       source/terrain.c source/terrain_stream.c source/terrain_data.c
       source/airports.c source/flight.c source/ground.c source/imagery.c
       source/weather_visual.c -lm
   usage: audit [airport index] [type 0/1] [exterior 0/1] */
#include "scenery.h"
#include "camera.h"
#include "terrain.h"
#include <stdio.h>
#include <stdlib.h>
static WorldVertex v[SCENERY_PART_CAPACITY];
int main(int argc,char **argv){
 int ap=argc>1?atoi(argv[1]):0,type=argc>2?atoi(argv[2]):1,exterior=argc>3?atoi(argv[3]):1;
 Flight s;resetFlightPhase(&s,type,ap,PHASE_TAKEOFF);
 View view={0};resetView(&view);view.exterior=exterior;CameraPose cam=makeCamera(&s,&view);
 printf("eye %.1f %.1f %.2f (%.2f m above terrain)\n",cam.eye.x,cam.eye.y,cam.eye.z,cam.eye.z-terrain(cam.eye.x,cam.eye.y));
 const float nearPlane=2;int straddle=0,worstPart=-1;float worst=0;
 for(int part=0;part<SCENERY_PARTS;part++){
  int n=buildSceneryPart(&s,v,SCENERY_PART_CAPACITY,part);
  for(int i=0;i+2<n;i+=3){
   Vec c[3];float zmin=1e9f,zmax=-1e9f,len=0;
   for(int j=0;j<3;j++){c[j]=projectCamera(&cam,v[i+j].p);zmin=fminf(zmin,c[j].z);zmax=fmaxf(zmax,c[j].z);}
   if(!(zmin<nearPlane&&zmax>nearPlane))continue;
   for(int j=0;j<3;j++){Vec a=v[i+j].p,b=v[i+(j+1)%3].p;len=fmaxf(len,sqrtf((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)));}
   straddle++;
   if(len>40||part>=32)printf("part %2d tri %5d len %7.1f m  z %8.1f..%8.1f  rgb %.2f %.2f %.2f mat %d\n",part,i/3,len,zmin,zmax,v[i].color[0],v[i].color[1],v[i].color[2],sceneryVertexMaterial(i));
   if(len>worst){worst=len;worstPart=part;}
  }
 }
 printf("near-plane straddlers %d, longest %.1f m (part %d)\n",straddle,worst,worstPart);
 return 0;
}
