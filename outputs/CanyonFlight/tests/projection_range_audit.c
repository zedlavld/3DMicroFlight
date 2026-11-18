/* Host audit: world triangles whose vertices project outside the PICA200
   rasterizer's fixed-point range (assumed about +/-2048 px; Azahar's software
   rasterizer uses 12.4 fixed point). Such vertices wrap on hardware and
   produce single-frame spikes; Azahar's GL path never shows them.
   Counts triangles that also have a vertex on screen (partly visible) and
   those wholly off screen but still submitted with their draw range.
   usage: audit <airport> <phase> <exterior 0/1> [yaw offset deg] */
#include "scenery.h"
#include "camera.h"
#include "terrain.h"
#include <stdio.h>
#include <stdlib.h>
static WorldVertex v[SCENERY_PART_CAPACITY];
int main(int argc,char **argv){
 int ap=argc>1?atoi(argv[1]):3,phase=argc>2?atoi(argv[2]):PHASE_CRUISE,exterior=argc>3?atoi(argv[3]):1;
 float yawOffset=argc>4?(float)atof(argv[4])*3.14159265f/180:0;
 Flight s;resetFlightPhase(&s,1,airportStarts[ap],phase);s.yaw+=yawOffset;
 View view={0};resetView(&view);view.exterior=exterior;CameraPose cam=makeCamera(&s,&view);
 const float f=220,nearPlane=exterior?6:2,limit=argc>5?(float)atof(argv[5]):1800; /* px from screen centre */
 long total=0,overflowPartly=0,overflowHidden=0,behind=0,straddle=0,frontOnly=0;float worst=0;
 for(int part=0;part<SCENERY_PARTS;part++){
  int n=buildSceneryPart(&s,v,SCENERY_PART_CAPACITY,part);
  for(int i=0;i+2<n;i+=3){
   total++;int onScreen=0,bad=0;
   for(int j=0;j<3;j++){
    Vec c=projectCamera(&cam,v[i+j].p);
    if(c.z<nearPlane){bad=1;continue;}
    float x=f*c.x/c.z,y=f*c.y/c.z;
    if(fabsf(x)<=200&&fabsf(y)<=120)onScreen=1;
    if(fabsf(x)>limit||fabsf(y)>limit){bad=1;if(fabsf(x)>worst)worst=fabsf(x);}
   }
   if(!bad)continue;
   int anyFront=0;for(int j=0;j<3;j++)if(projectCamera(&cam,v[i+j].p).z>=nearPlane)anyFront=1;
   if(!anyFront){behind++;continue;}      /* wholly behind: clipped away */
   int anyBehind=0;for(int j=0;j<3;j++)if(projectCamera(&cam,v[i+j].p).z<nearPlane)anyBehind=1;
   if(anyBehind)straddle++;else frontOnly++;
   if(onScreen)overflowPartly++;else overflowHidden++;
  }
 }
 printf("%s %s alt %.0f m AGL: %ld tris, out-of-range & partly on screen %ld, out-of-range off screen %ld, wholly behind %ld, worst |x| %.0f px\n   of the out-of-range: %ld cross the near plane, %ld wholly in front\n",
  airports[airportStarts[ap]].id,exterior?"exterior":"cockpit",s.pos.z-terrain(s.pos.x,s.pos.y),total,overflowPartly,overflowHidden,behind,worst,straddle,frontOnly);
 return 0;
}
