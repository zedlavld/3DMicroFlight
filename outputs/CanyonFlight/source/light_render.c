#include <3ds.h>
#include <citro3d.h>
#include <math.h>
#include <string.h>
#include "flight.h"
#include "light_render.h"
#include "airport_lights.h"
#include "city_lights.h"
#include "daylight.h"
#include "light_shader_shbin.h"
#define DYNAMIC_CAPACITY 6000
#define SLICE 768
static DVLB_s *shader;static shaderProgram_s program;static C3D_AttrInfo attributes;
static int locModelView,locEyeOffset,locColorScale,locProjection,locSize;
static LightPoint *dynamicVbo[2];static int dynamicBuffer,dynamicCount;static LightPoint *prepared;
/* Steady airport lamps: rebuilt every STEADY_FRAMES into the buffer not being drawn. */
#define STEADY_FRAMES 20
static LightPoint *steadyVbo[2];static int steadyBuffer,steadyCount;static Vec steadyAnchor;static unsigned steadyFrame=~0u;
static LightPoint *emitTarget;static int *emitCount;static Vec emitAnchor;
static u16 *indices;static Vec anchor;static unsigned builtFrame=~0u;
static double statMs,statDynamic,statCity;static int statFrames;
bool initLightRenderer(void){
 shader=DVLB_ParseFile((u32*)light_shader_shbin,light_shader_shbin_size);if(!shader)return false;
 shaderProgramInit(&program);shaderProgramSetVsh(&program,&shader->DVLE[0]);shaderProgramSetGsh(&program,&shader->DVLE[1],2);
 locModelView=shaderInstanceGetUniformLocation(program.vertexShader,"modelView");
 locEyeOffset=shaderInstanceGetUniformLocation(program.vertexShader,"eyeOffset");
 locColorScale=shaderInstanceGetUniformLocation(program.vertexShader,"colorScale");
 locProjection=shaderInstanceGetUniformLocation(program.geometryShader,"projection");
 locSize=shaderInstanceGetUniformLocation(program.geometryShader,"sizeParam");
 AttrInfo_Init(&attributes);AttrInfo_AddLoader(&attributes,0,GPU_FLOAT,3);AttrInfo_AddLoader(&attributes,1,GPU_UNSIGNED_BYTE,4);
 for(int i=0;i<2;i++)if(!(dynamicVbo[i]=linearAlloc(DYNAMIC_CAPACITY*sizeof(LightPoint)))||!(steadyVbo[i]=linearAlloc(DYNAMIC_CAPACITY*sizeof(LightPoint)))){freeLightRenderer();return false;}
 indices=linearAlloc(SLICE*sizeof(u16));if(!indices){freeLightRenderer();return false;}
 for(int i=0;i<SLICE;i++)indices[i]=i;
 GSPGPU_FlushDataCache(indices,SLICE*sizeof(u16));
 initCityLights(); /* optional: no town lights without the table */
 return true;
}
void freeLightRenderer(void){
 freeCityLights();
 for(int i=0;i<2;i++){if(dynamicVbo[i]){linearFree(dynamicVbo[i]);dynamicVbo[i]=NULL;}if(steadyVbo[i]){linearFree(steadyVbo[i]);steadyVbo[i]=NULL;}}
 if(indices){linearFree(indices);indices=NULL;}
 if(shader){shaderProgramFree(&program);DVLB_Free(shader);shader=NULL;}
}
static void emitDynamic(Vec p,float r,float g,float b,float size,float minPixels){
 (void)size;if(*emitCount>=DYNAMIC_CAPACITY)return;
 float px=minPixels*40;if(px>255)px=255;
 emitTarget[(*emitCount)++]=(LightPoint){p.x-emitAnchor.x,p.y-emitAnchor.y,p.z-emitAnchor.z,
  (unsigned char)(fminf(r,1)*255),(unsigned char)(fminf(g,1)*255),(unsigned char)(fminf(b,1)*255),(unsigned char)px};
}
static Vec viewEye;
/* v35.22: the pass binds its program (vertex + geometry shader) only when it has points to draw.
   Binding it and drawing nothing left the GPU in a state that, on hardware only, misplaced vertices
   in the next passes: the garbled cockpits since v35.02 (AT-position cruise and the runway cockpit
   draw no lights; airport cruise cases, which had lamps in range, were clean; Azahar never showed it). */
static const CameraPose *passCamera;static const C3D_Mtx *passProjection;static int passBound;static float passScale;
static void bindLightPass(void){
 passBound=1;
 C3D_BindProgram(&program);C3D_SetAttrInfo(&attributes);
 float rows[4][4];cameraViewRows(passCamera,passCamera->eye,rows);C3D_Mtx view;
 for(int i=0;i<4;i++)view.r[i]=FVec4_New(rows[i][0],rows[i][1],rows[i][2],rows[i][3]);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,locModelView,&view);
 C3D_FVUnifMtx4x4(GPU_GEOMETRY_SHADER,locProjection,passProjection);
 C3D_FVUnifSet(GPU_GEOMETRY_SHADER,locSize,.25f,1/440.f,1.1f,1.0f);
 C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);
 for(int i=0;i<6;i++)C3D_TexEnvInit(C3D_GetTexEnv(i));
 C3D_TexEnv *env=C3D_GetTexEnv(0);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
}
static void drawPoints(const LightPoint *base,Vec origin,int count){
 if(count<=0)return;
 if(!passBound)bindLightPass();
 C3D_FVUnifSet(GPU_VERTEX_SHADER,locColorScale,passScale,passScale,passScale,1);
 C3D_FVUnifSet(GPU_VERTEX_SHADER,locEyeOffset,viewEye.x-origin.x,viewEye.y-origin.y,viewEye.z-origin.z,0);
 for(int first=0;first<count;first+=SLICE){
  int length=count-first<SLICE?count-first:SLICE;
  C3D_BufInfo info;BufInfo_Init(&info);BufInfo_Add(&info,base+first,sizeof(LightPoint),2,0x10);C3D_SetBufInfo(&info);
  C3D_DrawElements(GPU_GEOMETRY_PRIM,length,C3D_UNSIGNED_SHORT,indices);
 }
}
static Vec vadd(Vec a,Vec b){return (Vec){a.x+b.x,a.y+b.y,a.z+b.z};}
void drawLightPass(const Flight *s,const CameraPose *camera,int exterior,const C3D_Mtx *projection,
                   unsigned frame,const Vec tips[3],int tipsValid){
 if(!shader)return;
 float night=daylightNight();
 /* v34.03: no early return in daylight; glideslope lights show by day. */
 u64 start=svcGetSystemTick();
 if(builtFrame!=frame){
  if(steadyFrame==~0u||frame-steadyFrame>=STEADY_FRAMES){
   steadyFrame=frame;int next=steadyBuffer^1,count=0;
   emitTarget=steadyVbo[next];emitCount=&count;emitAnchor=camera->eye;
   airportLights(camera->eye,s->weatherTime,night,emitDynamic,0);
   if(count)GSPGPU_FlushDataCache(steadyVbo[next],count*sizeof(LightPoint));
   steadyBuffer=next;steadyCount=count;steadyAnchor=camera->eye;
  }
  builtFrame=frame;anchor=camera->eye;dynamicCount=0;prepared=dynamicVbo[dynamicBuffer];dynamicBuffer^=1;
  emitTarget=prepared;emitCount=&dynamicCount;emitAnchor=anchor;
  airportLights(camera->eye,s->weatherTime,night,emitDynamic,1);
  trafficLights(camera->eye,camera->forward,s->weatherTime,night,emitDynamic);
  if(exterior&&tipsValid){
   float t=s->weatherTime,lamp=.25f+.75f*night;
   Vec l=vadd(s->pos,bodyWorld(s,tips[0])),r=vadd(s->pos,bodyWorld(s,tips[1])),tail=vadd(s->pos,bodyWorld(s,tips[2]));
   emitDynamic(l,lamp,.05f,.03f,.35f,1.6f);emitDynamic(r,.05f,lamp,.25f,.35f,1.6f);emitDynamic(tail,lamp,lamp,lamp,.3f,1.4f);
   if(fmodf(t,1.2f)<.06f){emitDynamic(l,1,1,1,.4f,2.5f);emitDynamic(r,1,1,1,.4f,2.5f);}               /* strobes */
   if(fmodf(t+.6f,1.0f)<.15f)emitDynamic(vadd(s->pos,bodyWorld(s,(Vec){0,0,-1.2f})),1,.1f,.05f,.45f,2.0f); /* beacon */
  }
  if(dynamicCount)GSPGPU_FlushDataCache(prepared,dynamicCount*sizeof(LightPoint));
 }
 passCamera=camera;passProjection=projection;passBound=0;viewEye=camera->eye;passScale=1/255.f;
 if(steadyCount)drawPoints(steadyVbo[steadyBuffer],steadyAnchor,steadyCount);
 if(dynamicCount)drawPoints(prepared,anchor,dynamicCount);
 int city=0;
 if(daylightLampLevel(night)>0){
  passScale=daylightLampLevel(night)/255.f;
  cityLights(camera,frame,drawPoints);
  int cells,pending;cityLightStats(&cells,&city,&pending);
 }
 statMs+=(svcGetSystemTick()-start)/(SYSCLOCK_ARM11/1000.0);statDynamic+=dynamicCount+steadyCount;statCity+=city;statFrames++;
}
void lightPassStats(float *ms,float *dynamicLamps,float *cityLamps){
 int n=statFrames?statFrames:1;*ms=statMs/n;*dynamicLamps=statDynamic/n;*cityLamps=statCity/n;
 statMs=statDynamic=statCity=0;statFrames=0;
}
