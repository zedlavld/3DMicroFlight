/* Host audit: fly the hands-off autotest cruise and print the world
   triangles covering one screen pixel, with each vertex's NDC and w.
   usage: probe <start index> <phase> <exterior> <px> <py> <t0> <t1> */
#include "scenery.h"
#include "camera.h"
#include <stdio.h>
#include <stdlib.h>
static WorldVertex v[SCENERY_PART_CAPACITY];
int main(int argc,char **argv){
 int k=atoi(argv[1]),phase=atoi(argv[2]),exterior=atoi(argv[3]);float PX=atof(argv[4]),PY=atof(argv[5]),t0=atof(argv[6]),t1=atof(argv[7]);
 Flight s;resetFlightPhase(&s,1,airportStarts[k],phase);
 View view={0};resetView(&view);view.exterior=exterior;
 const float tx=200.f/220,ty=120.f/220;
 float qx=PX/200-1,qy=1-PY/120;
 for(float t=0;t<=t1;t+=.5f){
  if(t>=t0){
   CameraPose cam=makeCamera(&s,&view);int hits=0;
   printf("t=%.1f pos %.0f %.0f %.0f\n",t,s.pos.x,s.pos.y,s.pos.z);
   for(int part=0;part<SCENERY_PARTS;part++){
    int n=buildSceneryPart(&s,v,SCENERY_PART_CAPACITY,part);
    for(int i=0;i+2<n;i+=3){
     float x[3],y[3],w[3];int ok=1;
     for(int j=0;j<3;j++){Vec c=projectCamera(&cam,v[i+j].p);w[j]=c.z;if(c.z<=0){ok=0;break;}x[j]=c.x/c.z/tx;y[j]=c.y/c.z/ty;}
     if(!ok)continue;
     float d0=(x[1]-x[0])*(qy-y[0])-(y[1]-y[0])*(qx-x[0]),d1=(x[2]-x[1])*(qy-y[1])-(y[2]-y[1])*(qx-x[1]),d2=(x[0]-x[2])*(qy-y[2])-(y[0]-y[2])*(qx-x[2]);
     if(!((d0>=0&&d1>=0&&d2>=0)||(d0<=0&&d1<=0&&d2<=0)))continue;
     hits++;printf("  part %2d tri %4d:",part,i/3);
     for(int j=0;j<3;j++)printf(" [%.0f,%.0f px w=%.0f | %.1f %.1f %.1f]",(x[j]+1)*200,(1-y[j])*120,w[j],v[i+j].p.x,v[i+j].p.y,v[i+j].p.z);
     printf("\n");
    }
   }
   if(!hits)printf("  NO TRIANGLE COVERS THIS PIXEL\n");
  }
  for(int step=0;step<15;step++)stepFlight(&s,1.f/30,0,0,0);
 }
 return 0;
}
