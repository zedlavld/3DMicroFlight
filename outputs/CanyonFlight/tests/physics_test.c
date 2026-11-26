#include "flight.h"
#include "camera.h"
#include "aircraft_mesh.h"
#include "scenery.h"
#include "terrain.h"
#include "imagery.h"
#include "stream_plan.h"
#include "corridor_index_bin.h"
#include "instruments.h"
#define PI 3.14159265359f
#define CHECK(x) do {checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
volatile int checks,failures,firstFailure;
volatile float diagnostic[24];
AircraftFace meshOutput[2][AIRCRAFT_FACE_CAPACITY];
volatile int meshCounts[2];
volatile int meshCapacity=AIRCRAFT_FACE_CAPACITY;
AircraftFace flapScratch[AIRCRAFT_FACE_CAPACITY];
WorldVertex worldOutput[3][WORLD_VERTEX_CAPACITY];
volatile int worldCounts[3];
volatile int worldCapacity=WORLD_VERTEX_CAPACITY;
WorldVertex partScratch[SCENERY_PART_CAPACITY];
volatile int maxPartVertices,maxImageryMaterialJob;
volatile int regionalPhotoTotals[2];
volatile int worldTerrainCounts[3],imageryCounts[3];
ImageryVertex imageryOutput[90000];int imageryMaterials[30000];
static ImageryVertex imageryScratch[30];
static float area(Vec a,Vec b,Vec c){return fabsf((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x));}
static int near(float a,float b,float t){return fabsf(a-b)<t;}
static float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
int main(void){
 /* Test presets against the actual conditioned terrain at every airport,
    especially runway slope during the ground settle and the final start's
    distance from the threshold rather than from runway centre. */
 for(int type=0;type<2;type++)for(int ap=0;ap<3;ap++)for(int phase=0;phase<PHASE_COUNT;phase++){
  Flight preset;resetFlightPhase(&preset,type,ap,phase);CHECK(preset.startPhase==phase&&isfinite(preset.ias));
  CHECK(near(preset.qw*preset.qw+preset.qx*preset.qx+preset.qy*preset.qy+preset.qz*preset.qz,1,.0001f));
  if(phase==PHASE_TAKEOFF){
   preset.brakeLeft=preset.brakeRight=1;for(int i=0;i<1200;i++)stepFlight(&preset,1/120.f,0,0,0);
   CHECK(!preset.crashed&&preset.onGround&&fabsf(preset.vel.z)<.1f);
  }else CHECK(preset.pos.z>terrain(preset.pos.x,preset.pos.y)+10);
  if(phase==PHASE_FINAL){float distance=hypotf(preset.pos.x-airports[ap].east,preset.pos.y-airports[ap].north);CHECK(near(distance,airports[ap].length/2+1400,.05f));if(type)CHECK(near(preset.mass,2517.4377f,.01f));}
 }
 CHECK(chooseImageryRegion((Vec){10624,32256,2000},0,0,0)==1);
 CHECK(chooseImageryRegion((Vec){-32768,128000,2500},1,1,1)==2);
 CHECK(chooseImageryRegion((Vec){70000,80000,2000},0,0,0)==0);
 CHECK(chooseImageryRegion((Vec){0,0,2000},0,2,0)==2);
 CHECK(near(imageryRegionOrigin(2).y,128000,.1f));
 /* Independent air-data and navigation invariants. */
 CHECK(calibratedAirspeed(0,atmosphere(0,0))==0);
 for(int speed=10;speed<=180;speed+=10)CHECK(near(calibratedAirspeed(speed,atmosphere(0,0)),speed/.514444444f,.003f));
 Atmosphere highAir=atmosphere(8500,0);float jetCas=calibratedAirspeed(145,highAir),jetEas=145*sqrtf(highAir.density/1.225f)/.514444444f;
 CHECK(jetCas>jetEas&&jetCas<145/.514444444f);
 Flight instrument;resetFlight(&instrument,1,0);setAttitude(&instrument,0,0,0);instrument.vel=(Vec){0,100,0};instrument.wind=(Vec){10,0,0};
 PfdData readout=readPfd(&instrument);CHECK(near(readout.groundSpeed,100/.514444444f,.001f));CHECK(near(readout.windFrom,270,.001f));CHECK(near(readout.heading,0,.001f)&&near(readout.track,0,.001f));CHECK(readout.tas>readout.groundSpeed);
 instrument.beta=0;instrument.rudder=.5f;CHECK(readPfd(&instrument).slip>0);instrument.rudder=-.5f;CHECK(readPfd(&instrument).slip<0);instrument.rudder=0;
 float heading=airports[0].heading*PI/180;
 instrument.pos.x=airports[0].east-1000*sinf(heading)+1852*cosf(heading);instrument.pos.y=airports[0].north-1000*cosf(heading)-1852*sinf(heading);
 readout=readPfd(&instrument);CHECK(near(readout.cdi,-1,.0001f)&&readout.to);CHECK(readout.distance>1);
 instrument.pos.z=0;instrument.flapActual=0;instrument.gearActual=0;CHECK(near(pfdAirspeedLimit(&instrument),250,.001f));instrument.gearActual=1;CHECK(near(pfdAirspeedLimit(&instrument),210,.001f));instrument.flapActual=1;CHECK(near(pfdAirspeedLimit(&instrument),150,.001f));
 for(int mode=1;mode<=2;mode++)for(int i=0;i<120;i++){
  Vec at={10000,32000,3000},a=sampleWeatherWind(mode,at,i*.5f),b=sampleWeatherWind(mode,at,i*.5f);
  CHECK(a.x==b.x&&a.y==b.y&&a.z==b.z);CHECK(isfinite(a.x)&&fabsf(a.y)<2&&fabsf(a.z)<1&&a.x>0&&a.x<12);
 }
 for(int type=0;type<2;type++){
  Flight gust;resetFlight(&gust,type,0);setWeather(&gust,2);float mass=gust.mass;
  for(int i=0;i<1200;i++)stepFlight(&gust,1/120.f,0,0,0);
  CHECK(isfinite(gust.pos.z)&&isfinite(gust.ias)&&gust.mass==mass);CHECK(gust.weatherTime>9.9f&&gust.weatherTime<10.1f);
  Vec velocity=gust.vel;setWeather(&gust,0);CHECK(gust.wind.x==0&&gust.wind.y==0&&gust.wind.z==0);CHECK(gust.vel.x==velocity.x&&gust.vel.y==velocity.y&&gust.vel.z==velocity.z);
 }
 /* Translating a photo region must preserve triangle partition area, world
    positions and UVs. The source triangle crosses several region boundaries. */
 WorldVertex untranslated[3]={{{-3000,-3000,1000},{1,1,1}},{{3000,-3000,1200},{1,1,1}},{{0,3000,1100},{1,1,1}}};
 ImageryVertex localPhoto[30],movedPhoto[30];
 Vec origins[2]={{10624,32256,0},{-32768,128000,0}};
 for(int zone=0;zone<2;zone++)for(int region=0;region<IMAGERY_REGIONS;region++){
  setImageryOrigin((Vec){0});int local=clipImageryTriangle(untranslated,region,localPhoto,30,(Vec){0,0,2000});
  WorldVertex moved[3];for(int i=0;i<3;i++){moved[i]=untranslated[i];moved[i].p.x+=origins[zone].x;moved[i].p.y+=origins[zone].y;}
  setImageryOrigin(origins[zone]);int count=clipImageryTriangle(moved,region,movedPhoto,30,(Vec){origins[zone].x,origins[zone].y,2000});CHECK(local==count);
  for(int i=0;i<count;i++){CHECK(near(localPhoto[i].p.x,movedPhoto[i].p.x-origins[zone].x,.02f));CHECK(near(localPhoto[i].p.y,movedPhoto[i].p.y-origins[zone].y,.02f));CHECK(near(localPhoto[i].uv[0],movedPhoto[i].uv[0],.00002f));CHECK(near(localPhoto[i].uv[1],movedPhoto[i].uv[1],.00002f));}
 }
 setImageryOrigin((Vec){0});

 CameraPose visibilityCamera={{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
 CHECK(cameraSphereVisible(&visibilityCamera,(Vec){0,0,100},1,0,40));
 CHECK(!cameraSphereVisible(&visibilityCamera,(Vec){0,0,-100},1,0,40));
 CHECK(!cameraSphereVisible(&visibilityCamera,(Vec){200,0,100},1,0,40));
 CHECK(!cameraSphereVisible(&visibilityCamera,(Vec){0,100,100},1,0,40));
 CHECK(!cameraSphereVisible(&visibilityCamera,(Vec){0,0,150000},1,0,40));
 CHECK(cameraSphereVisible(&visibilityCamera,(Vec){0,0,0},10,0,40));
 /* A range containing any sampled visible point must survive rejection,
    including both eyes at maximum supported separation/convergence. */
 for(int stereo=0;stereo<2;stereo++)for(int depth=1;depth<=4;depth++){
  float z=depth*1000.f,iod=stereo?2.4f:0,focus=14;
  for(int side=-1;side<=1;side+=2)for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
   Vec point={x*z*200/220.f+side*iod*.5f*(1-z/focus),y*z*120/220.f,z};
   Vec center={point.x+30,point.y-40,point.z+20};
   CHECK(cameraSphereVisible(&visibilityCamera,center,54,iod,focus));
  }
 }
 for(int original=0;original<2;original++){
  Flight s;resetFlight(&s,original,2);s.pos=(Vec){1234,5678,3000};s.vel=(Vec){25,80,-2};s.wind=(Vec){4,6,1};s.isaOffset=8;
  setAttitude(&s,.2f,.08f,1.5f);s.throttle=.65f;s.engine=.61f;s.flap=.5f;s.flapActual=.3f;s.p=.2f;s.q=-.1f;s.r=.04f;
  s.gear=0;s.gearActual=.2f;float qw=s.qw,qx=s.qx,qy=s.qy,qz=s.qz;
  switchAircraft(&s,1-original);
  CHECK(s.aircraft==1-original&&s.airport==2);CHECK(s.mass==aircraft[1-original].mass);
  CHECK(s.pos.x==1234&&s.pos.y==5678&&s.pos.z==3000);CHECK(s.vel.x==25&&s.vel.y==80&&s.vel.z==-2);
  CHECK(s.wind.x==4&&s.wind.y==6&&s.wind.z==1&&s.isaOffset==8);
  CHECK(s.qw==qw&&s.qx==qx&&s.qy==qy&&s.qz==qz);CHECK(s.p==0&&s.q==0&&s.r==0);
  CHECK(s.throttle==.65f&&s.engine==.61f&&s.flap==.5f&&s.flapActual==.3f);
  CHECK(s.aircraft?(s.gear==0&&s.gearActual==.2f):(s.gear==1&&s.gearActual==1));
  Aero a=evaluateAero(&s);CHECK(near(s.ias,a.ias,.0001f)&&near(s.alpha,a.alpha,.0001f));CHECK(isfinite(s.loadFactor)&&isfinite(s.stallWarning));
  int type=s.aircraft;float mass=s.mass,trim=s.trim;switchAircraft(&s,-1);switchAircraft(&s,2);switchAircraft(&s,type);
  CHECK(s.aircraft==type&&s.mass==mass&&s.trim==trim&&s.pos.z==3000);
  s.crashed=1;switchAircraft(&s,original);CHECK(s.crashed==1&&s.pos.z==3000);
 }

 for(int tile=0;tile<4;tile++){
  float uv[4];imageryCoarseTransform(tile,uv);
  for(int i=0;i<=8;i++){
   float e=-2048+(tile%2)*2048+i*256,n=-2048+(tile/2)*2048+i*256;
   float u=(1+(e-(-2048+(tile%2)*2048))*1022/2048)/1024;
   float v=(1+(n-(-2048+(tile/2)*2048))*1022/2048)/1024;
   float coarseU=(1+(e-(-8192+(tile%2)*8192))*510/8192)/512;
   float coarseV=(1+(n-(-8192+(tile/2)*8192))*510/8192)/512;
   CHECK(near(u*uv[0]+uv[2],coarseU,.000001f));CHECK(near(v*uv[1]+uv[3],coarseV,.000001f));
  }
 }

 for(int i=0;i<3;i++){
  float focus=i==0?14.f:i==1?160.f:120.f;
  float lz,lw,rz,rw;stereoProjectionTerms(-focus*.015f,focus,1.1f,&lz,&lw);stereoProjectionTerms(focus*.015f,focus,1.1f,&rz,&rw);
  CHECK(near(lw-lz*focus,0,.00001f));CHECK(near(rw-rz*focus,0,.00001f));
  CHECK(near(lz,-rz,.00001f)&&near(lw,-rw,.00001f));
  CHECK(lw-lz*focus/2<0&&rw-rz*focus/2>0);
  CHECK(lw-lz*focus*2>0&&rw-rz*focus*2<0);
  stereoProjectionTerms(0,focus,1.1f,&lz,&lw);CHECK(lz==0&&lw==0);
 }

 Atmosphere sl=atmosphere(0,0),hi=atmosphere(11000,0),hot=atmosphere(2000,20),standard=atmosphere(2000,0);
 CHECK(near(sl.density,1.225f,.001f));CHECK(near(sl.pressure,101325,1));CHECK(near(hi.temperature,216.65f,.01f));CHECK(near(hi.pressure,22632,5));
 CHECK(hot.density<standard.density);CHECK(near(hot.pressure,standard.pressure,1));
 for(int type=0;type<2;type++){
  Flight s;resetFlight(&s,type,0);
  s.gearActual=1;meshCounts[type]=buildAircraftMesh(&s,.5f,meshOutput[type],AIRCRAFT_FACE_CAPACITY);
  AircraftDynamicPart moving=aircraftDynamicPart();CHECK(moving.triangleCount>0&&moving.firstTriangle>=0&&moving.firstTriangle+moving.triangleCount<=meshCounts[type]);
  CHECK(meshCounts[type]>400&&meshCounts[type]<AIRCRAFT_FACE_CAPACITY);
  float minx=100,maxx=-100,miny=100,maxy=-100;
  for(int face=0;face<meshCounts[type];face++)for(int vertex=0;vertex<3;vertex++){
   ModelVertex v=meshOutput[type][face].v[vertex];
   CHECK(isfinite(v.p.x)&&isfinite(v.p.y)&&isfinite(v.p.z));CHECK(v.u>=0&&v.u<=1&&v.v>=0&&v.v<=1);
   minx=fminf(minx,v.p.x);maxx=fmaxf(maxx,v.p.x);miny=fminf(miny,v.p.y);maxy=fmaxf(maxy,v.p.y);
  }
  CHECK(near(maxy-miny,aircraft[type].span,.001f));CHECK(near(maxx-minx,aircraft[type].length,.05f));
  s.flapActual=1;int flapCount=buildAircraftMesh(&s,.5f,flapScratch,AIRCRAFT_FACE_CAPACITY);float maxDeflection=0;
  CHECK(flapCount==meshCounts[type]);
  for(int i=0;i<flapCount;i++)for(int j=0;j<3;j++)maxDeflection=fmaxf(maxDeflection,flapScratch[i].v[j].p.z-meshOutput[type][i].v[j].p.z);
  CHECK(maxDeflection>.10f&&maxDeflection<.50f);
  s.flapActual=0;s.gearActual=0;int retracted=buildAircraftMesh(&s,.5f,flapScratch,AIRCRAFT_FACE_CAPACITY);
  if(type)CHECK(retracted<meshCounts[type]);else CHECK(retracted==meshCounts[type]);
  resetFlight(&s,type,0);setAttitude(&s,0,0,PI/2);
  Vec f=bodyWorld(&s,(Vec){1,0,0}),r=bodyWorld(&s,(Vec){0,1,0}),u=bodyWorld(&s,(Vec){0,0,-1});
  CHECK(near(f.x,1,.0001f));CHECK(near(f.y,0,.0001f));CHECK(near(dot(f,r),0,.0001f));CHECK(near(dot(u,u),1,.0001f));
  setAttitude(&s,.6f,1.56f,2);f=bodyWorld(&s,(Vec){1,0,0});r=bodyWorld(&s,(Vec){0,1,0});CHECK(near(dot(f,r),0,.0001f));
  setAttitude(&s,0,0,0);s.pos.z=0;s.vel=(Vec){0,70,0};s.wind=(Vec){0,0,0};s.engine=.6f;s.trim=0;
  Aero base=evaluateAero(&s);s.wind=(Vec){8,-11,2};s.vel=(Vec){8,59,2};Aero wind=evaluateAero(&s);
  CHECK(near(base.force.x,wind.force.x,.01f));CHECK(near(base.force.z,wind.force.z,.01f));CHECK(near(base.ias,wind.ias,.001f));
  s.wind=(Vec){0,0,0};s.vel=(Vec){0,70,0};s.elevator=.2f;Aero pull=evaluateAero(&s);s.elevator=-.2f;Aero push=evaluateAero(&s);CHECK(pull.moment.y>push.moment.y);
  s.elevator=0;s.rudder=.3f;Aero right=evaluateAero(&s);s.rudder=-.3f;Aero left=evaluateAero(&s);CHECK(right.moment.z>left.moment.z);
  s.rudder=0;s.aileron=.3f;right=evaluateAero(&s);s.aileron=-.3f;left=evaluateAero(&s);CHECK(right.moment.x>left.moment.x);s.aileron=0;
  const float published[2][3]={{70,64,60},{86,77,67}};
  for(int flap=0;flap<3;flap++){
   s.flapActual=flap*.5f;float maxCL=-100;
   for(int i=0;i<2000;i++){
    float alpha=i*.0003f;setAttitude(&s,0,alpha,0);Aero a=evaluateAero(&s);if(a.cl>maxCL)maxCL=a.cl;
   }
   float measured=sqrtf(2*s.mass*9.80665f/(1.225f*aircraft[type].area*maxCL))/.514444444f;
   diagnostic[type*3+flap]=measured;CHECK(near(measured,published[type][flap],.1f));
   float wings=stallSpeedCAS(&s,0),bank=stallSpeedCAS(&s,PI/3);CHECK(near(bank/wings,sqrtf(2),.001f));
   s.mass=aircraft[type].mass*.8f;CHECK(near(stallSpeedCAS(&s,0)/wings,sqrtf(.8f),.001f));s.mass=aircraft[type].mass;
  }
  resetFlight(&s,type,0);float start=s.pos.z,initialIAS=s.ias;
  for(int i=0;i<7200;i++)stepFlight(&s,1/120.f,0,0,0);
  diagnostic[6+type*3]=s.pos.z-start;diagnostic[7+type*3]=s.ias;diagnostic[8+type*3]=s.roll;
  CHECK(isfinite(s.pos.z)&&isfinite(s.ias)&&isfinite(s.roll));CHECK(near(s.qw*s.qw+s.qx*s.qx+s.qy*s.qy+s.qz*s.qz,1,.0001f));
  CHECK(s.ias>initialIAS*.95f&&s.ias<initialIAS*1.05f);CHECK(fabsf(s.pos.z-start)<10);CHECK(fabsf(s.roll)<.05f);
  resetFlight(&s,type,0);s.pos.z=10000;setAttitude(&s,0,1.55f,0);s.vel=(Vec){0,20,60};s.q=.6f;
  for(int i=0;i<1200;i++)stepFlight(&s,1/120.f,.1f,.1f,-.1f);
  CHECK(isfinite(s.pitch)&&isfinite(s.yaw)&&isfinite(s.roll));CHECK(near(s.qw*s.qw+s.qx*s.qx+s.qy*s.qy+s.qz*s.qz,1,.0001f));
  View view={0};resetView(&view);CHECK(near(view.distance,32,.001f));
  view.exterior=1;zoomView(&view,10,1);CHECK(near(view.distance,14,.001f));zoomView(&view,10,-1);CHECK(near(view.distance,160,.001f));
  resetView(&view);CHECK(near(view.distance,160,.001f));view.distance=32;view.exterior=0;
  CameraPose c=makeCamera(&s,&view);Vec center=projectCamera(&c,s.pos);CHECK(near(center.x,0,.001f));
  view.exterior=1;for(int j=0;j<400;j++){moveView(&view,.02f,1,.3f);c=makeCamera(&s,&view);center=projectCamera(&c,s.pos);CHECK(near(center.x,0,.01f)&&near(center.y,0,.01f)&&center.z>0);
   if(j%20==0){
    float rows[4][4];Vec anchor={s.pos.x-200,s.pos.y+300,0},point={s.pos.x+125,s.pos.y-75,s.pos.z+40};cameraViewRows(&c,anchor,rows);
    Vec local={point.x-anchor.x,point.y-anchor.y,point.z-anchor.z},expected=projectCamera(&c,point);
    float xx=rows[0][0]*local.x+rows[0][1]*local.y+rows[0][2]*local.z+rows[0][3];
    float yy=rows[1][0]*local.x+rows[1][1]*local.y+rows[1][2]*local.z+rows[1][3];
    float zz=rows[2][0]*local.x+rows[2][1]*local.y+rows[2][2]*local.z+rows[2][3];
    CHECK(near(xx,expected.x,.02f)&&near(yy,expected.y,.02f)&&near(zz,-expected.z,.02f));
    aircraftViewRows(&s,&c,rows);Vec body={2,3,-1},w=bodyWorld(&s,body);w.x+=s.pos.x;w.y+=s.pos.y;w.z+=s.pos.z;expected=projectCamera(&c,w);
    xx=rows[0][0]*body.x+rows[0][1]*body.y+rows[0][2]*body.z+rows[0][3];
    yy=rows[1][0]*body.x+rows[1][1]*body.y+rows[1][2]*body.z+rows[1][3];
    zz=rows[2][0]*body.x+rows[2][1]*body.y+rows[2][2]*body.z+rows[2][3];
    CHECK(near(xx,expected.x,.02f)&&near(yy,expected.y,.02f)&&near(zz,-expected.z,.02f));
   }}
 }
 const float altitudes[3]={0,609.6f,1828.8f},speeds[3]={101,100,98};
 for(int j=0;j<3;j++){
  Flight s;resetFlight(&s,0,0);s.pos.z=altitudes[j];s.engine=1;s.aileron=s.rudder=0;
  float rho=atmosphere(s.pos.z,0).density,speed=speeds[j]*.514444444f/sqrtf(rho/1.225f);
  float cl=2*s.mass*9.80665f/(rho*speed*speed*aircraft[0].area),alpha=(cl-aircraft[0].cl0)/aircraft[0].cla;
  setAttitude(&s,0,alpha,0);s.vel=(Vec){0,speed,0};Aero a=evaluateAero(&s);
  diagnostic[12+j]=(a.force.x*cosf(alpha)+a.force.z*sinf(alpha))*speed/(s.mass*9.80665f)*196.8504f;
  const float publishedClimb[3]={1398,1279,1041};CHECK(fabsf(diagnostic[12+j]/publishedClimb[j]-1)<.05f);
 }
 for(int j=0;j<2;j++){
  Flight s;resetFlight(&s,0,0);s.pos.z=j?1828.8f:609.6f;s.mass=1315.418f;s.aileron=s.rudder=0;
  float rho=atmosphere(s.pos.z,0).density,speed=(j?170:167)*.514444444f;
  s.engine=(j?.68f:.72f)/(1.132f*rho/1.225f-.132f);
  float cl=2*s.mass*9.80665f/(rho*speed*speed*aircraft[0].area),alpha=(cl-aircraft[0].cl0)/aircraft[0].cla;
  setAttitude(&s,0,alpha,0);s.vel=(Vec){0,speed,0};Aero a=evaluateAero(&s);
  diagnostic[15+j]=(a.force.x*cosf(alpha)+a.force.z*sinf(alpha))*speed/1000;CHECK(fabsf(diagnostic[15+j])<5);
 }
 CHECK(terrainContains(0,0));CHECK(terrainContains(-40000,145000));CHECK(!terrainContains(500000,0));
 CHECK(rawElevation(10500,56000)>3200);CHECK(rawElevation(-38000,141000)<1800);
 for(int ap=0;ap<3;ap++){
  const Airport *airport=&airports[ap];float heading=airport->heading*PI/180;
  for(int j=-1;j<=1;j++){
   float along=j*airport->length*.45f,e=airport->east+sinf(heading)*along,n=airport->north+cosf(heading)*along;
   CHECK(near(terrain(e,n),runwayElevation(airport,along),.01f));
  }
  Flight s;resetFlight(&s,0,ap);
  if(ap==0)s.pos=(Vec){-1600,-2300,2400};
  if(ap==1)s.pos=(Vec){10500,44500,2800};
  if(ap==2)s.pos=(Vec){-41000,133000,3100};
  for(int part=0;part<SCENERY_PARTS;part++){
   int count=buildSceneryPart(&s,partScratch,SCENERY_PART_CAPACITY,part);
   if(part<32)worldTerrainCounts[ap]+=count;
   CHECK(count>=0&&count<SCENERY_PART_CAPACITY);if(count>maxPartVertices)maxPartVertices=count;
   if(part<32)for(int material=-1;material<IMAGERY_TILES;material++){
    int written=0;
    int first,end;imageryMaterialRegions(material,&first,&end);
    for(int i=0;i<count;i+=3)for(int region=first;region<end;region++){
     int n=clipImageryTriangle(partScratch+i,region,imageryScratch,30,s.pos);CHECK(n>=0);if(n>0)written+=n;
    }
    CHECK(written<=SCENERY_PART_CAPACITY*2);if(written>maxImageryMaterialJob)maxImageryMaterialJob=written;
   }

  }
  worldCounts[ap]=buildScenery(&s,worldOutput[ap],WORLD_VERTEX_CAPACITY);
  CHECK(worldCounts[ap]>25000&&worldCounts[ap]<WORLD_VERTEX_CAPACITY);
  for(int i=0;i<worldCounts[ap];i+=31){WorldVertex v=worldOutput[ap][i];CHECK(isfinite(v.p.z)&&v.p.z>-100&&v.p.z<4500); /* v1.1: US-wide ground (was > 400 m, Arizona only); Death Valley -86 m */CHECK(v.color[0]>=0&&v.color[0]<=1&&v.color[1]>=0&&v.color[1]<=1);}
 }
 /* The clipping partition must cover every source terrain triangle exactly
    once, preserve its plane, and stay inside the texture gutters and buffers. */
 for(int scene=0;scene<3;scene++){
  int total=0;
  for(int i=0;i<worldTerrainCounts[scene];i+=3){
   WorldVertex *input=worldOutput[scene]+i;float sum=0,expected=area(input[0].p,input[1].p,input[2].p);
   for(int region=0;region<IMAGERY_REGIONS;region++){
    int count=clipImageryTriangle(input,region,imageryScratch,30,(Vec){0,0,2400});CHECK(count>=0&&count%3==0);if(count<0)continue;
    for(int j=0;j<count;j+=3){
     sum+=area(imageryScratch[j].p,imageryScratch[j+1].p,imageryScratch[j+2].p);
     if(scene==0){
      CHECK(total+j+3<=90000);
      if(total+j+3<=90000){for(int k=0;k<3;k++)imageryOutput[total+j+k]=imageryScratch[j+k];imageryMaterials[(total+j)/3]=imageryMaterial(region);}
     }
    }
    for(int j=0;j<count;j++){
     ImageryVertex v=imageryScratch[j];CHECK(isfinite(v.p.z)&&v.color[3]>=0&&v.color[3]<=1);
     if(region>=4){
      if(fabsf(v.p.x)==8192||fabsf(v.p.y)==8192)CHECK(v.color[3]==0);
      if(region>=IMAGERY_FINE_REGION&&(fabsf(v.p.x)==2048||fabsf(v.p.y)==2048))CHECK(v.color[3]==0);
      CHECK(v.uv[0]>=1/1024.f-.00001f&&v.uv[0]<=1023/1024.f+.00001f);CHECK(v.uv[1]>=1/1024.f-.00001f&&v.uv[1]<=1023/1024.f+.00001f);}
     /* A barycentric affine plane remains unchanged through clipping. */
     if(i%93==0&&expected>1){
      Vec a=input[0].p,b=input[1].p,c=input[2].p;
      float den=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
      float u=((b.y-c.y)*(v.p.x-c.x)+(c.x-b.x)*(v.p.y-c.y))/den;
      float w=((c.y-a.y)*(v.p.x-c.x)+(a.x-c.x)*(v.p.y-c.y))/den;
      CHECK(near(v.p.z,u*a.z+w*b.z+(1-u-w)*c.z,.02f));
     }
    }
    total+=count;
   }
   CHECK(fabsf(sum-expected)<fmaxf(.05f,expected*.00005f));
  }
  for(int i=worldTerrainCounts[scene];i<worldCounts[scene];i++){
   WorldVertex p=worldOutput[scene][i];if(scene==0){imageryOutput[total]=(ImageryVertex){p.p,{0,0},{p.color[0],p.color[1],p.color[2],1}};imageryMaterials[total/3]=-1;}total++;
  }
  imageryCounts[scene]=total;CHECK(total<90000);
 }
 /* Exercise complete terrain jobs at both new photo origins, including the
    approach start outside the Canyon pack and every clipping material budget. */
 for(int airport=1;airport<=2;airport++){
  Flight scene;resetFlight(&scene,0,airport);setImageryOrigin(origins[airport-1]);int total=0;
  for(int part=0;part<32;part++){
   int count=buildSceneryPart(&scene,partScratch,SCENERY_PART_CAPACITY,part),jobs[9]={0};
   CHECK(count>0&&count<SCENERY_PART_CAPACITY);
   for(int i=0;i<count;i+=3){
    float expected=area(partScratch[i].p,partScratch[i+1].p,partScratch[i+2].p),sum=0;
    for(int region=0;region<IMAGERY_REGIONS;region++){
     int n=clipImageryTriangle(partScratch+i,region,imageryScratch,30,scene.pos);CHECK(n>=0&&n%3==0);
     if(n>0){jobs[imageryMaterial(region)+1]+=n;for(int j=0;j<n;j+=3)sum+=area(imageryScratch[j].p,imageryScratch[j+1].p,imageryScratch[j+2].p);}
    }
    CHECK(fabsf(sum-expected)<fmaxf(.1f,expected*.0001f));
   }
   for(int material=0;material<9;material++){CHECK(jobs[material]<SCENERY_PART_CAPACITY*2);total+=jobs[material];}
  }
  regionalPhotoTotals[airport-1]=total;CHECK(total<90000);
 }
 /* Move the fine window independently through every valid alignment with
    an 8 km coarse tile. Check exact partition area and the fine-to-coarse UV
    transform against world coordinates, including negative grid indices. */
 for(int dx=-4096;dx<=4096;dx+=2048)for(int dy=-4096;dy<=4096;dy+=2048){
  Vec fine={-32768+dx,131072+dy,0},coarse={-32768,131072,0};setImageryWindows(fine,coarse);
  WorldVertex tri[3]={{{coarse.x-12000,coarse.y-10000,500},{1,1,1}},{{coarse.x+14000,coarse.y-9000,500},{1,1,1}},{{coarse.x+1000,coarse.y+13000,500},{1,1,1}}};
  float sum=0,expected=area(tri[0].p,tri[1].p,tri[2].p);
  for(int region=0;region<IMAGERY_REGIONS;region++){
   int count=clipImageryTriangle(tri,region,imageryScratch,30,fine);CHECK(count>=0);
   for(int j=0;j<count;j+=3)sum+=area(imageryScratch[j].p,imageryScratch[j+1].p,imageryScratch[j+2].p);
   int tile=imageryMaterial(region);if(tile>=0&&tile<4){
    float uv[4];imageryCoarseTransform(tile,uv);int c=imageryCoarseTile(tile)-4;
    float left=coarse.x-8192+(c%2)*8192,bottom=coarse.y-8192+(c/2)*8192;
    for(int j=0;j<count;j++){
     CHECK(near(imageryScratch[j].uv[0]*uv[0]+uv[2],(1+(imageryScratch[j].p.x-left)*510/8192)/512,.00001f));
     CHECK(near(imageryScratch[j].uv[1]*uv[1]+uv[3],(1+(imageryScratch[j].p.y-bottom)*510/8192)/512,.00001f));
    }
   }
  }
  CHECK(fabsf(sum-expected)<expected*.00001f);
 }
 /* Coarse-only rendering must cover the fine rectangle too, without a hole. */
 setImageryFineEnabled(0);
 WorldVertex coarseTri[3]={{{-45000,120000,500},{1,1,1}},{{-20000,120000,500},{1,1,1}},{{-30000,145000,500},{1,1,1}}};
 float coarseSum=0;
 for(int region=0;region<IMAGERY_REGIONS;region++){
  int count=clipImageryTriangle(coarseTri,region,imageryScratch,30,(Vec){0});CHECK(count>=0);
  if(region>=IMAGERY_FINE_REGION)CHECK(count==0);
  for(int j=0;j<count;j+=3)coarseSum+=area(imageryScratch[j].p,imageryScratch[j+1].p,imageryScratch[j+2].p);
 }
 CHECK(near(coarseSum,area(coarseTri[0].p,coarseTri[1].p,coarseTri[2].p),100));setImageryFineEnabled(1);
 /* Traverse the shipped route at 100 m intervals. Every required visible
    tile must exist, and the fine window must fit within its coarse window. */
 Vec route[4]={{-45.25601,.21236,0},{10631.13802,32229.87559,0},{-32759.85966,122731.63782,0},{-32768,135168,0}};
 for(int leg=0;leg<3;leg++){
  float length=hypotf(route[leg+1].x-route[leg].x,route[leg+1].y-route[leg].y);
  for(float d=0;d<=length;d+=100){float t=d/length;Vec p={route[leg].x+t*(route[leg+1].x-route[leg].x),route[leg].y+t*(route[leg+1].y-route[leg].y),0};StreamWindow w;streamWindow(p,&w);
   CHECK(fabsf(w.fine.x-w.coarse.x)+2048<=8192&&fabsf(w.fine.y-w.coarse.y)+2048<=8192);
   for(int i=0;i<8;i++)CHECK(streamCatalogHas(corridor_index_bin,corridor_index_bin_size,w.keys[i]));
  }
 }
 CHECK(!streamCatalogHas(corridor_index_bin,corridor_index_bin_size,(StreamKey){0,99999,99999}));
 CHECK(!streamCatalogHas(corridor_index_bin,15,(StreamKey){0,0,0}));
 setImageryOrigin((Vec){0});
 WorldVertex testTri[3]={{{-3000,-3000,1000},{1,1,1}},{{3000,-3000,1000},{1,1,1}},{{0,3000,1000},{1,1,1}}};
 CHECK(clipImageryTriangle(testTri,IMAGERY_FINE_REGION,imageryScratch,0,(Vec){0})==-1);
 CHECK(clipImageryTriangle(testTri,-1,imageryScratch,30,(Vec){0})==-1);
 return failures;
}
