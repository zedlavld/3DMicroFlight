#ifndef COCKPIT_H
#define COCKPIT_H
#include <stdint.h>
#include "flight.h"
#define COCKPIT_CAPACITY 1280 /* v34.03b: 737/A321/F-16 cockpits peak near 1000 */
typedef struct {Vec v[3];uint32_t color;} CockpitFace;
/* Pilot-eye-relative body coordinates, x forward/y right/z down, metres.
   Original simplified geometry; no imported aircraft interior assets. */
int buildCockpitStatic(int type,CockpitFace *faces,int capacity);
int buildCockpitDynamic(const Flight *s,CockpitFace *faces,int capacity);
int buildCockpit(const Flight *s,CockpitFace *faces,int capacity);
/* v34.03: default downward tilt of the cockpit view, radians. */
float cockpitViewTilt(int type);
#endif
