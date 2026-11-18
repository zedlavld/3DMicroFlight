#ifndef AIRCRAFT_RENDER_H
#define AIRCRAFT_RENDER_H
#include <stdbool.h>
#include "camera.h"
typedef struct {int fixedTriangles,liveTriangles,uploadBytes;} CockpitRenderStats;
CockpitRenderStats cockpitRenderStats(void);
bool initAircraftRenderer(void);
bool hasGroundImagery(void);
void beginRendererFrame(void);
void setRendererEye(float iod,float screen);
/* World near plane: 6 m in exterior view, 2 m in the cockpit (v31.1). */
void setRendererExterior(int exterior);
/* v31.3 render-test bisection: photo textures off; pre-v31.2 strip ranges. */
void setRendererDebug(int noPhotos,int stripRanges,int twice,int noCull);
/* v31.9 hole probes for the world pass: depth test ALWAYS (a dropped
   triangle stays missing, a depth-rejected one appears), shader guard clamp
   off, and each triangle uploaded as (b,c,a) instead of (a,b,c). */
/* v31.11 draw-path probes: ranges start on 64-byte boundaries, ranges drawn
   as 8-triangle pieces. Since v31.12 world ranges are drawn indexed;
   PROBE_DRAW_ARRAYS brings back the old array draws for comparison. */
enum {PROBE_DEPTH_ALWAYS=1,PROBE_NO_GUARD=2,PROBE_ROTATE_TRIANGLES=4,PROBE_ALIGN_RANGES=8,PROBE_SMALL_DRAWS=16,PROBE_DRAW_ARRAYS=32};
void setRendererProbe(int flags);
/* v31.7 diagnostics: scene builds, active buffer, vertices, ranges, ranges
   drawn last frame, and ranges whose memory no longer matches what the CPU
   wrote (re-read after invalidating the CPU cache). */
/* photo: ground photos on screen; detailed: fine tiles in use; drops: scenes
   thrown back to procedural ground; sheds: v31.13 steps down to coarse tiles;
   cancels: background builds stopped to free their tile pins; farDrops: drops
   from a jump of more than 4 km; block[]: see sceneryStreamBlockCounts. */
typedef struct {int builds,active,vertices,ranges,visible,badRanges,firstBad,photo,detailed,drops,sheds,cancels,farDrops,block[3],pool[4],linearFreeKB;} RendererWorldStats;
RendererWorldStats rendererVerifyWorld(void);
/* v31.15: 1 while the scene at pos is not photo scenery yet (or not detailed
   where fine tiles exist). setSceneryLoading(1) builds scenes in one frame
   and uploads whole tiles; the caller holds the flight meanwhile. */
int rendererSceneryLoading(Vec pos);
void setSceneryLoading(int on);
/* Time spent since the last call: stream update, scene building (ms), and
   scene parts built. */
void rendererFrameTimes(float *streamMs,float *buildMs,int *parts);
/* Scene build stages since the last call (ms): [0] mesh, [1] block sort,
   [2] photo clipping, [3] vertex upload. */
void rendererBuildStages(float ms[4]);
/* These passes leave 3D state active. Before drawing 2D overlays, the caller
   must call C2D_Prepare and set its desired depth/write mask. */
void drawVirtualCockpit(const Flight *s,const CameraPose *camera);
void drawTexturedAircraft(const Flight *s,const CameraPose *camera);
void drawWorldScenery(const Flight *s,const CameraPose *camera);
void drawWeatherClouds(const Flight *s,const CameraPose *camera);
/* v34: runway edge lights at night; navigation lights, beacon and strobes
   on the aircraft in exterior view. */
void drawNightLights(const Flight *s,const CameraPose *camera,int exterior);
/* Since the last call: average light-building ms, light vertices and city
   lights considered per frame. */
void nightLightFrameStats(float *ms,float *vertices,float *cityConsidered);
void freeAircraftRenderer(void);
#endif
