#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "imagery.h"
#include "scenery_stream.h"
#include "terrain_stream.h"
#include "aircraft_render.h"
#include "aircraft_mesh.h"
#include "cockpit.h"
#include "cockpit_cache.h"
#include "cockpit_material_bin.h"
#include "airport_material_bin.h"
#include "scenery.h"
#include "weather_visual.h"
#include "daylight.h"
#include "terrain.h"
#include "airport_lights.h"
#include "light_render.h"
#include "flight.h"
#include "aircraft_shader_shbin.h"
#include "sr22_texture_bin.h"
#include "sf50_texture_bin.h"
#include "b737_texture_bin.h"
#include "f16_texture_bin.h"
#include "a321_texture_bin.h"
#define PI 3.14159265359f
#define MAX_VERTICES (AIRCRAFT_FACE_CAPACITY*3)
/* Body axes in metres. Adapted source assets: third_party/aircraft/ATTRIBUTION.md. */
typedef struct {float p[3],uv[2],color[4];} GPUVertex;
static AircraftFace faces[AIRCRAFT_FACE_CAPACITY];static int faceCount;
static GPUVertex *vbo[2];static int bufferIndex;
static C3D_Tex cockpitTexture;
static C3D_Tex airportTexture;
#define WORLD_DEPTH_TOP .9f /* v35.18: depth split between the world and the cockpit */
static GPUVertex *cockpitVbo[2];static CockpitFace cockpitMesh[COCKPIT_CAPACITY];
static CockpitCache cockpitCache;static int cockpitUploadBytes;
CockpitRenderStats cockpitRenderStats(void){return (CockpitRenderStats){cockpitCache.fixedCount,cockpitCache.liveCount,cockpitUploadBytes};}
static unsigned cockpitFrame;static int cockpitCount;static GPUVertex *preparedCockpit;
static int cachedType=-1;static float cachedFlap=-1,cachedGear=-1,propPhase;static u64 propTime;
static AircraftDynamicPart movingPart;
static unsigned renderFrame,aircraftFrame,worldFrame;
static GPUVertex *preparedAircraft;
static GPUVertex *cloudVbo[2];static unsigned cloudFrame;static int cloudBuffer,cloudCount;
static Vec cloudAnchor;static GPUVertex *preparedCloud;
static float stereoIod,stereoScreen=40;
/* v34: time-of-day tint times the storm darkening (lightning brightens both).
   floor[] keeps a minimum per channel (the cockpit's panel lighting). */
static void sceneLightColor(const Flight *s,const float *floor,float rgb[3]){
 daylightTint(rgb);float storm=weatherStorm(s),flash=weatherFlash(s);
 float k=storm>0?fminf(1,1-.35f*storm+flash*.32f):1;
 for(int i=0;i<3;i++){rgb[i]=fminf(1,rgb[i]*k+flash*.25f);if(floor&&rgb[i]<floor[i])rgb[i]=floor[i];}
}
static void sceneLight(const Flight *s,int stage,const float *floor){
 float rgb[3];sceneLightColor(s,floor,rgb);if(rgb[0]>=.999f&&rgb[1]>=.999f&&rgb[2]>=.999f)return;
 C3D_TexEnv *env=C3D_GetTexEnv(stage);C3D_TexEnvInit(env);
 C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,0);C3D_TexEnvFunc(env,C3D_RGB,GPU_MODULATE);
 C3D_TexEnvColor(env,C2D_Color32((int)(rgb[0]*255),(int)(rgb[1]*255),(int)(rgb[2]*255),255));
}
static void weatherLight(const Flight *s,int stage){sceneLight(s,stage,NULL);}
void beginRendererFrame(void){renderFrame++;}
void setRendererEye(float iod,float screen){stereoIod=iod;stereoScreen=screen;}
/* v31.1: New 3DS hardware drew one-frame wedges from long triangles crossing
   the near plane: the clipped vertices land far outside the rasterizer's range
   and wrap (Azahar's float32 path never showed them). A deeper near plane keeps
   them in range. Exterior view: 6 m (camera stays >=14 m from the aircraft and
   >=3 m above ground). Cockpit view keeps 2 m so the ground seen down through
   the side windows on the runway is not cut away. */
static float worldNear=2;
void setRendererExterior(int exterior){worldNear=exterior?6:2;}
static void projectionForEye(C3D_Mtx *projection){
 Mtx_PerspTilt(projection,2*atanf(120.f/220),C3D_AspectRatioTop,worldNear,140000,false);
 /* Parallel cameras with off-axis convergence, in the rotated framebuffer.
    Incorporate the eye translation here; modelView remains shared. */
 stereoProjectionTerms(stereoIod,stereoScreen,-projection->r[1].x,&projection->r[1].z,&projection->r[1].w);
}
#define RENDER_WORLD_CAPACITY 90000
#define RANGE_VERTICES 768
/* v31.1: compact per-block ranges need far more entries (one per block and
   material); ~32 bytes each, double buffered. */
#define WORLD_DRAW_RANGES 3072
typedef struct {int first,count,tile;Vec center;float radius;} WorldRange;
static WorldRange worldRanges[2][WORLD_DRAW_RANGES];static int worldRangeCount[2];
/* v31.7 diagnostics: a hash of each range's vertex words as the CPU wrote
   them, re-read from memory at autotest captures (rendererVerifyWorld). */
static unsigned rangeHash[2][WORLD_DRAW_RANGES];static int worldBuilds,lastVisibleRanges;
static C3D_Tex groundTextures[IMAGERY_TILES];static bool imageryReady;
static ImageryVertex photoPart[SCENERY_PART_CAPACITY*2];
static int photoError;
static int corridorMode,worldPhoto[2];static StreamWindow worldWindows[2];
static u64 lastWorldBuild; /* osGetTime when the last scene went live */
static int loadBoost,frameParts;static u64 frameStreamTicks,frameBuildTicks,stageTicks[4];
static int shedding,photoDrops,photoSheds,photoCancels,photoFarDrops; /* v31.13 cache-pressure step-down: 1 requested, 2 building */
bool hasGroundImagery(void){return imageryReady;}
static void freeGroundImagery(void){
 for(int i=0;i<IMAGERY_TILES;i++)if(groundTextures[i].data){C3D_TexDelete(&groundTextures[i]);memset(&groundTextures[i],0,sizeof(C3D_Tex));}
 imageryReady=false;
}
static int imageryRegion,sceneryAirport;
static bool loadGroundImagery(int region){
 for(int i=0;i<IMAGERY_TILES;i++){
  char path[128];snprintf(path,sizeof(path),"sdmc:/3ds/CanyonFlight/scenery/%s_%d.cft",imageryRegionName(region),i);
  FILE *file=fopen(path,"rb");if(!file){freeGroundImagery();return false;}
  unsigned char magic[8];u32 header[6];
  int valid=fread(magic,1,8,file)==8&&memcmp(magic,"CFNAIP09",8)==0&&fread(header,4,6,file)==6;
  int dimension=i<4?1024:512,level=i<4?7:6;u32 payload=i<4?2796160:699008;
  valid=valid&&header[0]==(u32)i&&header[1]==(u32)dimension&&header[2]==(u32)level&&header[3]==payload&&header[4]==0&&header[5]==0;
  if(!valid||!C3D_TexInitMipmap(&groundTextures[i],dimension,dimension,GPU_RGB565)){fclose(file);freeGroundImagery();return false;}
  size_t bytes=C3D_TexCalcTotalSize(groundTextures[i].size,groundTextures[i].maxLevel);
  valid=bytes==header[3]&&fread(groundTextures[i].data,1,bytes,file)==bytes&&fgetc(file)==EOF;fclose(file);
  if(!valid){freeGroundImagery();return false;}
  C3D_TexFlush(&groundTextures[i]);C3D_TexSetFilter(&groundTextures[i],GPU_LINEAR,GPU_LINEAR);
  C3D_TexSetFilterMipmap(&groundTextures[i],GPU_LINEAR);C3D_TexSetWrap(&groundTextures[i],GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
 }
 return true;
}
static u16 *rangeIndices; /* 0..RANGE_VERTICES-1, shared by every world range */
/* v31.14: the SF50 showed a phantom triangle between its two tail tips on
   New 3DS XL hardware in some frames (at least 4 of 176 exterior shots in the v31.13
   run; no face in the mesh spans the tail, and its colour changed each
   time). That is the v31.12 terrain fault the other way round: an array draw
   assembled a triangle from the wrong vertices. Every non-terrain mesh now
   draws like terrain: one buffer slice per 768 vertices, indexed. */
static void drawIndexedSlices(GPUVertex *base,int count){
 for(int first=0;first<count;first+=RANGE_VERTICES){
  int length=count-first<RANGE_VERTICES?count-first:RANGE_VERTICES;
  C3D_BufInfo info;BufInfo_Init(&info);BufInfo_Add(&info,base+first,sizeof(GPUVertex),3,0x210);C3D_SetBufInfo(&info);
  C3D_DrawElements(GPU_TRIANGLES,length,C3D_UNSIGNED_SHORT,rangeIndices);
 }
}
static GPUVertex *worldVbo[2];static WorldVertex worldPart[SCENERY_PART_CAPACITY];
static int activeWorld,worldCount[2],streamPart=-1,worldReady;
static Vec worldAnchor[2];static Flight streamFlight;

#define AIRCRAFT_TEXTURES 5 /* mesh index: SR22, SF50, 737, F-16, A321 */
static C3D_Tex textures[AIRCRAFT_TEXTURES];static DVLB_s *shader;static shaderProgram_s program;
static C3D_AttrInfo attributes;static int projectionLocation,viewLocation,coarseUvLocation,eyeOffsetLocation,guardLocation;static bool ready;
/* Every pass sets the eye offset, so it also restores the normal guard. */
static void setEyeOffset(Vec offset){C3D_FVUnifSet(GPU_VERTEX_SHADER,eyeOffsetLocation,offset.x,offset.y,offset.z,0);C3D_FVUnifSet(GPU_VERTEX_SHADER,guardLocation,4,8,0,0);}
bool initAircraftRenderer(void){
 shader=DVLB_ParseFile((u32*)aircraft_shader_shbin,aircraft_shader_shbin_size);if(!shader)return false;
 shaderProgramInit(&program);shaderProgramSetVsh(&program,&shader->DVLE[0]);
 projectionLocation=shaderInstanceGetUniformLocation(program.vertexShader,"projection");
 viewLocation=shaderInstanceGetUniformLocation(program.vertexShader,"modelView");
 coarseUvLocation=shaderInstanceGetUniformLocation(program.vertexShader,"coarseUv");
 eyeOffsetLocation=shaderInstanceGetUniformLocation(program.vertexShader,"eyeOffset");
 guardLocation=shaderInstanceGetUniformLocation(program.vertexShader,"guard");
 AttrInfo_Init(&attributes);AttrInfo_AddLoader(&attributes,0,GPU_FLOAT,3);AttrInfo_AddLoader(&attributes,1,GPU_FLOAT,2);AttrInfo_AddLoader(&attributes,2,GPU_FLOAT,4);
 for(int i=0;i<2;i++){
  cloudVbo[i]=linearAlloc(WEATHER_PUFF_CAPACITY*24*sizeof(GPUVertex));
  cockpitVbo[i]=linearAlloc(COCKPIT_CAPACITY*3*sizeof(GPUVertex));
  worldVbo[i]=linearAlloc(RENDER_WORLD_CAPACITY*sizeof(GPUVertex));
  if(!rangeIndices){rangeIndices=linearAlloc(RANGE_VERTICES*sizeof(u16));
   if(rangeIndices){for(int k=0;k<RANGE_VERTICES;k++)rangeIndices[k]=k;GSPGPU_FlushDataCache(rangeIndices,RANGE_VERTICES*sizeof(u16));}}
  if(!rangeIndices){freeAircraftRenderer();return false;}
  vbo[i]=linearAlloc(MAX_VERTICES*sizeof(GPUVertex));
  if(!cloudVbo[i]||!cockpitVbo[i]||!vbo[i]||!worldVbo[i]){freeAircraftRenderer();return false;}
 }
 static const unsigned char *const textureData[AIRCRAFT_TEXTURES]={sr22_texture_bin,sf50_texture_bin,b737_texture_bin,f16_texture_bin,a321_texture_bin};
 for(int i=0;i<AIRCRAFT_TEXTURES;i++){
  if(!C3D_TexInitMipmap(&textures[i],512,512,GPU_RGB565)){freeAircraftRenderer();return false;}
  memcpy(textures[i].data,textureData[i],699008);C3D_TexFlush(&textures[i]);
  C3D_TexSetFilterMipmap(&textures[i],GPU_LINEAR);
  C3D_TexSetFilter(&textures[i],GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&textures[i],GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
 }
 if(!C3D_TexInit(&cockpitTexture,128,128,GPU_RGB565)){freeAircraftRenderer();return false;}
 memcpy(cockpitTexture.data,cockpit_material_bin,32768);C3D_TexFlush(&cockpitTexture);
 C3D_TexSetFilter(&cockpitTexture,GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&cockpitTexture,GPU_REPEAT,GPU_REPEAT);
 if(!C3D_TexInitMipmap(&airportTexture,128,128,GPU_RGB565)){freeAircraftRenderer();return false;}
 memcpy(airportTexture.data,airport_material_bin,airport_material_bin_size);C3D_TexFlush(&airportTexture);
 C3D_TexSetFilter(&airportTexture,GPU_LINEAR,GPU_LINEAR);C3D_TexSetFilterMipmap(&airportTexture,GPU_LINEAR);
 C3D_TexSetWrap(&airportTexture,GPU_REPEAT,GPU_REPEAT);
 imageryRegion=sceneryAirport=0;setImageryOrigin(imageryRegionOrigin(0));
 corridorMode=initSceneryStream();imageryReady=corridorMode?false:loadGroundImagery(0);
 initTerrainStream(NULL); /* optional: absent tiles fall back to the 500 m base */
 initLightRenderer(); /* optional: no night lights if the shader fails */
 C3D_DepthMap(true,-WORLD_DEPTH_TOP,0); /* v35.18: world passes write depth 0..0.9 */
 ready=true;return true;
}
static void uploadCockpitRange(GPUVertex *gpu,int first,int length){
  for(int i=first;i<first+length;i++){
   Vec a=cockpitMesh[i].v[0],b=cockpitMesh[i].v[1],c0=cockpitMesh[i].v[2];
   float light=1;
   if(i<cockpitCache.fixedCount&&!(fabsf(a.x-b.x)<.00001f&&fabsf(a.x-c0.x)<.00001f&&a.x>.98f&&a.x<1.018f)){
    Vec u={b.x-a.x,b.y-a.y,b.z-a.z},v={c0.x-a.x,c0.y-a.y,c0.z-a.z};
    Vec n={u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
    float norm=sqrtf(n.x*n.x+n.y*n.y+n.z*n.z);
    light=clampf(.70f+.30f*fabsf(n.x*.25f+n.y*.35f-n.z*.90f)/fmaxf(norm,.000001f),.70f,1);
   }
   for(int j=0;j<3;j++){
   Vec p=cockpitMesh[i].v[j];u32 c=cockpitMesh[i].color;
   /* Fine original matte grain; the white atlas corner keeps instrument
      graphics untextured. Flat screen/control artwork lies near x=1m. */
   float u=.03125f,v=.03125f;
   if(p.x<.94f||p.x>1.018f){u=.125f+.7f*(p.y+1)/3;v=.125f+.7f*(p.z+1)/2;}
   gpu[i*3+j]=(GPUVertex){{p.x,p.y,p.z},{u,v},{light*(c&255)/255.f,light*((c>>8)&255)/255.f,light*((c>>16)&255)/255.f,1}};
  }
 }
 GSPGPU_FlushDataCache(gpu+first*3,length*3*sizeof(GPUVertex));
}
void drawVirtualCockpit(const Flight *s,const CameraPose *camera){
 if(!ready)return;
 if(cockpitFrame!=renderFrame){
  cockpitFrame=renderFrame;cockpitUploadBytes=0;
  int dirty=updateCockpitCache(&cockpitCache,s,cockpitMesh,COCKPIT_CAPACITY);
  cockpitCount=cockpitCache.fixedCount+cockpitCache.liveCount;
  if(dirty&COCKPIT_FIXED_DIRTY){for(int i=0;i<2;i++)uploadCockpitRange(cockpitVbo[i],0,cockpitCache.fixedCount);cockpitUploadBytes+=2*cockpitCache.fixedCount*3*sizeof(GPUVertex);}
  if(dirty&COCKPIT_LIVE_DIRTY){
   preparedCockpit=cockpitVbo[renderFrame&1];
   uploadCockpitRange(preparedCockpit,cockpitCache.fixedCount,cockpitCache.liveCount);
   cockpitUploadBytes+=cockpitCache.liveCount*3*sizeof(GPUVertex);
  }
 }

 /* The interior uses its own near plane; terrain colour stays behind the window openings.
    v35.18: no mid-frame depth clear (it was FrameSplit + a depth memory fill). World passes write
    depth 0..0.9 and the cockpit is mapped to 0.9..1, so it is in front of all terrain and still
    depth-sorts against itself. */
 C2D_Flush();C3D_BindProgram(&program);C3D_SetAttrInfo(&attributes);
 C3D_DepthMap(true,-(1-WORLD_DEPTH_TOP),WORLD_DEPTH_TOP); /* near 1, far 0.9 (depth = scale*z/w+offset, z/w -1..0) */
 C3D_BufInfo info;BufInfo_Init(&info);BufInfo_Add(&info,preparedCockpit,sizeof(GPUVertex),3,0x210);C3D_SetBufInfo(&info);
 /* v31.1: 0.07 -> 0.2 m for the same hardware clipping wrap (white slivers
    over the panel). Nearest on-screen cockpit geometry over the full look
    range is 0.339 m (tests/cockpit_near_check.c), so nothing visible is cut. */
 C3D_Mtx projection;Mtx_PerspTilt(&projection,2*atanf(120.f/220),C3D_AspectRatioTop,.2f,40,false);
 stereoProjectionTerms(stereoIod,stereoScreen,-projection.r[1].x,&projection.r[1].z,&projection.r[1].w);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,projectionLocation,&projection);
 Flight cabin=*s;cabin.pos=pilotEyeWorld(s);
 float rows[4][4];aircraftViewRows(&cabin,camera,rows);C3D_Mtx matrix;
 for(int i=0;i<4;i++)matrix.r[i]=FVec4_New(rows[i][0],rows[i][1],rows[i][2],rows[i][3]);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,viewLocation,&matrix);setEyeOffset((Vec){0,0,0});
 C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_ALL);C3D_AlphaTest(false,GPU_ALWAYS,0);C3D_LightEnvBind(NULL);
 for(int i=0;i<6;i++)C3D_TexEnvInit(C3D_GetTexEnv(i));
 C3D_TexEnv *env=C3D_GetTexEnv(0);C3D_TexEnvSrc(env,C3D_Both,GPU_TEXTURE0,GPU_PRIMARY_COLOR,0);C3D_TexEnvFunc(env,C3D_Both,GPU_MODULATE);
 static const float panelLight[3]={.52f,.44f,.38f}; /* instrument and flood lighting */
 sceneLight(s,1,panelLight);
 C3D_TexBind(0,&cockpitTexture);drawIndexedSlices(preparedCockpit,cockpitCount*3);
 C3D_DepthMap(true,-WORLD_DEPTH_TOP,0);
}
/* v35.06: the aircraft's shadow on the ground, for judging height. The model is flattened
   along the sun direction onto the local ground plane; that projection is affine, so it is one
   extra matrix in front of the aircraft matrix (no per-vertex work). Drawn in translucent dark
   grey, once per pixel (stencil), depth-tested so terrain in front hides it. The sun azimuth is
   the southwest of the baked terrain relief; its height follows the time of day. */
static int shadowMatrix(const Flight *s,float rows[4][4],float camRange,float *alphaOut){
 float el=daylightSunElevation();if(el<3)return 0;
 float elr=fminf(fmaxf(el,25),70)*PI/180,ch=cosf(elr);
 Vec L={-.70711f*ch,-.70711f*ch,sinf(elr)};
 Vec P=s->pos;float agl=P.z-terrain(P.x,P.y);if(agl>900)return 0;
 float tt=fmaxf(agl,0)/L.z;Vec Q={P.x-L.x*tt,P.y-L.y*tt,0};
 tt=fmaxf(P.z-terrain(Q.x,Q.y),0)/L.z;Q.x=P.x-L.x*tt;Q.y=P.y-L.y*tt;
 if(tt*ch>2500)return 0;
 Q.z=terrain(Q.x,Q.y);
 float gx=(terrain(Q.x+10,Q.y)-terrain(Q.x-10,Q.y))/20,gy=(terrain(Q.x,Q.y+10)-terrain(Q.x,Q.y-10))/20;
 Vec n={-gx,-gy,1};float nl=n.x*L.x+n.y*L.y+n.z*L.z;if(nl<.05f)return 0;
 float k=(n.x*P.x+n.y*P.y+n.z*P.z)-(n.x*Q.x+n.y*Q.y+n.z*Q.z);
 float lift=.3f+.0015f*camRange;
 Vec a[3]={bodyWorld(s,(Vec){1,0,0}),bodyWorld(s,(Vec){0,1,0}),bodyWorld(s,(Vec){0,0,1})};
 float aL[3],m[3];for(int i=0;i<3;i++){aL[i]=a[i].x*L.x+a[i].y*L.y+a[i].z*L.z;m[i]=a[i].x*n.x+a[i].y*n.y+a[i].z*n.z;}
 for(int i=0;i<3;i++){for(int j=0;j<3;j++)rows[i][j]=(i==j)-aL[i]*m[j]/nl;rows[i][3]=-aL[i]*k/nl+lift*a[i].z;}
 rows[3][0]=rows[3][1]=rows[3][2]=0;rows[3][3]=1;
 float fadeSun=fminf((el-3)/10,1),fadeHeight=1-fminf(fmaxf((agl-150)/750,0),1);
 *alphaOut=.42f*fadeSun*fadeHeight*(1-.75f*weatherStorm(s));
 return *alphaOut>.02f;
}
void drawTexturedAircraft(const Flight *s,const CameraPose *camera){
 if(!ready)return;
 if(aircraftFrame!=renderFrame){
 aircraftFrame=renderFrame;
 float flap=roundf(s->flapActual*20)/20,gear=roundf(s->gearActual*20)/20;
 if(cachedType!=s->aircraft||flap!=cachedFlap||gear!=cachedGear){
  Flight draw=*s;draw.flapActual=flap;draw.gearActual=gear;
  faceCount=buildAircraftMesh(&draw,0,faces,AIRCRAFT_FACE_CAPACITY);movingPart=aircraftDynamicPart();
  for(int buffer=0;buffer<2;buffer++){
   for(int i=0;i<faceCount;i++)for(int j=0;j<3;j++){
    ModelVertex v=faces[i].v[j];vbo[buffer][i*3+j]=(GPUVertex){{v.p.x,v.p.y,v.p.z},{v.u,v.v},{v.light,v.light,v.light,1}};
   }
   GSPGPU_FlushDataCache(vbo[buffer],faceCount*3*sizeof(GPUVertex));
  }
  cachedType=s->aircraft;cachedFlap=flap;cachedGear=gear;
 }
 GPUVertex *gpu=vbo[bufferIndex];bufferIndex=1-bufferIndex;preparedAircraft=gpu;
 u64 now=osGetTime();float dt=propTime?fminf((now-propTime)/1000.f,.1f):0;propTime=now;
 propPhase=fmodf(propPhase+dt*18*(.2f+s->engine),2*PI);
 float angle=propPhase*movingPart.rate,co=cosf(angle),si=sinf(angle);Vec pivot=movingPart.pivot;
 for(int i=movingPart.firstTriangle;i<movingPart.firstTriangle+movingPart.triangleCount;i++)for(int j=0;j<3;j++){
  Vec p=faces[i].v[j].p;float y=p.y-pivot.y,z=p.z-pivot.z;
  gpu[i*3+j].p[1]=pivot.y+y*co-z*si;gpu[i*3+j].p[2]=pivot.z+y*si+z*co;
 }
 GSPGPU_FlushDataCache(gpu+movingPart.firstTriangle*3,movingPart.triangleCount*3*sizeof(GPUVertex));
 }
 GPUVertex *gpu=preparedAircraft;
 C2D_Flush();C3D_BindProgram(&program);C3D_SetAttrInfo(&attributes);
 C3D_BufInfo info;BufInfo_Init(&info);BufInfo_Add(&info,gpu,sizeof(GPUVertex),3,0x210);C3D_SetBufInfo(&info);
 C3D_Mtx projection;projectionForEye(&projection);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,projectionLocation,&projection);
 float aircraftRows[4][4];aircraftViewRows(s,camera,aircraftRows);C3D_Mtx aircraftMatrix;
 for(int i=0;i<4;i++)aircraftMatrix.r[i]=FVec4_New(aircraftRows[i][0],aircraftRows[i][1],aircraftRows[i][2],aircraftRows[i][3]);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,viewLocation,&aircraftMatrix);setEyeOffset((Vec){0,0,0});
 C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_ALL);
 C3D_AlphaTest(false,GPU_ALWAYS,0);C3D_LightEnvBind(NULL);
 for(int i=0;i<6;i++)C3D_TexEnvInit(C3D_GetTexEnv(i));
 C3D_TexEnv *env=C3D_GetTexEnv(0);C3D_TexEnvSrc(env,C3D_Both,GPU_TEXTURE0,GPU_PRIMARY_COLOR,0);C3D_TexEnvFunc(env,C3D_Both,GPU_MODULATE);
  {float S[4][4],alpha,camRange=sqrtf(aircraftRows[0][3]*aircraftRows[0][3]+aircraftRows[1][3]*aircraftRows[1][3]+aircraftRows[2][3]*aircraftRows[2][3]);
  if(shadowMatrix(s,S,camRange,&alpha)){
   C3D_Mtx shadow;for(int i=0;i<4;i++){float r[4];for(int j=0;j<4;j++)r[j]=aircraftRows[i][0]*S[0][j]+aircraftRows[i][1]*S[1][j]+aircraftRows[i][2]*S[2][j]+aircraftRows[i][3]*S[3][j];shadow.r[i]=FVec4_New(r[0],r[1],r[2],r[3]);}
   C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,viewLocation,&shadow);
   C3D_TexEnv *se=C3D_GetTexEnv(0);C3D_TexEnvInit(se);C3D_TexEnvSrc(se,C3D_Both,GPU_CONSTANT,0,0);C3D_TexEnvFunc(se,C3D_Both,GPU_REPLACE);
   C3D_TexEnvColor(se,C2D_Color32(12,14,18,(int)(alpha*255)));
   C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA);
   C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_COLOR);
   C3D_StencilTest(true,GPU_EQUAL,0,0xFF,0xFF);C3D_StencilOp(GPU_STENCIL_KEEP,GPU_STENCIL_KEEP,GPU_STENCIL_INCR);
   drawIndexedSlices(gpu,faceCount*3);
   C3D_StencilTest(false,GPU_ALWAYS,0,0xFF,0x00);C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_ALL);
   C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,viewLocation,&aircraftMatrix);
   se=C3D_GetTexEnv(0); /* fetched again: citro3d only re-sends a stage marked dirty since the last draw */
   C3D_TexEnvInit(se);C3D_TexEnvSrc(se,C3D_Both,GPU_TEXTURE0,GPU_PRIMARY_COLOR,0);C3D_TexEnvFunc(se,C3D_Both,GPU_MODULATE);
  }}
  weatherLight(s,1);C3D_TexBind(0,&textures[aircraft[s->aircraft].mesh]);drawIndexedSlices(gpu,faceCount*3);
}
static int debugProbe; /* v31.9 hole probes, see setRendererProbe */
static void appendWorldRange(int pending,const ImageryVertex *vertices,int count,int tile){
 if(!count)return;
 /* v31.11 probe: 16 vertices = 576 bytes, so every range starts 64-byte
    aligned (RANGE_VERTICES is a multiple of 16). The gap is never drawn. */
 if(debugProbe&PROBE_ALIGN_RANGES)worldCount[pending]=(worldCount[pending]+15)&~15;
 if(worldCount[pending]+count>RENDER_WORLD_CAPACITY||worldRangeCount[pending]+(count+RANGE_VERTICES-1)/RANGE_VERTICES>WORLD_DRAW_RANGES){photoError=1;return;}
 int first=worldCount[pending];GPUVertex *gpu=worldVbo[pending]+first;Vec anchor=worldAnchor[pending];
 for(int i=0;i<count;i++){
  /* v31.9 probe: (a,b,c) -> (b,c,a); same triangle, other setup order. */
  int from=debugProbe&PROBE_ROTATE_TRIANGLES?i-i%3+(i%3+1)%3:i;
  ImageryVertex p=vertices[from<count?from:i];gpu[i]=(GPUVertex){{p.p.x-anchor.x,p.p.y-anchor.y,p.p.z-anchor.z},{p.uv[0],p.uv[1]},{p.color[0],p.color[1],p.color[2],p.color[3]}};
 }
 GSPGPU_FlushDataCache(gpu,count*sizeof(GPUVertex));worldCount[pending]+=count;
 /* Bound small contiguous chunks, rather than one sphere around the whole
    360-degree terrain ring. Bounds are computed once during scene upload. */
 for(int start=0;start<count;start+=RANGE_VERTICES){
  int length=count-start;if(length>RANGE_VERTICES)length=RANGE_VERTICES;
  Vec lo=vertices[start].p,hi=lo;
  for(int i=start+1;i<start+length;i++){
   Vec p=vertices[i].p;
   lo.x=fminf(lo.x,p.x);lo.y=fminf(lo.y,p.y);lo.z=fminf(lo.z,p.z);
   hi.x=fmaxf(hi.x,p.x);hi.y=fmaxf(hi.y,p.y);hi.z=fmaxf(hi.z,p.z);
  }
  Vec center={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f};
  float dx=hi.x-center.x,dy=hi.y-center.y,dz=hi.z-center.z;
  unsigned hash=2166136261u;const unsigned *words=(const unsigned*)(gpu+start);
  for(unsigned w=0;w<length*sizeof(GPUVertex)/4;w++)hash=(hash^words[w])*16777619u;
  rangeHash[pending][worldRangeCount[pending]]=hash;
  worldRanges[pending][worldRangeCount[pending]++]=(WorldRange){first+start,length,tile,center,sqrtf(dx*dx+dy*dy+dz*dz)};
 }
}
/* v31.1 compact draw ranges. Parts were uploaded as consecutive 768-vertex
   chunks of a ring strip up to 130 km wide, so frustum culling almost never
   removed them and ~1,500 triangles per frame beside or behind the camera
   reached the GPU with vertices far outside the PICA200 rasterizer's range.
   On New 3DS hardware those wrap into single-frame streaks across the
   scenery (Azahar's float32 path never shows them). Grouping each part's
   triangles into 8-cell spatial blocks lets cameraSphereVisible() drop whole
   blocks: at KPHX cruise the out-of-range triangles reaching the GPU fall
   from 599 to 17 and triangles drawn from 13,018 to 5,389
   (tests/range_cull_audit.c); the vertex shader clamps the remainder. */
/* v31.3 hardware bisection (render-test modes): draw photo pieces with their
   procedural vertex colour, or rebuild with the pre-v31.2 strip ranges. */
static int debugNoPhotos,debugStripRanges,debugRebuild,debugTwice,debugNoCull;
void setRendererDebug(int noPhotos,int stripRanges,int twice,int noCull){
 debugNoPhotos=noPhotos;debugTwice=twice;debugNoCull=noCull;
 if(stripRanges!=debugStripRanges){debugStripRanges=stripRanges;debugRebuild=1;}
}
void setRendererProbe(int flags){
 if((flags^debugProbe)&(PROBE_ROTATE_TRIANGLES|PROBE_ALIGN_RANGES))debugRebuild=1;
 debugProbe=flags;
}
#define BLOCK_BUCKETS 96
static WorldVertex sortedPart[SCENERY_PART_CAPACITY];static signed char sortedMaterial[SCENERY_PART_CAPACITY];
static int blockStart[BLOCK_BUCKETS+1];
static int sortIntoBlocks(int count,int part){
 if(debugStripRanges){
  for(int i=0;i<count;i++){sortedPart[i]=worldPart[i];sortedMaterial[i]=(signed char)sceneryVertexMaterial(i);}
  blockStart[0]=0;blockStart[1]=count;return 1;
 }
 int tris=count/3;float cell=part<32?32.f*(1<<(part/4)):128.f,size=cell*8;
 static int keys[BLOCK_BUCKETS];static unsigned char bucketOf[SCENERY_PART_CAPACITY/3];int buckets=0,sizes[BLOCK_BUCKETS]={0};
 for(int t=0;t<tris;t++){
  const WorldVertex *v=worldPart+t*3;
  int key=((int)floorf((v[0].p.x+v[1].p.x+v[2].p.x)/(3*size))&0xffff)<<16|((int)floorf((v[0].p.y+v[1].p.y+v[2].p.y)/(3*size))&0xffff);
  int b=0;while(b<buckets&&keys[b]!=key)b++;
  if(b==buckets){if(buckets<BLOCK_BUCKETS)keys[buckets++]=key;else b=BLOCK_BUCKETS-1;}
  bucketOf[t]=(unsigned char)b;sizes[b]++;
 }
 blockStart[0]=0;for(int b=0;b<buckets;b++)blockStart[b+1]=blockStart[b]+sizes[b]*3;
 int fill[BLOCK_BUCKETS];for(int b=0;b<buckets;b++)fill[b]=blockStart[b];
 for(int t=0;t<tris;t++){
  int at=fill[bucketOf[t]];fill[bucketOf[t]]+=3;
  for(int j=0;j<3;j++){sortedPart[at+j]=worldPart[t*3+j];sortedMaterial[at+j]=(signed char)sceneryVertexMaterial(t*3+j);}
 }
 return buckets;
}
static void uploadSceneryPart(int pending){
 u64 mark=svcGetSystemTick(),now;
 int count=buildSceneryPartCached(&streamFlight,worldPart,SCENERY_PART_CAPACITY,streamPart);
 now=svcGetSystemTick();stageTicks[0]+=now-mark;mark=now;
 int blocks=count>0?sortIntoBlocks(count,streamPart):0;
 now=svcGetSystemTick();stageTicks[1]+=now-mark;mark=now;u64 uploadTicks=0;
 int photoCandidate=(corridorMode?worldPhoto[pending]:imageryReady)&&streamPart<32&&count>0;
 if(photoCandidate){
  Vec lo=worldPart[0].p,hi=lo;
  for(int i=1;i<count;i++){
   Vec p=worldPart[i].p;lo.x=fminf(lo.x,p.x);lo.y=fminf(lo.y,p.y);
   hi.x=fmaxf(hi.x,p.x);hi.y=fmaxf(hi.y,p.y);
  }
  /* Far-away parts need no photo-region clipping or repeated triangle scans. */
  Vec origin=getImageryCoarseOrigin();
  photoCandidate=!(hi.x<=origin.x-8192||lo.x>=origin.x+8192||hi.y<=origin.y-8192||lo.y>=origin.y+8192);
 }
 float regionRect[IMAGERY_REGIONS][4];int regionKeeps[IMAGERY_REGIONS]={0};
 if(photoCandidate)for(int region=0;region<IMAGERY_REGIONS;region++)regionKeeps[region]=imageryRegionRect(region,regionRect[region]);
 for(int block=0;block<blocks;block++){
  int b0=blockStart[block],b1=blockStart[block+1];
  if(photoCandidate){
   /* Exterior regions share material -1; photo regions use detailed tiles 0..3 and outer tiles 4..7. */
   /* v31.15: bounds once per triangle, region rectangles once per part;
      clip only against regions a triangle overlaps (was every region). */
   static float box[SCENERY_PART_CAPACITY/3][4];
   for(int i=b0;i<b1;i+=3){const WorldVertex *v=sortedPart+i;float *bb=box[(i-b0)/3];
    bb[0]=fminf(v[0].p.x,fminf(v[1].p.x,v[2].p.x));bb[1]=fmaxf(v[0].p.x,fmaxf(v[1].p.x,v[2].p.x));
    bb[2]=fminf(v[0].p.y,fminf(v[1].p.y,v[2].p.y));bb[3]=fmaxf(v[0].p.y,fmaxf(v[1].p.y,v[2].p.y));}
   for(int material=-1;material<IMAGERY_TILES;material++){
    int written=0,first,last;imageryMaterialRegions(material,&first,&last);
    for(int i=b0;i<b1;i+=3){const float *bb=box[(i-b0)/3];
     for(int region=first;region<last;region++){
      const float *r=regionRect[region];if(!regionKeeps[region]||bb[1]<r[0]||bb[0]>r[1]||bb[3]<r[2]||bb[2]>r[3])continue;
      int n=clipImageryTriangle(sortedPart+i,region,photoPart+written,SCENERY_PART_CAPACITY*2-written,streamFlight.pos);
      if(n<0){photoError=1;break;}written+=n;
     }
    }
    {u64 a=svcGetSystemTick();appendWorldRange(pending,photoPart,written,material);uploadTicks+=svcGetSystemTick()-a;}
   }
  }else{
   for(int i=b0;i<b1;i++){
    WorldVertex p=sortedPart[i];photoPart[i]=(ImageryVertex){p.p,{(p.p.x-worldAnchor[pending].x)*.25f,(p.p.y-worldAnchor[pending].y)*.25f},{p.color[0],p.color[1],p.color[2],1}};
   }
   /* Keep each material contiguous; batching below binds asphalt once per eye. */
   for(int first=b0;first<b1;){
    int tile=sortedMaterial[first],last=first+3;
    while(last<b1&&sortedMaterial[last]==tile)last+=3;
    {u64 a=svcGetSystemTick();appendWorldRange(pending,photoPart+first,last-first,tile);uploadTicks+=svcGetSystemTick()-a;}first=last;
   }
  }
 }
 now=svcGetSystemTick();stageTicks[2]+=now-mark-uploadTicks;stageTicks[3]+=uploadTicks;
 streamPart++;
 if(streamPart==SCENERY_PARTS){
  /* Never replace a complete scene with a partially clipped build. */
  /* v31.7: flush the whole finished buffer once more before the GPU first
     reads it. Per-range flushes already cover every byte; this guards
     against any dirty line left behind on hardware (Azahar has no caches). */
  if(!photoError)GSPGPU_FlushDataCache(worldVbo[pending],worldCount[pending]*sizeof(GPUVertex));
  if(!photoError){activeWorld=pending;worldReady=1;worldBuilds++;lastWorldBuild=osGetTime();if(corridorMode)imageryReady=worldPhoto[pending];}else if(!corridorMode){imageryReady=false;}
  streamPart=-1;if(shedding==2)shedding=0;
 }
}
void drawWorldScenery(const Flight *s,const CameraPose *camera){
 if(!ready)return;
 if(worldFrame!=renderFrame){worldFrame=renderFrame;
 /* v31.15: the scene is rebuilt each time the plane crosses this grid. Every
    256 m kept a full rebuild running all the time at cruise (about half of
    each frame on Azahar). Higher up the finest rings matter less, so the
    grid widens with height above ground, and the scene centres on the
    nearest grid point (the plane stays within half a step of the centre). */
 static float step=256;float agl=s->pos.z-terrain(s->pos.x,s->pos.y);
 /* 100 m hysteresis around 800 m and 2000 m. */
 if(step<1024&&agl>(step==256?900:2100))step*=2;else if(step>256&&agl<(step==1024?1900:700))step/=2;
 float e=roundf(s->pos.x/step)*step,n=roundf(s->pos.y/step)*step;
 StreamWindow desired;int desiredPhoto=0,changed=0;
 if(corridorMode){
  /* Teleports and rare cache-pressure transitions release both snapshots. A
     procedural scene remains available while the new SD window fills. */
  int far=worldReady&&hypotf(s->pos.x-worldAnchor[activeWorld].x,s->pos.y-worldAnchor[activeWorld].y)>4096;
  /* v31.13: the stream is blocked when the next window's tiles do not fit
     beside the ones the scene on screen pins (20 MB, 12 slots: one detailed
     window is 4x2.8 MB fine + 4x0.7 MB coarse). Dropping both scenes to
     procedural ground freed the pins but left over a minute of flat ground
     on hardware while all eight tiles reloaded. Instead, rebuild the scene
     on screen with its coarse tiles only: its fine tiles (11 MB, 4 slots)
     unpin once that build lands, the new window loads beside it, and the
     normal window change brings detail back. */
  int blocked=sceneryStreamBlocked();
  /* A background build for another window pins up to eight more tiles. When
     the stream is blocked that window is already stale (the plane has left
     it), so stop the build and keep the scene on screen. */
  if(blocked&&streamPart>=0&&shedding!=2&&worldReady&&worldPhoto[1-activeWorld]&&
     (sceneryWindowPins(&worldWindows[1-activeWorld])&~(worldPhoto[activeWorld]?sceneryWindowPins(&worldWindows[activeWorld]):0u))){
   streamPart=-1;photoCancels++;blocked=0;
  }
  int canShed=worldReady&&worldPhoto[activeWorld]&&worldWindows[activeWorld].detailed;
  if(far||(blocked&&!canShed&&!shedding)){
   if(worldReady&&worldPhoto[activeWorld]){photoDrops++;if(far)photoFarDrops++;}
   worldReady=0;streamPart=-1;worldPhoto[0]=worldPhoto[1]=0;imageryReady=false;shedding=0;
  }else if(blocked&&canShed&&!shedding){shedding=1;streamPart=-1;}
  unsigned pins=worldReady&&worldPhoto[activeWorld]?sceneryWindowPins(&worldWindows[activeWorld]):0;
  if(streamPart>=0&&worldPhoto[1-activeWorld])pins|=sceneryWindowPins(&worldWindows[1-activeWorld]);
  {u64 t0=svcGetSystemTick();updateSceneryStream(s->pos,s->vel,pins);frameStreamTicks+=svcGetSystemTick()-t0;}
  desiredPhoto=readySceneryWindow(s->pos,&desired);
  changed=desiredPhoto&&(!worldPhoto[activeWorld]||desired.detailed!=worldWindows[activeWorld].detailed||desired.fine.x!=worldWindows[activeWorld].fine.x||desired.fine.y!=worldWindows[activeWorld].fine.y||desired.coarse.x!=worldWindows[activeWorld].coarse.x||desired.coarse.y!=worldWindows[activeWorld].coarse.y);
 }else{
  int requestedRegion=chooseImageryRegion(s->pos,imageryRegion,s->airport,sceneryAirport);sceneryAirport=s->airport;
  if(imageryRegion!=requestedRegion){
   freeGroundImagery();imageryRegion=requestedRegion;setImageryOrigin(imageryRegionOrigin(imageryRegion));
   imageryReady=loadGroundImagery(imageryRegion);worldReady=0;streamPart=-1;
  }
 }
 if(streamPart>=0&&hypotf(s->pos.x-streamFlight.pos.x,s->pos.y-streamFlight.pos.y)>4096)streamPart=-1;
 /* v31.15: a whole scene costs 1.3-1.8 s in one frame during the loading
    hold. Build the stand-in once, then only the scene the hold waits for
    (not each intermediate window or elevation tile). */
 /* v31.15: elevation tiles keep arriving in flight and each one alone
    restarted the whole scene (every ~3 s at the Grand Canyon). Tiles under
    the plane are read at once (terrainStreamUpdate); farther ones may wait. */
 int holdAllows=!loadBoost||!worldReady||!corridorMode||(desiredPhoto&&(desired.detailed||!sceneryWindowCanDetail(s->pos)));
 if(streamPart<0&&holdAllows&&(!worldReady||changed||shedding||e!=worldAnchor[activeWorld].x||n!=worldAnchor[activeWorld].y||(terrainStreamPending()&&osGetTime()-lastWorldBuild>6000)||debugRebuild)){debugRebuild=0;
  /* Adopt streamed elevation only between rebuilds so all 36 parts agree. */
  terrainStreamPublish(s->pos);
  int pending=1-activeWorld;streamFlight=*s;streamFlight.pos.x=e;streamFlight.pos.y=n;
  if(corridorMode){
   worldPhoto[pending]=desiredPhoto||(worldReady&&worldPhoto[activeWorld]);
   if(desiredPhoto)worldWindows[pending]=desired;else if(worldPhoto[pending])worldWindows[pending]=worldWindows[activeWorld];
   /* shedding is still 2 here only if that build was cut short: start over. */
   if(shedding&&worldPhoto[pending]){
    if(!desiredPhoto||desired.detailed)worldWindows[pending]=worldWindows[activeWorld];
    worldWindows[pending].detailed=0;for(int k=0;k<4;k++)worldWindows[pending].slots[k]=-1;
    shedding=2;photoSheds++;
   }else if(shedding)shedding=0;
  }
  /* Keep elevation local too: PICA vertex arithmetic loses centimetre-scale
     pavement offsets when subtracting kilometre-scale MSL elevations. */
  worldAnchor[pending]=(Vec){e,n,floorf(s->pos.z/256)*256};worldCount[pending]=0;worldRangeCount[pending]=0;photoError=0;streamPart=0;
 }
 if(streamPart>=0){
  int pending=1-activeWorld;
  if(corridorMode&&worldPhoto[pending]){setImageryWindows(worldWindows[pending].fine,worldWindows[pending].coarse);setImageryFineEnabled(worldWindows[pending].detailed);}
  int immediate=loadBoost||!worldReady||hypotf(s->pos.x-worldAnchor[activeWorld].x,s->pos.y-worldAnchor[activeWorld].y)>4096;
  u64 t0=svcGetSystemTick();
  if(immediate){while(streamPart>=0){uploadSceneryPart(pending);frameParts++;}}else{uploadSceneryPart(pending);frameParts++;}
  frameBuildTicks+=svcGetSystemTick()-t0;
 }
 }
 if(corridorMode&&worldPhoto[activeWorld]){setImageryWindows(worldWindows[activeWorld].fine,worldWindows[activeWorld].coarse);setImageryFineEnabled(worldWindows[activeWorld].detailed);}
 C2D_Flush();C3D_BindProgram(&program);C3D_SetAttrInfo(&attributes);
 C3D_BufInfo info;BufInfo_Init(&info);BufInfo_Add(&info,worldVbo[activeWorld],sizeof(GPUVertex),3,0x210);C3D_SetBufInfo(&info);
 C3D_Mtx projection;projectionForEye(&projection);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,projectionLocation,&projection);
 /* Rotation-only view; the shader subtracts the anchor-relative eye first. */
 Vec anchor=worldAnchor[activeWorld];
 float rows[4][4];cameraViewRows(camera,camera->eye,rows);C3D_Mtx modelView;
 for(int i=0;i<4;i++)modelView.r[i]=FVec4_New(rows[i][0],rows[i][1],rows[i][2],rows[i][3]);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,viewLocation,&modelView);
 setEyeOffset((Vec){camera->eye.x-anchor.x,camera->eye.y-anchor.y,camera->eye.z-anchor.z});
 if(debugProbe&PROBE_NO_GUARD)C3D_FVUnifSet(GPU_VERTEX_SHADER,guardLocation,4,1e9f,0,0);
 C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(true,debugProbe&PROBE_DEPTH_ALWAYS?GPU_ALWAYS:GPU_GREATER,GPU_WRITE_ALL);C3D_AlphaTest(false,GPU_ALWAYS,0);C3D_LightEnvBind(NULL);
 for(int i=0;i<6;i++)C3D_TexEnvInit(C3D_GetTexEnv(i));
 C3D_TexEnv *env=C3D_GetTexEnv(0);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
 /* All world surfaces are opaque and depth tested. Group by material so
    texture/environment setup happens at most nine times per eye. */
 weatherLight(s,4);
 unsigned char visible[WORLD_DRAW_RANGES];
 for(int i=0;i<worldRangeCount[activeWorld];i++){
  WorldRange *r=&worldRanges[activeWorld][i];
  visible[i]=debugNoCull||cameraSphereVisible(camera,r->center,r->radius,stereoIod,stereoScreen);
 }
 lastVisibleRanges=0;for(int i=0;i<worldRangeCount[activeWorld];i++)lastVisibleRanges+=visible[i];
 /* v31.7 bisection: DRAW TWICE repeats every world draw. A triangle the GPU
    drops at random comes back; one built from bad vertex data does not. */
 for(int pass=0;pass<(debugTwice?2:1);pass++)
 {for(int tile=-2;tile<IMAGERY_TILES;tile++){
  int any=0;for(int i=0;i<worldRangeCount[activeWorld];i++)if(visible[i]&&worldRanges[activeWorld][i].tile==tile){any=1;break;}
  if(!any)continue;
  C3D_TexEnvInit(C3D_GetTexEnv(0));C3D_TexEnvInit(C3D_GetTexEnv(1));C3D_TexEnvInit(C3D_GetTexEnv(2));C3D_TexEnvInit(C3D_GetTexEnv(3));env=C3D_GetTexEnv(0);
  if(tile>=0&&!debugNoPhotos){
   C3D_TexEnvSrc(env,C3D_RGB,GPU_TEXTURE0,0,0);C3D_TexEnvFunc(env,C3D_RGB,GPU_REPLACE);
   /* Vertex alpha controls the texture/procedural mix, not framebuffer opacity. */
   C3D_TexEnvSrc(env,C3D_Alpha,GPU_CONSTANT,0,0);C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);C3D_TexEnvColor(env,0xFFFFFFFF);
   C3D_TexBind(0,corridorMode?sceneryStreamTexture(worldWindows[activeWorld].slots[tile]):&groundTextures[tile]);
   env=C3D_GetTexEnv(1);C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,tile<4?GPU_TEXTURE1:GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR);
   C3D_TexEnvOpRgb(env,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_ALPHA);
   C3D_TexEnvFunc(env,C3D_RGB,GPU_INTERPOLATE);
   if(tile<4){
    C3D_TexBind(1,corridorMode?sceneryStreamTexture(worldWindows[activeWorld].slots[imageryCoarseTile(tile)]):&groundTextures[imageryCoarseTile(tile)]);
    float uv[4];imageryCoarseTransform(tile,uv);
    C3D_FVUnifSet(GPU_VERTEX_SHADER,coarseUvLocation,uv[0],uv[1],uv[2],uv[3]);
    env=C3D_GetTexEnv(2);C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_PRIMARY_COLOR,GPU_CONSTANT);
    C3D_TexEnvOpRgb(env,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_ALPHA);C3D_TexEnvFunc(env,C3D_RGB,GPU_INTERPOLATE);
    int opacity=(int)(255*(1-sceneryHaze(hypotf(s->pos.x-getImageryOrigin().x,s->pos.y-getImageryOrigin().y))));
    C3D_TexEnvColor(env,C2D_Color32(255,255,255,opacity));
   }
   /* Aerial photos contain their own captured lighting. Retain 65% of that
      detail while exposing the baked slope lighting/rock colors, consistently
      across fine/coarse tiles and their procedural boundary. No new texture,
      mesh, or draw pass is needed; alpha remains opaque. */
   env=C3D_GetTexEnv(3);
   C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_PRIMARY_COLOR,GPU_CONSTANT);
   C3D_TexEnvOpRgb(env,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_ALPHA);
   C3D_TexEnvFunc(env,C3D_RGB,GPU_INTERPOLATE);C3D_TexEnvColor(env,C2D_Color32(255,255,255,166));
  }else if(tile==-2){
   C3D_TexBind(0,&airportTexture);
   C3D_TexEnvSrc(env,C3D_Both,GPU_TEXTURE0,GPU_PRIMARY_COLOR,0);C3D_TexEnvFunc(env,C3D_Both,GPU_MODULATE);
  }else{
   C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
  }
  for(int i=0;i<worldRangeCount[activeWorld];i++){
   WorldRange range=worldRanges[activeWorld][i];
   /* v31.4: draw every range from vertex 0 of its own buffer slice. The world
      buffer holds up to 90,000 vertices, and array draws use the PICA200's
      16-bit index path ("unsigned short or drawing arrays", 3dbrew
      GPUREG_INDEXBUFFER_CONFIG): a range starting past 65,535 fetched
      vertices from elsewhere on hardware, leaving holes (sky showing through
      terrain breaks) and stray triangles that changed with each rebuild.
      Azahar indexes with 32 bits and never showed them.
      v31.12: on New 3DS XL hardware, array draws still lost whole triangles
      (no fragments at all, with memory, depth, culling, blending, the shader
      guard and vertex order all ruled out). Drawn indexed through a shared
      0..767 u16 index list, the same ranges had no holes in any of 26 test
      frames over Phoenix, Sedona, Grand Canyon and Tucson, against holes in
      every array-drawn frame. Same frame rate. */
   if(visible[i]&&range.tile==tile){
    if(debugProbe&PROBE_SMALL_DRAWS){
     for(int piece=0;piece<range.count;piece+=24){
      BufInfo_Init(&info);BufInfo_Add(&info,worldVbo[activeWorld]+range.first+piece,sizeof(GPUVertex),3,0x210);C3D_SetBufInfo(&info);
      C3D_DrawArrays(GPU_TRIANGLES,0,range.count-piece<24?range.count-piece:24);
     }
     continue;
    }
    BufInfo_Init(&info);BufInfo_Add(&info,worldVbo[activeWorld]+range.first,sizeof(GPUVertex),3,0x210);C3D_SetBufInfo(&info);
    if(debugProbe&PROBE_DRAW_ARRAYS)C3D_DrawArrays(GPU_TRIANGLES,0,range.count);
    else C3D_DrawElements(GPU_TRIANGLES,range.count,C3D_UNSIGNED_SHORT,rangeIndices);
   }
  }
 }}
}
int rendererSceneryLoading(Vec pos){
 if(!ready||!corridorMode)return 0;
 /* v32: nothing to wait for where the pack has no photos (outside the
    states, or tiles not built yet). */
 if(!sceneryWindowCatalogued(pos))return 0;
 if(!worldReady||!worldPhoto[activeWorld])return 1;
 if(hypotf(pos.x-worldAnchor[activeWorld].x,pos.y-worldAnchor[activeWorld].y)>4096)return 1;
 return !worldWindows[activeWorld].detailed&&sceneryWindowCanDetail(pos);
}
void setSceneryLoading(int on){loadBoost=on;sceneryStreamBoost(on&&corridorMode);}
void rendererBuildStages(float ms[4]){for(int i=0;i<4;i++){ms[i]=stageTicks[i]/(SYSCLOCK_ARM11/1000.f);stageTicks[i]=0;}}
void rendererFrameTimes(float *streamMs,float *buildMs,int *parts){
 const float perMs=SYSCLOCK_ARM11/1000.f;
 *streamMs=frameStreamTicks/perMs;*buildMs=frameBuildTicks/perMs;*parts=frameParts;
 frameStreamTicks=frameBuildTicks=0;frameParts=0;
}
RendererWorldStats rendererVerifyWorld(void){
 RendererWorldStats st={worldBuilds,activeWorld,worldCount[activeWorld],worldRangeCount[activeWorld],lastVisibleRanges,0,-1,
  imageryReady,corridorMode?worldReady&&worldPhoto[activeWorld]&&worldWindows[activeWorld].detailed:imageryReady,photoDrops,photoSheds,photoCancels,photoFarDrops,{0,0,0},{0,0,0,0},(int)(linearSpaceFree()/1024)};
 if(corridorMode){sceneryStreamBlockCounts(st.block);sceneryStreamPoolInfo(st.pool);}
 if(!worldReady)return st;
 /* The active buffer is no longer written: drop CPU cache lines and read
    what memory (and so the GPU) actually holds. */
 GSPGPU_InvalidateDataCache(worldVbo[activeWorld],worldCount[activeWorld]*sizeof(GPUVertex));
 for(int r=0;r<worldRangeCount[activeWorld];r++){
  WorldRange range=worldRanges[activeWorld][r];unsigned hash=2166136261u;const unsigned *words=(const unsigned*)(worldVbo[activeWorld]+range.first);
  for(unsigned w=0;w<range.count*sizeof(GPUVertex)/4;w++)hash=(hash^words[w])*16777619u;
  if(hash!=rangeHash[activeWorld][r]){st.badRanges++;if(st.firstBad<0)st.firstBad=r;}
 }
 return st;
}
void drawWeatherClouds(const Flight *s,const CameraPose *camera){
 if(!ready||!s->weather)return;
 if(cloudFrame!=renderFrame){
  cloudFrame=renderFrame;cloudAnchor=s->pos;cloudCount=0;
  WeatherPuff puffs[WEATHER_PUFF_CAPACITY];float depth[WEATHER_PUFF_CAPACITY];
  int n=weatherPuffs(s,puffs,WEATHER_PUFF_CAPACITY);
  for(int i=0;i<n;i++)depth[i]=projectCamera(camera,puffs[i].center).z;
  for(int i=1;i<n;i++)for(int j=i;j>0&&depth[j]>depth[j-1];j--){float d=depth[j];depth[j]=depth[j-1];depth[j-1]=d;WeatherPuff p=puffs[j];puffs[j]=puffs[j-1];puffs[j-1]=p;}
  preparedCloud=cloudVbo[cloudBuffer];cloudBuffer=1-cloudBuffer;
  for(int i=0;i<n;i++){
   WeatherPuff p=puffs[i];if(depth[i]<2||!cameraSphereVisible(camera,p.center,p.width,stereoIod,stereoScreen))continue;
   float shade=clampf(p.shade+weatherFlash(s)*.35f,0,1);
   Vec center={p.center.x-cloudAnchor.x,p.center.y-cloudAnchor.y,p.center.z-cloudAnchor.z};
   static const float ringX[8]={1,.70710678f,0,-.70710678f,-1,-.70710678f,0,.70710678f};
   static const float ringY[8]={0,.70710678f,1,.70710678f,0,-.70710678f,-1,-.70710678f};
   for(int edge=0;edge<8;edge++){
    GPUVertex *v=preparedCloud+cloudCount;
    v[0]=(GPUVertex){{center.x,center.y,center.z},{0,0},{shade,shade,shade,p.opacity}};
    for(int j=0;j<2;j++){
     int corner=(edge+j)&7;float u=ringX[corner]*p.width*.5f,w=ringY[corner]*p.height*.5f;
     Vec q={center.x+camera->right.x*u+camera->up.x*w,center.y+camera->right.y*u+camera->up.y*w,center.z+camera->right.z*u+camera->up.z*w};
     v[j+1]=(GPUVertex){{q.x,q.y,q.z},{0,0},{shade,shade,shade,0}};
    }
    cloudCount+=3;
   }
  }
  GSPGPU_FlushDataCache(preparedCloud,cloudCount*sizeof(GPUVertex));
 }
 if(!cloudCount)return;
 C2D_Flush();C3D_BindProgram(&program);C3D_SetAttrInfo(&attributes);
 C3D_BufInfo info;BufInfo_Init(&info);BufInfo_Add(&info,preparedCloud,sizeof(GPUVertex),3,0x210);C3D_SetBufInfo(&info);
 C3D_Mtx projection;projectionForEye(&projection);C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,projectionLocation,&projection);
 float rows[4][4];cameraViewRows(camera,cloudAnchor,rows);C3D_Mtx matrix;
 for(int i=0;i<4;i++)matrix.r[i]=FVec4_New(rows[i][0],rows[i][1],rows[i][2],rows[i][3]);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,viewLocation,&matrix);setEyeOffset((Vec){0,0,0});
 C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);
 for(int i=0;i<6;i++)C3D_TexEnvInit(C3D_GetTexEnv(i));
 C3D_TexEnv *env=C3D_GetTexEnv(0);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
 /* Cloud shade already carries the storm darkening; apply only time of day. */
 {float rgb[3];daylightTint(rgb);if(rgb[0]<.999f||rgb[2]<.999f){env=C3D_GetTexEnv(1);C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,0);C3D_TexEnvFunc(env,C3D_RGB,GPU_MODULATE);
  C3D_TexEnvColor(env,C2D_Color32((int)(rgb[0]*255),(int)(rgb[1]*255),(int)(rgb[2]*255),255));}}
 drawIndexedSlices(preparedCloud,cloudCount);
}
/* ------------------------------------------------------------------ v34 night lights
   Drawn by light_render.c (geometry-shader lamps). Here: the aircraft's
   navigation-light positions from the mesh actually drawn. */
static Vec navTips[3];static int tipType=-1;static float tipFlap=-1;
static void meshTips(void){
 /* Body axes: x forward, y right, z down. */
 float lo=1e9f,hi=-1e9f,back=1e9f;Vec a={0,0,0},b={0,0,0},c={0,0,0};
 for(int i=0;i<faceCount;i++)for(int j=0;j<3;j++){Vec p=faces[i].v[j].p;
  if(p.y<lo){lo=p.y;a=p;}if(p.y>hi){hi=p.y;b=p;}if(p.x<back){back=p.x;c=p;}}
 navTips[0]=(Vec){a.x,a.y-.15f,a.z};navTips[1]=(Vec){b.x,b.y+.15f,b.z};navTips[2]=(Vec){c.x-.2f,c.y,c.z};
}
void drawNightLights(const Flight *s,const CameraPose *camera,int exterior){
 if(!ready)return;
 int valid=exterior&&cachedType==s->aircraft&&faceCount>0;
 if(valid&&(tipType!=s->aircraft||tipFlap!=cachedFlap)){meshTips();tipType=s->aircraft;tipFlap=cachedFlap;}
 C2D_Flush();
 C3D_Mtx projection;projectionForEye(&projection);
 drawLightPass(s,camera,exterior,&projection,renderFrame,navTips,valid);
}
void nightLightFrameStats(float *ms,float *vertices,float *cityConsidered){
 float dyn;lightPassStats(ms,&dyn,cityConsidered);*vertices=dyn; /* now lamps, one vertex each */
}
void freeAircraftRenderer(void){
 freeSceneryStream();freeTerrainStream();freeSceneryPartCache();freeGroundImagery();freeLightRenderer();
 if(airportTexture.data){C3D_TexDelete(&airportTexture);memset(&airportTexture,0,sizeof(airportTexture));}
 if(cockpitTexture.data){C3D_TexDelete(&cockpitTexture);memset(&cockpitTexture,0,sizeof(cockpitTexture));}
 for(int i=0;i<AIRCRAFT_TEXTURES;i++)if(textures[i].data){C3D_TexDelete(&textures[i]);memset(&textures[i],0,sizeof(textures[i]));}
 for(int i=0;i<2;i++)if(vbo[i]){linearFree(vbo[i]);vbo[i]=NULL;}
 for(int i=0;i<2;i++)if(cockpitVbo[i]){linearFree(cockpitVbo[i]);cockpitVbo[i]=NULL;}
 for(int i=0;i<2;i++)if(worldVbo[i]){linearFree(worldVbo[i]);worldVbo[i]=NULL;}
 for(int i=0;i<2;i++)if(cloudVbo[i]){linearFree(cloudVbo[i]);cloudVbo[i]=NULL;}
 if(rangeIndices){linearFree(rangeIndices);rangeIndices=NULL;}
 if(shader){shaderProgramFree(&program);DVLB_Free(shader);shader=NULL;}ready=false;
}


