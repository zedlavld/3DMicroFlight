#include "cockpit.h"
#include "camera.h"
#include <string.h>
int checks,failures,firstFailure,counts[2],fixedCounts[2];CockpitFace meshes[2][COCKPIT_CAPACITY];
static CockpitFace scratch[COCKPIT_CAPACITY];
float terrain(float x,float y){return -1000;}
static float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
static Vec sub(Vec a,Vec b){return (Vec){a.x-b.x,a.y-b.y,a.z-b.z};}
static Vec cross(Vec a,Vec b){return (Vec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
int main(void){
 Flight cameraFlight={0};View view={.distance=32};
 for(int pose=0;pose<5;pose++)for(int yaw=-4;yaw<=4;yaw++)for(int pitch=-4;pitch<=4;pitch++){
  setAttitude(&cameraFlight,.2f*pose,.1f*pose,.6f*pose);view.insideYaw=yaw*.6f;view.insidePitch=pitch*.25f;
  CameraPose c=makeCamera(&cameraFlight,&view);
  Vec eye=pilotEyeWorld(&cameraFlight);CHECK(fabsf(c.eye.x-eye.x)+fabsf(c.eye.y-eye.y)+fabsf(c.eye.z-eye.z)<.00001f);
  CHECK(fabsf(dot(c.forward,c.up))<.00001f&&fabsf(dot(c.forward,c.right))<.00001f&&fabsf(dot(c.up,c.right))<.00001f);
  CHECK(fabsf(dot(c.forward,c.forward)-1)<.00001f&&fabsf(dot(c.up,c.up)-1)<.00001f&&fabsf(dot(c.right,c.right)-1)<.00001f);
 }

 for(int type=0;type<2;type++){
  Flight s={0};s.aircraft=type;s.ias=type?170:105;s.pos.z=1800;s.yaw=.8f;
  fixedCounts[type]=buildCockpitStatic(type,scratch,COCKPIT_CAPACITY);
  int n=counts[type]=buildCockpit(&s,meshes[type],COCKPIT_CAPACITY);CHECK(n>200&&n<COCKPIT_CAPACITY);
  for(int i=0;i<n;i++){
   CockpitFace *f=&meshes[type][i];Vec normal=cross(sub(f->v[1],f->v[0]),sub(f->v[2],f->v[0]));
   CHECK(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z>1e-16f);
   for(int j=0;j<3;j++){Vec p=f->v[j];CHECK(isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<3&&fabsf(p.y)<3&&fabsf(p.z)<3);}
  }
  s.roll=.4f;s.pitch=.15f;s.ias+=20;s.pos.z+=100;
  int changed=buildCockpit(&s,scratch,COCKPIT_CAPACITY);CHECK(changed>0&&memcmp(scratch,meshes[type],(changed<n?changed:n)*sizeof(CockpitFace)));
  /* Extreme attitudes must keep clipped PFD polygons finite and bounded. */
  for(int a=0;a<12;a++){
   s.roll=a*.6f;s.pitch=(a-6)*.25f;int k=buildCockpit(&s,scratch,COCKPIT_CAPACITY);CHECK(k>0&&k<COCKPIT_CAPACITY);
   for(int i=0;i<k;i++)for(int j=0;j<3;j++)CHECK(isfinite(scratch[i].v[j].x)&&isfinite(scratch[i].v[j].y)&&isfinite(scratch[i].v[j].z));
  }
  CHECK(buildCockpit(&s,NULL,0)==0);
  memset(scratch,0xa5,sizeof(scratch));CHECK(buildCockpit(&s,scratch,7)==7);
  const unsigned char *bytes=(const unsigned char*)&scratch[7];CHECK(bytes[0]==0xa5&&bytes[sizeof(CockpitFace)-1]==0xa5);
 }
 return failures;
}
