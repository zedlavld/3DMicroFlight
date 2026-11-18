/* Estimated three-contact suspension and dry-tire model. Geometry, stiffness,
   friction and damage thresholds are engineering estimates, not certified data.
   Both aircraft use castering nose wheels: yaw at taxi speed comes primarily
   from differential main-wheel braking, not an invented nose steering servo. */
#include "flight.h"
#define G 9.80665f
static float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static Vec cross(Vec a,Vec b){return (Vec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static Vec add(Vec a,Vec b){return (Vec){a.x+b.x,a.y+b.y,a.z+b.z};}
static Vec scale(Vec a,float k){return (Vec){a.x*k,a.y*k,a.z*k};}
static Vec unit(Vec v){return scale(v,1/fmaxf(sqrtf(dot(v,v)),.00001f));}
static Vec toBody(const Flight *s,Vec v){return (Vec){dot(v,bodyWorld(s,(Vec){1,0,0})),dot(v,bodyWorld(s,(Vec){0,1,0})),dot(v,bodyWorld(s,(Vec){0,0,1}))};}
static int paved(Vec p){
 for(int i=0;i<AIRPORT_COUNT;i++){const Airport *a=&airports[i];float e=p.x-a->east,n=p.y-a->north,r=airportReach(a);
  if(e*e+n*n>r*r)continue;
  float h=a->heading*.01745329252f,along=e*sinf(h)+n*cosf(h),across=e*cosf(h)-n*sinf(h);
  if(fabsf(along)<a->length/2+10&&fabsf(across)<a->width/2+5)return 1;
  float apron=across*(float)-a->apronSide;
  if(a->primary&&a->apronSide&&along>-650&&along<250&&apron<-45&&apron>-250)return 1;
 }return 0;
}
void groundReaction(Flight *s,float dt,Vec external,Vec *force,Vec *moment){
 *force=*moment=(Vec){0};s->onGround=0;s->groundLoad=0;
 float base=terrain(s->pos.x,s->pos.y),clear=s->pos.z-base;
 if(clear>aircraft[s->aircraft].span*.5f+2)return;
 for(int side=-1;side<=1;side+=2){Vec tip=add(s->pos,bodyWorld(s,(Vec){0,side*aircraft[s->aircraft].span*.45f,0}));if(tip.z<terrain(tip.x,tip.y)){s->crashed=1;return;}}
 const Aircraft *craft=&aircraft[s->aircraft];
 if(s->gearActual<.95f){if(clear<craft->bellyClear+.15f)s->crashed=1;return;}
 if(clear>fmaxf(4,craft->gearArmZ+1.5f))return;
 if(clear<craft->bellyClear){s->crashed=1;return;}
 Vec normal=unit((Vec){-(terrain(s->pos.x+1,s->pos.y)-terrain(s->pos.x-1,s->pos.y))*.5f,-(terrain(s->pos.x,s->pos.y+1)-terrain(s->pos.x,s->pos.y-1))*.5f,1});
 Vec forward=bodyWorld(s,(Vec){1,0,0});forward=unit(add(forward,scale(normal,-dot(forward,normal))));Vec right=unit(cross(forward,normal));
 float mass=fmaxf(s->mass,100),weight=mass*G,k=weight/.27f,damping=1.5f*sqrtf(k*mass/3);
 Vec totalExternal=add(external,(Vec){0,0,-weight});
 const Aircraft *ac=&aircraft[s->aircraft];float track=ac->gearTrack,nose=ac->noseGearX;
 for(int i=0;i<3;i++){
  Vec arm={i==0?nose:ac->mainGearX,i==0?0:(i==1?-track:track),ac->gearArmZ};
  Vec offset=bodyWorld(s,arm),point=add(s->pos,offset);
  float penetration=terrain(point.x,point.y)-point.z;
  if(penetration<=0)continue;
  Vec spin=cross((Vec){s->p,s->q,s->r},arm),velocity=add(s->vel,bodyWorld(s,spin));float vn=dot(velocity,normal);
  if(vn<-4.5f||fabsf(s->roll)>.30f||s->pitch<-.18f||s->pitch>craft->tailStrike){s->crashed=1;continue;}
  s->onGround=1;s->touchdownSink=fmaxf(s->touchdownSink,fmaxf(-vn,0));
  float load=clampf(k*penetration-damping*vn,0,weight*3);s->groundLoad+=load/weight;
  int hard=paved(point);float mu=hard?.70f:.45f,rolling=hard?.022f:.075f;
  float brake=i==1?s->brakeLeft:i==2?s->brakeRight:0;float longitudinal=dot(velocity,forward),lateral=dot(velocity,right);
  Vec wheelForward=forward;
  if(i==0){Vec tangent=add(velocity,scale(normal,-vn));float speed=sqrtf(dot(tangent,tangent));if(speed>.05f)wheelForward=scale(tangent,1/speed);longitudinal=dot(velocity,wheelForward);lateral=0;}
  float longRequest=-longitudinal*mass/(3*dt)-dot(totalExternal,wheelForward)*load/weight;
  float fx=clampf(longRequest,-load*(rolling+.60f*clampf(brake,0,1)),load*(rolling+.60f*clampf(brake,0,1)));
  float fy=i==0?0:clampf(-lateral*mass/(3*fmaxf(dt,.03f))-dot(totalExternal,right)*load/weight,-mu*load,mu*load);
  float friction=sqrtf(fx*fx+fy*fy),limit=mu*load;if(friction>limit&&friction>0){fx*=limit/friction;fy*=limit/friction;}
  Vec reaction=add(scale(normal,load),add(scale(wheelForward,fx),scale(right,fy)));
  *force=add(*force,reaction);*moment=add(*moment,cross(arm,toBody(s,reaction)));
 }
}
