#ifndef LIGHT_RENDER_H
#define LIGHT_RENDER_H
#include <stdbool.h>
#include <citro3d.h>
#include "camera.h"
/* v34.02 GPU night lights: one 16-byte vertex per lamp, expanded to a
   camera-facing square by a geometry shader (source/light_shader.*.pica).
   Airport and aircraft lights are rebuilt every frame; town/city lights
   live in static per-cell buffers (city_lights.c) filled as cells come
   into range. */
typedef struct {float x,y,z;unsigned char r,g,b,size;} LightPoint;
bool initLightRenderer(void);
void freeLightRenderer(void);
/* tips[0..2]: left wingtip, right wingtip, tail in body axes; valid = 0
   when the aircraft mesh is not built. */
void drawLightPass(const Flight *s,const CameraPose *camera,int exterior,const C3D_Mtx *projection,
                   unsigned frame,const Vec tips[3],int tipsValid);
/* Since the last call: average ms building lights, dynamic lamps and city
   lamps drawn per frame. */
void lightPassStats(float *ms,float *dynamicLamps,float *cityLamps);
#endif
