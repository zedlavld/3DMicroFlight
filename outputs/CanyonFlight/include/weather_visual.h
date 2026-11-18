#ifndef WEATHER_VISUAL_H
#define WEATHER_VISUAL_H
#include "flight.h"
#define WEATHER_PUFF_CAPACITY 27
typedef struct {Vec center;float width,height,opacity,shade;} WeatherPuff;
int weatherPuffs(const Flight *s,WeatherPuff *out,int capacity);
float weatherFlash(const Flight *s);
float weatherThunder(const Flight *s);
float weatherRain(const Flight *s);
/* 0 .. 1: how stormy it is here (1 = the storm preset). */
float weatherStorm(const Flight *s);
#endif
