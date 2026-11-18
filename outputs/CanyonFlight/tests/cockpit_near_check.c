/* Host check: nearest on-screen cockpit geometry over the full look range.
   The cockpit near plane must stay below this or visible parts are clipped.
   gcc -O2 -Iinclude tests/cockpit_near_check.c source/cockpit.c source/flight.c
       source/ground.c source/airports.c source/terrain.c source/terrain_stream.c
       source/terrain_data.c source/instruments.c -lm */
#include "cockpit.h"
#include <stdio.h>
static CockpitFace faces[COCKPIT_CAPACITY];
int main(void){
 const float tx=200.f/220,ty=120.f/220; /* top-screen half-extents at depth 1 */
 for(int type=0;type<2;type++){
  Flight s;resetFlightPhase(&s,type,0,PHASE_TAKEOFF);
  int n=buildCockpit(&s,faces,COCKPIT_CAPACITY);
  float best=1e9f,by=0,bp=0;int bf=-1;
  for(float yaw=-2.6f;yaw<=2.6f;yaw+=.05f)for(float pitch=-1.1f;pitch<=1.1f;pitch+=.05f){
   float cy=cosf(yaw),sy=sinf(yaw),cp=cosf(pitch),sp=sinf(pitch);
   Vec f={cp*cy,cp*sy,-sp},r={-sy,cy,0},u={-sp*cy,-sp*sy,-cp};
   for(int i=0;i<n;i++){
    Vec a=faces[i].v[0],b=faces[i].v[1],c=faces[i].v[2];
    for(int p=0;p<=8;p++)for(int q=0;q<=8-p;q++){
     float wa=p/8.f,wb=q/8.f,wc=1-wa-wb;
     Vec v={a.x*wa+b.x*wb+c.x*wc,a.y*wa+b.y*wb+c.y*wc,a.z*wa+b.z*wb+c.z*wc};
     float d=v.x*f.x+v.y*f.y+v.z*f.z;if(d<=.001f)continue;
     float x=(v.x*r.x+v.y*r.y+v.z*r.z)/d,y=(v.x*u.x+v.y*u.y+v.z*u.z)/d;
     if(fabsf(x)<=tx&&fabsf(y)<=ty&&d<best){best=d;by=yaw;bp=pitch;bf=i;}
    }
   }
  }
  printf("%s: %d faces, nearest on-screen depth %.3f m (face %d, yaw %.2f pitch %.2f)\n",type?"SF50":"SR22",n,best,bf,by,bp);
 }
 return 0;
}
