#ifndef CITY_LIGHTS_H
#define CITY_LIGHTS_H
#include "flight.h"
#include "camera.h"
#include "light_render.h"
#include "airport_lights.h"
/* v34.02: town, city and street lights from data/night_lights.bin
   (tools/build_night_lights.py: Census TIGER places and roads, filtered by
   the photo tiles' built-up score). 4 km cells; level 0 lights draw to
   42 km, level 1 to 10 km, level 2 to 3.5 km. Each cell in range gets a
   static GPU buffer of LightPoints (cell-relative positions); ground heights
   come from rawElevation, 600 lamps per frame, cells within 10 km first.
   Cells unused for a while are freed.
   v1.1p14: the rest of the lower 48 from scenery/night_lights_us.bin on the card
   (tools/build_us_night_lights.py, OSM streets), cell points read by a worker. */
int initCityLights(void);
void freeCityLights(void);
/* Calls draw(points, cell origin, count) for each visible cell. */
typedef void (*CityDraw)(const LightPoint *points,Vec origin,int count);
void cityLights(const CameraPose *camera,unsigned frame,CityDraw draw);
void cityLightStats(int *cells,int *drawn,int *pendingHeights);
/* v34.02: moving car lights on primary/secondary roads within 12 km
   (emitted every frame with the animated airport lamps). */
void trafficLights(Vec eye,Vec forward,float time,float night,LightEmit emit);
#endif
