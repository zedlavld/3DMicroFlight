#include "camera.h"
#include "cockpit.h"
#define PI 3.14159265359f
static Vec add(Vec a,Vec b){return (Vec){a.x+b.x,a.y+b.y,a.z+b.z};}
static Vec scale(Vec a,float s){return (Vec){a.x*s,a.y*s,a.z*s};}
static float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static Vec cross(Vec a,Vec b){return (Vec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static Vec unit(Vec v){return scale(v,1/fmaxf(sqrtf(dot(v,v)),.00001f));}
static float deadzone(float x){return fabsf(x)<.12f?0:copysignf((clampf(fabsf(x),0,1)-.12f)/.88f,x);}
void resetView(View *v){v->insideYaw=0;v->insidePitch=0;v->orbitYaw=0;v->orbitPitch=.24f;if(v->distance<=0)v->distance=32;}
void zoomView(View *v,float dt,float direction){if(v->exterior)v->distance=clampf(v->distance*expf(-direction*dt*1.2f),14,160);}
void moveView(View *v,float dt,float x,float y){
 x=deadzone(x);y=deadzone(y);
 if(v->exterior){
  v->orbitYaw=remainderf(v->orbitYaw+x*dt*1.7f,2*PI);
  v->orbitPitch=clampf(v->orbitPitch+y*dt*1.3f,-1.15f,1.35f);
 }else{
  v->insideYaw=clampf(v->insideYaw+x*dt*1.7f,-2.6f,2.6f);
  v->insidePitch=clampf(v->insidePitch+y*dt*1.3f,-1.1f,1.1f);
 }
}
/* Approximate seated eye height above the dynamics CG. Cockpit geometry is
   authored relative to this eye, not relative to the wheel-contact origin. */
Vec pilotEyeWorld(const Flight *s){return add(s->pos,bodyWorld(s,(Vec){0,0,-.55f}));}
CameraPose makeCamera(const Flight *s,const View *v){
 CameraPose c;
 if(v->exterior){
  /* Orbit is relative to aircraft heading; horizon stays level in exterior view. */
  float yaw=s->yaw+v->orbitYaw,pitch=v->orbitPitch,distance=clampf(v->distance,14,160);
  /* v33.07: the imported 737 / F-16 exteriors are true size; frame them by
     span (737 ~41 m, F-16 ~26 m at the default 32 m zoom; tuned in Azahar). */
  if(aircraft[s->aircraft].mesh>=2)distance*=fmaxf(.8f,aircraft[s->aircraft].span/28);
  c.eye=add(s->pos,(Vec){-sinf(yaw)*cosf(pitch)*distance,-cosf(yaw)*cosf(pitch)*distance,sinf(pitch)*distance});
  c.eye.z=fmaxf(c.eye.z,terrain(c.eye.x,c.eye.y)+3);
  c.forward=unit(add(s->pos,scale(c.eye,-1)));
  c.right=unit(cross(c.forward,(Vec){0,0,1}));
  c.up=unit(cross(c.right,c.forward));
 }else{
  float look=v->insidePitch-cockpitViewTilt(s->aircraft);
  float cy=cosf(v->insideYaw),sy=sinf(v->insideYaw),cp=cosf(look),sp=sinf(look);
  c.eye=pilotEyeWorld(s);
  c.forward=bodyWorld(s,(Vec){cp*cy,cp*sy,-sp});
  c.right=bodyWorld(s,(Vec){-sy,cy,0});
  c.up=bodyWorld(s,(Vec){-sp*cy,-sp*sy,-cp});
 }
 return c;
}
Vec projectCamera(const CameraPose *c,Vec world){
 Vec d=add(world,scale(c->eye,-1));return (Vec){dot(d,c->right),dot(d,c->up),dot(d,c->forward)};
}

/* Conservative top-screen sphere test. Include both stereo eyes and their
   off-axis convergence shift; use it only to reject wholly invisible ranges. */
int cameraSphereVisible(const CameraPose *c,Vec center,float radius,float iod,float convergence){
 Vec p=projectCamera(c,center);
 float r=fmaxf(radius,0)+2,halfEye=fabsf(iod)*.5f;
 float sx=200.f/220+halfEye/fmaxf(convergence,2),sy=120.f/220;
 float horizontal=r*sqrtf(1+sx*sx)+halfEye,vertical=r*sqrtf(1+sy*sy);
 return p.z+r>=2&&p.z-r<=140000&&
  p.x+sx*p.z>=-horizontal&&sx*p.z-p.x>=-horizontal&&
  p.y+sy*p.z>=-vertical&&sy*p.z-p.y>=-vertical;
}

void cameraViewRows(const CameraPose *c,Vec anchor,float rows[4][4]){
 Vec t=projectCamera(c,anchor);
 float r[4][4]={{c->right.x,c->right.y,c->right.z,t.x},{c->up.x,c->up.y,c->up.z,t.y},{-c->forward.x,-c->forward.y,-c->forward.z,-t.z},{0,0,0,1}};
 for(int i=0;i<4;i++)for(int j=0;j<4;j++)rows[i][j]=r[i][j];
}

void aircraftViewRows(const Flight *s,const CameraPose *c,float rows[4][4]){
 Vec axes[3]={bodyWorld(s,(Vec){1,0,0}),bodyWorld(s,(Vec){0,1,0}),bodyWorld(s,(Vec){0,0,1})};
 Vec t=projectCamera(c,s->pos);
 for(int i=0;i<3;i++){rows[0][i]=dot(c->right,axes[i]);rows[1][i]=dot(c->up,axes[i]);rows[2][i]=-dot(c->forward,axes[i]);}
 rows[0][3]=t.x;rows[1][3]=t.y;rows[2][3]=-t.z;
 rows[3][0]=rows[3][1]=rows[3][2]=0;rows[3][3]=1;
}

/* Off-axis horizontal coefficients for the rotated top-screen projection. */
void stereoProjectionTerms(float signedIod,float convergence,float scale,float *z,float *w){
 float offset=signedIod*.5f;
 *w=scale*offset;*z=*w/fmaxf(convergence,2.f);
}
