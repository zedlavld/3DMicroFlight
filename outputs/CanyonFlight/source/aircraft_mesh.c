#include "aircraft_mesh.h"
#include <stdint.h>
#include <string.h>
#include "sr22_imported_mesh_bin.h"
#include "sf50_imported_mesh_bin.h"
#include "b737_imported_mesh_bin.h"
#include "f16_imported_mesh_bin.h"
#include "a321_imported_mesh_bin.h"
#define PI 3.14159265359f
typedef char ImportedFaceSizeCheck[sizeof(AircraftFace)==76?1:-1];
static float visualFlap;
static AircraftDynamicPart dynamicPart;
AircraftDynamicPart aircraftDynamicPart(void){return dynamicPart;}
static AircraftFace *faces;static int faceCount,faceCapacity;
static ModelVertex vertex(Vec p,float u,float v){return (ModelVertex){p,u,v,1};}
static Vec subtract(Vec a,Vec b){return (Vec){a.x-b.x,a.y-b.y,a.z-b.z};}
static Vec cross(Vec a,Vec b){return (Vec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static void triangle(ModelVertex a,ModelVertex b,ModelVertex c){
 if(faceCount>=faceCapacity)return;
 Vec n=cross(subtract(b.p,a.p),subtract(c.p,a.p));float norm=sqrtf(n.x*n.x+n.y*n.y+n.z*n.z);
 /* Absolute light avoids inverted procedural face winding; refine normals later. */
 float light=.62f+.38f*fabsf((n.x*.25f+n.y*.35f-n.z*.9f)/fmaxf(norm,.001f));
 a.light=b.light=c.light=clampf(light,.45f,1);
 faces[faceCount++]=(AircraftFace){{a,b,c},0};
}
static ModelVertex material(Vec p,int swatch){
 return vertex(p,(swatch*32+16.f)/512,1-3.5f/512);
}
static void panel(Vec a,Vec b,Vec c,Vec d,int swatch){
 triangle(material(a,swatch),material(b,swatch),material(c,swatch));
 triangle(material(a,swatch),material(c,swatch),material(d,swatch));
}
/* Fuselage sections fitted by eye to the POH three-view silhouettes.
   Top/bottom are independent: a cabin is not a symmetric cigar. */
typedef struct {float x,width,top,bottom;} Station;
static float spline(float a,float b,float c,float d,float t){
 return .5f*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t);
}
static Station section(const Station *stations,int n,float x){
 int i=0;while(i<n-2&&x<stations[i+1].x)i++;
 float t=clampf((stations[i].x-x)/(stations[i].x-stations[i+1].x),0,1);
 Station a=stations[i?i-1:0],b=stations[i],c=stations[i+1],d=stations[i+2<n?i+2:n-1];
 return (Station){x,fmaxf(.003f,spline(a.width,b.width,c.width,d.width,t)),spline(a.top,b.top,c.top,d.top,t),spline(a.bottom,b.bottom,c.bottom,d.bottom,t)};
}
static Vec skin(const Station *s,int n,float x,float theta,float offset){
 Station a=section(s,n,x);float mid=(a.top+a.bottom)/2,h=(a.bottom-a.top)/2;
 return (Vec){x,(a.width+offset)*cosf(theta),mid+(h+offset)*sinf(theta)};
}
static void loft(const Station *stations,int n,int swatch){
 const int rings=24,steps=3;
 for(int i=0;i<n-1;i++)for(int sub=0;sub<steps;sub++)for(int j=0;j<rings;j++){
  float x0=stations[i].x+(stations[i+1].x-stations[i].x)*sub/steps;
  float x1=stations[i].x+(stations[i+1].x-stations[i].x)*(sub+1)/steps;
  float t0=j*2*PI/rings,t1=(j+1)*2*PI/rings;
  Vec a=skin(stations,n,x0,t0,0),b=skin(stations,n,x1,t0,0),c=skin(stations,n,x1,t1,0),d=skin(stations,n,x0,t1,0);
  int paint=swatch<0?(sinf((t0+t1)/2)>.45f?1:0):swatch;
  panel(a,b,c,d,paint);
  if(swatch<0){
   const float angle[6]={t0,t0,t1,t0,t1,t1};
   for(int v=0;v<6;v++){
    ModelVertex *p=&faces[faceCount-2+v/3].v[v%3];
    p->u=.02f+.71f*(stations[0].x-p->p.x)/(stations[0].x-stations[n-1].x);
    p->v=1-(.04f+.92f*angle[v]/(2*PI));
   }
  }
  /* Smooth radial shading keeps the cabin rounded at handheld resolution. */
  for(int f=faceCount-2;f<faceCount;f++)for(int v=0;v<3;v++){
   Vec p=faces[f].v[v].p;Station st=section(stations,n,p.x);
   float yy=p.y/fmaxf(st.width,.03f),zz=(p.z-(st.top+st.bottom)/2)/fmaxf((st.bottom-st.top)/2,.03f);
   faces[f].v[v].light=clampf(.72f+.23f*(-zz)+.08f*yy,.48f,1);
  }
 }
}
/* Surface-conforming glazing and paint patches use x/theta coordinates.
   They do not stretch window graphics around the nose or across the roof. */
typedef struct {float x,theta;} SkinPoint;
static void patch(const Station *s,int n,SkinPoint a,SkinPoint b,SkinPoint c,SkinPoint d,int color){
 for(int i=0;i<5;i++)for(int j=0;j<4;j++){
  Vec q[4];float us[4]={i/5.f,(i+1)/5.f,(i+1)/5.f,i/5.f};float vs[4]={j/4.f,j/4.f,(j+1)/4.f,(j+1)/4.f};
  for(int k=0;k<4;k++){
   float u=us[k],v=vs[k],x=(a.x*(1-u)+b.x*u)*(1-v)+(d.x*(1-u)+c.x*u)*v;
   float theta=(a.theta*(1-u)+b.theta*u)*(1-v)+(d.theta*(1-u)+c.theta*u)*v;
   q[k]=skin(s,n,x,theta,color==0?.058f:.045f);
  }
  panel(q[0],q[1],q[2],q[3],color);
 }
}
static void sideWindow(const Station *s,int n,float front,float back,float top,float bottom,int sign){
 float mirror=sign>0?0:-PI;float factor=sign>0?1:-1;
 patch(s,n,(SkinPoint){front-.05f,mirror+factor*bottom},(SkinPoint){back+.04f,mirror+factor*bottom},
  (SkinPoint){back,mirror+factor*top},(SkinPoint){front-.12f,mirror+factor*(top-.08f)},2);
}
static void ovalWindow(const Station *s,int n,float x,float halfWidth,float theta,float halfHeight,int sign){
 float mirror=sign>0?0:-PI,factor=sign>0?1:-1;
 for(int ring=0;ring<3;ring++)for(int j=0;j<20;j++){
  Vec q[4];float radius[4]={ring/3.f,(ring+1)/3.f,(ring+1)/3.f,ring/3.f};
  float angle[4]={j*2*PI/20,j*2*PI/20,(j+1)*2*PI/20,(j+1)*2*PI/20};
  for(int k=0;k<4;k++)q[k]=skin(s,n,x+halfWidth*radius[k]*copysignf(powf(fabsf(cosf(angle[k])),.65f),cosf(angle[k])),mirror+factor*(theta+halfHeight*radius[k]*copysignf(powf(fabsf(sinf(angle[k])),.65f),sinf(angle[k]))),.045f);
  panel(q[0],q[1],q[2],q[3],2);
 }
}
static void skinStroke(const Station *s,int n,const SkinPoint *points,int count,int sign){
 float mirror=sign>0?0:-PI,factor=sign>0?1:-1;
 for(int i=0;i<count-1;i++){
  float dx=points[i+1].x-points[i].x,dt=points[i+1].theta-points[i].theta;
  float norm=fmaxf(hypotf(dx,dt),.001f),px=-dt/norm*.009f,pt=dx/norm*.012f;
  for(int j=0;j<6;j++){
   float u=j/6.f,v=(j+1)/6.f,x=points[i].x+dx*u,th=points[i].theta+dt*u,xx=points[i].x+dx*v,tt=points[i].theta+dt*v;
   panel(skin(s,n,x+px,mirror+factor*(th+pt),.054f),skin(s,n,xx+px,mirror+factor*(tt+pt),.054f),
    skin(s,n,xx-px,mirror+factor*(tt-pt),.054f),skin(s,n,x-px,mirror+factor*(th-pt),.054f),4);
  }
 }
}
static void windshield(const Station *s,int n,float front,float rear){
 /* Two shaped panes meet at a narrow centre post rather than a black roof band. */
 for(int half=0;half<2;half++)for(int i=0;i<6;i++)for(int j=0;j<5;j++){
  Vec q[4];float us[4]={i/6.f,(i+1)/6.f,(i+1)/6.f,i/6.f},vs[4]={j/5.f,j/5.f,(j+1)/5.f,(j+1)/5.f};
  for(int k=0;k<4;k++){
   float u=(half+us[k])*.5f,theta=-.43f-u*(PI-.86f);
   theta+=half?-.022f:.022f;
   float f=front+.12f*sinf(PI*u),r=rear+.08f*sinf(PI*u),x=f+(r-f)*vs[k];
   q[k]=skin(s,n,x,theta,.048f);
  }
  panel(q[0],q[1],q[2],q[3],2);
 }
}
static Vec wingPoint(int sign,float rootX,float rootChord,float tipX,float tipChord,float span,float rootZ,float tipZ,float t,float f,int lower){
 float chord=rootChord+(tipChord-rootChord)*t,leading=rootX+(tipX-rootX)*t;
 /* Rounded cap: the tip loses a little chord while retaining its measured span. */
 if(t>.94f){float cap=(t-.94f)/.06f;leading-=.045f*cap;chord-=.09f*cap;}
 float thickness=.60f*(.2969f*sqrtf(fmaxf(f,0))-.1260f*f-.3516f*f*f+.2843f*f*f*f-.1036f*f*f*f*f);
 float camber=.012f*sinf(PI*f),z=rootZ+(tipZ-rootZ)*t+chord*(lower?thickness*.7f-camber:-thickness-camber);
 float x=leading-f*chord;
 if(span>4&&t<=.5801f&&f>.72f){
  float deflection=visualFlap*.58f,arm=(f-.72f)*chord;x+=arm*(1-cosf(deflection));z+=arm*sinf(deflection);
 }
 return (Vec){x,sign*span*t,z};
}
/* White root fairings bridge the loft skin and upper lifting surface.
   Both ends overlap their parent surfaces to avoid a light-through seam. */
static void rootFairing(const Station *body,int count,int sign,float x,float chord,float tx,float tc,float span,float rz,float tz){
 const float f[5]={0,.06f,.25f,.55f,.82f};
 for(int i=0;i<4;i++){
  Vec a=skin(body,count,x-f[i]*chord,sign>0?.38f:PI-.38f,.003f);
  Vec d=skin(body,count,x-f[i+1]*chord,sign>0?.38f:PI-.38f,.003f);
  Vec b=wingPoint(sign,x,chord,tx,tc,span,rz,tz,.15f,f[i],0);
  Vec c=wingPoint(sign,x,chord,tx,tc,span,rz,tz,.15f,f[i+1],0);
  b.z-=.003f;c.z-=.003f;panel(a,b,c,d,0);
 }
}
static void wing(int sign,float rootX,float rootChord,float tipX,float tipChord,float span,float rootZ,float tipZ,int swatch){
 const float spanStations[7]={0,.08f,.25f,.58f,.65f,.94f,1};
 const float profile[18]={0,.015f,.04f,.09f,.18f,.32f,.5f,.7f,.86f,1,.86f,.7f,.5f,.32f,.18f,.09f,.04f,.015f};
 for(int j=0;j<6;j++)for(int k=0;k<18;k++){
  int next=(k+1)%18;
  Vec a=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,spanStations[j],profile[k],k>9);
  Vec b=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,spanStations[j+1],profile[k],k>9);
  Vec c=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,spanStations[j+1],profile[next],next>9);
  Vec d=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,spanStations[j],profile[next],next>9);
  int first=faceCount;panel(a,b,c,d,swatch);
  const int corner[6]={0,1,2,0,2,3};
  for(int q=0;q<6&&first+q/3<faceCount;q++){
   int v=corner[q];float t=spanStations[j+((v==1||v==2)?1:0)],f=profile[(v>=2)?next:k];int lower=(v>=2)?next>9:k>9;
   float tf=t<.999f?t+.001f:t-.001f,ff=f<.999f?f+.001f:f-.001f;
   ModelVertex *surface=&faces[first+q/3].v[q%3];
   surface->u=((lower?902:784)+108*f)/1024;
   surface->v=1-(400+610*t)/1024;
   Vec p=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,t,f,lower);
   Vec dt=subtract(wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,tf,f,lower),p);
   Vec df=subtract(wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,t,ff,lower),p);
   Vec n=cross(dt,df);float length=fmaxf(sqrtf(n.x*n.x+n.y*n.y+n.z*n.z),.0000001f);
   float direction=(lower?n.z<0:n.z>0)?-1.f:1.f;
   faces[first+q/3].v[q%3].light=clampf(.72f+direction*(-.23f*n.z+.08f*n.y)/length,.48f,1);
  }
 }
 for(int k=1;k<17;k++)triangle(material(wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,1,0,0),swatch),
  material(wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,1,profile[k],k>9),swatch),
  material(wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,1,profile[k+1],k+1>9),swatch));
 /* Upper-surface control hinge and spanwise parting lines. */
 for(int j=0;j<6;j++){
  Vec a=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,spanStations[j],.72f,0);
  Vec b=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,spanStations[j+1],.72f,0);
  Vec c=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,spanStations[j+1],.727f,0);
  Vec d=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,spanStations[j],.727f,0);
  a.z-=.003f;b.z-=.003f;c.z-=.003f;d.z-=.003f;panel(a,b,c,d,4);
 }
 /* Small position-light housings at measured wing extremities. */
 if(span>4){
  Vec a=wingPoint(sign,rootX,rootChord,tipX,tipChord,span,rootZ,tipZ,1,.12f,0);
  panel((Vec){a.x+.04f,a.y,a.z-.015f},(Vec){a.x-.08f,a.y,a.z-.015f},(Vec){a.x-.08f,a.y,a.z+.035f},(Vec){a.x+.04f,a.y,a.z+.035f},sign>0?6:5);
 }
}
static void strut(Vec a,Vec b,float radius){
 Vec axis=subtract(b,a);float len=sqrtf(axis.x*axis.x+axis.y*axis.y+axis.z*axis.z);
 axis.x/=len;axis.y/=len;axis.z/=len;Vec side=cross(axis,(Vec){1,0,0});float sl=sqrtf(side.x*side.x+side.y*side.y+side.z*side.z);
 side.x/=sl;side.y/=sl;side.z/=sl;Vec up=cross(axis,side);
 for(int i=0;i<6;i++){
  float t=i*2*PI/6,u=(i+1)*2*PI/6;Vec q[4];
  for(int k=0;k<4;k++){Vec p=k==0||k==3?a:b;float angle=k<2?t:u;
   q[k]=(Vec){p.x+radius*(side.x*cosf(angle)+up.x*sinf(angle)),p.y+radius*(side.y*cosf(angle)+up.y*sinf(angle)),p.z+radius*(side.z*cosf(angle)+up.z*sinf(angle))};}
  panel(q[0],q[1],q[2],q[3],4);
 }
}
static void gearLeg(float x,float y,float z,int fairing){
 float bottom=z+.60f;
 Vec attach={fabsf(y)>.1f?x+.28f:x+.13f,y*.64f,z-.12f};
 strut(attach,(Vec){x,y,bottom},fairing?.045f:.055f);
 if(!fairing&&fabsf(y)>.1f)strut((Vec){x+.4f,y*.72f,z-.02f},(Vec){x,y,bottom-.18f},.025f);
 for(int j=0;j<16;j++){
  float a=j*2*PI/16,b=(j+1)*2*PI/16;
  Vec p={x+.19f*cosf(a),y-.09f,bottom+.19f*sinf(a)},q={x+.19f*cosf(b),y-.09f,bottom+.19f*sinf(b)};
  Vec r=q,t=p;r.y+=.18f;t.y+=.18f;panel(p,q,r,t,3);
  triangle(material((Vec){x,y-.095f,bottom},3),material(p,3),material(q,3));
  p.y+=.185f;q.y+=.185f;triangle(material((Vec){x,y+.095f,bottom},3),material(q,3),material(p,3));
 }

 for(int side=-1;side<=1;side+=2)for(int i=0;i<12;i++){
  float a=i*2*PI/12,b=(i+1)*2*PI/12,yy=y+side*.097f;
  triangle(material((Vec){x,yy,bottom},4),material((Vec){x+.095f*cosf(a),yy,bottom+.095f*sinf(a)},4),material((Vec){x+.095f*cosf(b),yy,bottom+.095f*sinf(b)},4));
 }
 if(fairing){
  const Station pant[]={ {x+.46f,.01f,bottom-.10f,bottom+.02f},{x+.25f,.14f,bottom-.21f,bottom+.12f},
   {x-.04f,.16f,bottom-.22f,bottom+.13f},{x-.45f,.08f,bottom-.09f,bottom+.06f},{x-.57f,.005f,bottom,bottom+.01f} };
  int start=faceCount;loft(pant,5,0);
  for(int i=start;i<faceCount;i++)for(int v=0;v<3;v++)faces[i].v[v].p.y+=y;
 }
}

int buildProceduralAircraftMesh(const Flight *s,float propAngle,AircraftFace *destination,int capacity){
 faces=destination;faceCapacity=capacity;faceCount=0;dynamicPart=(AircraftDynamicPart){0};visualFlap=clampf(s->flapActual,0,1);

 if(s->aircraft){
  const Station fuselage[]={
   {4.05f,.008f,.03f,.05f},{3.91f,.24f,-.16f,.24f},{3.55f,.43f,-.31f,.37f},
   {2.90f,.58f,-.49f,.47f},{2.30f,.68f,-.83f,.53f},{1.60f,.75f,-1.12f,.58f},
   {.75f,.78f,-1.16f,.58f},{-.20f,.76f,-1.10f,.56f},{-1.20f,.66f,-.96f,.46f},
   {-2.20f,.50f,-.79f,.31f},{-3.25f,.30f,-.62f,.18f},{-4.25f,.15f,-.49f,.09f},{-5.02f,.012f,-.05f,.03f}};
  loft(fuselage,13,-1);
  windshield(fuselage,13,2.55f,1.70f);
  for(int sign=-1;sign<=1;sign+=2){
   ovalWindow(fuselage,13,1.20f,.29f,-.51f,.40f,sign);
   const SkinPoint door[]={{1.73f,-1.08f},{.73f,-1.02f},{.58f,.65f},{1.47f,.75f},{1.73f,-1.08f}};
   if(sign<0){skinStroke(fuselage,13,door,5,sign);const SkinPoint handle[]={{1.45f,.23f},{1.58f,.23f}};skinStroke(fuselage,13,handle,2,sign);}
   else {const SkinPoint hatch[]={{1.59f,-1.02f},{.79f,-.96f},{.77f,.02f},{1.60f,.02f},{1.59f,-1.02f}};skinStroke(fuselage,13,hatch,5,sign);}
   ovalWindow(fuselage,13,.34f,.23f,-.50f,.34f,sign);
   ovalWindow(fuselage,13,-.30f,.21f,-.50f,.30f,sign);
   wing(sign,.67f,1.95f,.17f,1.13f,aircraft[1].span/2,.49f,.13f,0);
   rootFairing(fuselage,13,sign,.67f,1.95f,.17f,1.13f,aircraft[1].span/2,.49f,.13f);
   wing(sign,-3.60f,1.22f,-4.55f,.75736f,2.25f,-.13f,-2.02f,0);
  }
  /* Short, aft-mounted FJ33 nacelle, embedded into a dorsal fairing. */
  const Station pod[]={ {-.83f,.32f,-1.44f,-.80f},{-1.05f,.36f,-1.43f,-.74f},
   {-1.65f,.35f,-1.35f,-.69f},{-2.35f,.30f,-1.23f,-.65f},{-3.05f,.22f,-1.10f,-.66f} };
  const Station dorsal[]={ {-.95f,.13f,-.91f,-.76f},{-1.5f,.25f,-.90f,-.59f},{-2.3f,.23f,-.79f,-.48f},{-3.2f,.06f,-.58f,-.45f} };
  loft(dorsal,4,0);
  loft(pod,5,0);
  for(int j=0;j<24;j++){
   float a=j*2*PI/24,b=(j+1)*2*PI/24;
   panel((Vec){-.83f,.32f*cosf(a),-1.12f+.32f*sinf(a)},(Vec){-.83f,.32f*cosf(b),-1.12f+.32f*sinf(b)},
    (Vec){-.76f,.267f*cosf(b),-1.12f+.267f*sinf(b)},(Vec){-.76f,.267f*cosf(a),-1.12f+.267f*sinf(a)},0);
   panel((Vec){-.76f,.267f*cosf(a),-1.12f+.267f*sinf(a)},(Vec){-.76f,.267f*cosf(b),-1.12f+.267f*sinf(b)},
    (Vec){-.98f,.25f*cosf(b),-1.12f+.25f*sinf(b)},(Vec){-.98f,.25f*cosf(a),-1.12f+.25f*sinf(a)},3);
   panel((Vec){-3.05f,.22f*cosf(a),-.88f+.22f*sinf(a)},(Vec){-3.05f,.22f*cosf(b),-.88f+.22f*sinf(b)},
    (Vec){-3.09f,.18f*cosf(b),-.88f+.18f*sinf(b)},(Vec){-3.09f,.18f*cosf(a),-.88f+.18f*sinf(a)},4);
  }
  dynamicPart=(AircraftDynamicPart){faceCount,10,{-.805f,0,-1.12f},.3f};
  for(int blade=0;blade<10;blade++){
   float a=blade*2*PI/10+propAngle*.3f;
   triangle(material((Vec){-.805f,.07f*cosf(a),-1.12f+.07f*sinf(a)},4),
    material((Vec){-.805f,.255f*cosf(a+.16f),-1.12f+.255f*sinf(a+.16f)},4),
    material((Vec){-.805f,.255f*cosf(a+.34f),-1.12f+.255f*sinf(a+.34f)},4));
  }
  for(int j=0;j<20;j++){
   float a=j*2*PI/20,b=(j+1)*2*PI/20;
   triangle(material((Vec){-.821f,0,-1.12f},3),material((Vec){-.821f,.28f*cosf(a),-1.12f+.28f*sinf(a)},3),material((Vec){-.821f,.28f*cosf(b),-1.12f+.28f*sinf(b)},3));
   triangle(material((Vec){-3.06f,0,-.88f},3),material((Vec){-3.06f,.20f*cosf(a),-.88f+.20f*sinf(a)},3),material((Vec){-3.06f,.20f*cosf(b),-.88f+.20f*sinf(b)},3));
  }
 }else{
  const Station fuselage[]={
   {3.05f,.008f,-.01f,.01f},{2.87f,.13f,-.14f,.14f},{2.67f,.25f,-.21f,.21f},
   {2.60f,.40f,-.20f,.25f},{2.10f,.49f,-.26f,.34f},{1.45f,.53f,-.36f,.45f},
   {1.10f,.57f,-.62f,.49f},{.65f,.62f,-.82f,.53f},{0,.63f,-.83f,.55f},
   {-.80f,.55f,-.75f,.49f},{-1.60f,.42f,-.60f,.40f},{-2.50f,.29f,-.49f,.30f},
   {-3.30f,.21f,-.48f,.22f},{-4.00f,.13f,-.45f,.14f},{-4.63f,.008f,-.07f,.04f}};
  loft(fuselage,15,-1);
  windshield(fuselage,15,1.39f,.70f);
  for(int sign=-1;sign<=1;sign+=2){
   sideWindow(fuselage,15,.56f,-.21f,-.99f,-.08f,sign);
   const SkinPoint door[]={{.70f,-1.15f},{-.27f,-1.08f},{-.38f,.62f},{.49f,.72f},{.70f,-1.15f}};
   skinStroke(fuselage,15,door,5,sign);
   const SkinPoint handle[]={{-.08f,.15f},{.08f,.15f}};skinStroke(fuselage,15,handle,2,sign);
   const SkinPoint baggage[]={{-1.12f,-.15f},{-1.65f,-.12f},{-1.65f,.48f},{-1.12f,.48f},{-1.12f,-.15f}};
   if(sign<0)skinStroke(fuselage,15,baggage,5,sign);
   float mirror=sign>0?0:-PI,factor=sign>0?1:-1;
   patch(fuselage,15,(SkinPoint){-.30f,mirror-factor*.16f},(SkinPoint){-1.07f,mirror-factor*.32f},
    (SkinPoint){-1.03f,mirror-factor*.55f},(SkinPoint){-.42f,mirror-factor*.87f},2);
   wing(sign,.66f,1.57f,.49f,.81f,aircraft[0].span/2,.48f,.29f,0);
   rootFairing(fuselage,15,sign,.66f,1.57f,.49f,.81f,aircraft[0].span/2,.48f,.29f);
   wing(sign,-3.52f,.82f,-3.72f,.48f,1.98f,-.13f,-.17f,0);
   /* Swept fin outline follows the side drawing; rounded top is approximated. */
   float y=sign*.028f;
   panel((Vec){-3.38f,y,-.47f},(Vec){-4.60f,y,-.10f},(Vec){-4.8748f,y,-1.62f},(Vec){-4.26f,y,-1.60f},0);
   panel((Vec){-4.46f,y+sign*.002f,-1.60f},(Vec){-4.8748f,y+sign*.002f,-1.62f},(Vec){-4.84f,y+sign*.002f,-1.39f},(Vec){-4.36f,y+sign*.002f,-1.39f},1);
  }
  const Vec fin[4]={{-3.38f,0,-.47f},{-4.60f,0,-.10f},{-4.8748f,0,-1.62f},{-4.26f,0,-1.60f}};
  for(int i=0;i<4;i++){Vec a=fin[i],b=fin[(i+1)%4],c=b,d=a;a.y=b.y=-.028f;c.y=d.y=.028f;panel(a,b,c,d,0);}
  /* Small dorsal VHF and belly antenna silhouettes. */
  panel((Vec){-.9f,-.015f,-.78f},(Vec){-1.2f,-.015f,-.74f},(Vec){-1.15f,-.015f,-1.02f},(Vec){-1.02f,-.015f,-1.02f},0);
  panel((Vec){-1.6f,-.015f,.40f},(Vec){-1.8f,-.015f,.37f},(Vec){-1.83f,-.015f,.65f},(Vec){-1.74f,-.015f,.65f},0);
  /* Cowl cooling inlets flank the spinner. */
  for(int sign=-1;sign<=1;sign+=2)panel((Vec){2.608f,sign*.26f,-.12f},(Vec){2.608f,sign*.36f,-.09f},(Vec){2.608f,sign*.35f,.02f},(Vec){2.608f,sign*.23f,.01f},3);
  dynamicPart=(AircraftDynamicPart){faceCount,6,{2.68f,0,0},1};
  float angle=fmodf(propAngle,2*PI);
  for(int j=0;j<3;j++){
   float a=angle+j*2*PI/3,dy=cosf(a),dz=sinf(a);
   panel((Vec){2.68f,dy*.18f,dz*.18f},(Vec){2.68f,dy*.89f-.09f*dz,dz*.89f+.09f*dy},(Vec){2.68f,dy*.99f+.03f*dz,dz*.99f-.03f*dy},(Vec){2.68f,dy*.25f+.08f*dz,dz*.25f-.08f*dy},3);
  }
 }
 if(s->gearActual>.05f){
  float retract=s->aircraft?1-s->gearActual:0;
  gearLeg(s->aircraft?2.8f:1.94f,0,.47f-retract*.9f,!s->aircraft);
  gearLeg(-.18f,s->aircraft?-1.70f:-1.40f,.48f-retract*.9f,!s->aircraft);
  gearLeg(-.18f,s->aircraft?1.70f:1.40f,.48f-retract*.9f,!s->aircraft);
 }
 return faceCount;
}


/* v33 imported exteriors (tools/import_aircraft_v33.py): 737 MAX (mesh 2)
   F-16 (mesh 3) and A321 (mesh 4). Header <7I4f>: faces, dynamic first/count (unused),
   gear first/count, 2 reserved, gear split x, nose/main gear top z.
   Gear faces fold up toward their bay as gearActual goes to 0 and are
   dropped when retracted. No flap deformation or moving parts. */
static int buildImportedV33(const Flight *s,int mesh,AircraftFace *destination,int capacity){
 const unsigned char *raw=mesh==2?b737_imported_mesh_bin:mesh==3?f16_imported_mesh_bin:a321_imported_mesh_bin;
 const uint32_t *header=(const uint32_t*)raw;const float *h=(const float*)(raw+28);
 const AircraftFace *source=(const AircraftFace*)(raw+44);
 faces=destination;faceCapacity=capacity;faceCount=0;dynamicPart=(AircraftDynamicPart){0};
 int count=(int)header[0],gearFirst=(int)header[3];if(count>capacity)return 0;
 float gear=clampf(s->gearActual,0,1);
 if(gear<.05f)count=gearFirst;
 memcpy(destination,source,count*sizeof(AircraftFace));faceCount=count;
 if(gear<1)for(int i=gearFirst;i<count;i++){
  float x=(faces[i].v[0].p.x+faces[i].v[1].p.x+faces[i].v[2].p.x)/3,top=x>=h[0]?h[1]:h[2];
  for(int j=0;j<3;j++){Vec *p=&faces[i].v[j].p;p->z=top+(p->z-top)*gear;}
 }
 return faceCount;
}
/* Exterior meshes converted from the licensed sources documented in
   third_party/aircraft/ATTRIBUTION.md. The original procedural builder is
   retained as source reference; this entry point uses the imported models. */
int buildAircraftMesh(const Flight *s,float propAngle,AircraftFace *destination,int capacity){
 int mesh=aircraft[s->aircraft].mesh;
 if(mesh>=2)return buildImportedV33(s,mesh,destination,capacity);
 const unsigned char *raw=mesh?sf50_imported_mesh_bin:sr22_imported_mesh_bin;
 const uint32_t *header=(const uint32_t*)raw;const float *pivot=(const float*)(raw+12);
 const AircraftFace *source=(const AircraftFace*)(raw+24);
 faces=destination;faceCapacity=capacity;faceCount=0;dynamicPart=(AircraftDynamicPart){0};
 int count=(int)header[0];if(count>capacity)return 0;
 memcpy(destination,source,count*sizeof(AircraftFace));faceCount=count;
 float flap=clampf(s->flapActual,0,1)*.58f;
 for(int i=0;i<count;i++)for(int j=0;j<3;j++){
  Vec *p=&faces[i].v[j].p;float y=fabsf(p->y);
  if(y>.65f&&y<3.5f&&p->z>.02f&&p->z<.65f&&p->x<-.55f&&p->x>-1.5f){
   float weight=clampf((y-.65f)/.25f,0,1)*clampf((3.5f-y)/.3f,0,1);
   /* Fade to zero at every selection boundary so nearby shell pieces do
      not jump apart when a flap crosses a hard spatial mask. */
   weight*=clampf((p->x+1.5f)/.08f,0,1)*clampf((p->z-.02f)/.06f,0,1)*clampf((.65f-p->z)/.06f,0,1);
   float arm=fminf(-.55f-p->x,.72f),angle=flap*weight;
   p->x+=arm*(1-cosf(angle));p->z+=arm*sinf(angle);
  }
 }
 if(!mesh){
  dynamicPart=(AircraftDynamicPart){header[1],header[2],{pivot[0],pivot[1],pivot[2]},1};
  float co=cosf(propAngle),si=sinf(propAngle);
  for(int i=dynamicPart.firstTriangle;i<dynamicPart.firstTriangle+dynamicPart.triangleCount;i++)for(int j=0;j<3;j++){
   Vec *p=&faces[i].v[j].p;float y=p->y-pivot[1],z=p->z-pivot[2];p->y=pivot[1]+y*co-z*si;p->z=pivot[2]+y*si+z*co;
  }
 }else{
  /* Lightweight rotating inlet fan and animated gear supplement the fixed
     source exterior. Door/gear mechanism animation is approximate. */
  dynamicPart=(AircraftDynamicPart){faceCount,10,{-1.45f,0,-.90f},.3f};
  for(int blade=0;blade<10;blade++){
   float a=blade*2*PI/10+propAngle*.3f;
   triangle(material((Vec){-1.45f,.055f*cosf(a),-.90f+.055f*sinf(a)},4),material((Vec){-1.45f,.25f*cosf(a+.16f),-.90f+.25f*sinf(a+.16f)},4),material((Vec){-1.45f,.25f*cosf(a+.36f),-.90f+.25f*sinf(a+.36f)},4));
  }
  if(s->gearActual>.05f){
   float retract=1-s->gearActual;gearLeg(2.8f,0,.47f-retract*.9f,0);gearLeg(-.18f,-1.70f,.48f-retract*.9f,0);gearLeg(-.18f,1.70f,.48f-retract*.9f,0);
  }
 }
 return faceCount;
}
