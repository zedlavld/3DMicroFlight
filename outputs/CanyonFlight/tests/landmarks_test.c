/* v1.1p9: build every landmark of source/landmarks.c on flat ground, log its triangle count
   (all, and the far set of parts at least 3 m thick) and extent, and check the vertices are
   finite. Run by tools/test_landmarks.py (ARM under unicorn, like tools/test_maneuver.py). */
#include "landmarks.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
char logbuf[16384];int logLen,checks,failures,total,totalFar;
float rawElevation(float east,float north){(void)east;(void)north;return 300;}
int main(void){
 for(int i=0;i<landmarkCount();i++){
  LandmarkVertex *v=NULL;float z0=0;int far=0,n=landmarkTriangles(i,300,&v,&z0,&far);
  float lo[3]={1e9f,1e9f,1e9f},hi[3]={-1e9f,-1e9f,-1e9f};
  checks++;if(n<3||!v||far<0||far>n||far%3)failures++;
  for(int k=0;k<n;k++){float p[3]={v[k].x,v[k].y,v[k].z};for(int a=0;a<3;a++){if(!isfinite(p[a])){failures++;break;}
   if(p[a]<lo[a])lo[a]=p[a];if(p[a]>hi[a])hi[a]=p[a];}}
  if(logLen<(int)sizeof(logbuf)-120)logLen+=snprintf(logbuf+logLen,sizeof(logbuf)-logLen,"%-32s %6d triangles (%5d far)  %5.0f x %5.0f m, top %5.0f m\n",
   landmarkName(i),n/3,far/3,hi[0]-lo[0],hi[1]-lo[1],hi[2]+z0);
  total+=n/3;totalFar+=far/3;free(v);
 }
 logLen+=snprintf(logbuf+logLen,sizeof(logbuf)-logLen,"total %d triangles, %d far\n",total,totalFar);
 return 0;
}
