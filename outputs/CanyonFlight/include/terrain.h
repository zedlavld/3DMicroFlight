#ifndef TERRAIN_H
#define TERRAIN_H
#include "flight.h"
float rawElevation(float east,float north);
float runwayElevation(const Airport *airport,float along);
/* 1 inside the flattened runway/apron core, fading to 0 over 100 m. */
float airportFlattening(float e,float n);
/* terrain() plus airportFlattening() in a single pass. */
float terrainWithFlattening(float e,float n,float *flattening);
int terrainContains(float east,float north);
/* v34.04: restrict terrain()/terrainWithFlattening() to runways near a box
   (scenery part builds); Off restores all. Same thread only. */
void terrainLimitAirports(float e0,float n0,float e1,float n1);
void terrainLimitAirportsOff(void);
#endif
