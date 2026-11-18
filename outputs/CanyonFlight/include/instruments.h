#ifndef INSTRUMENTS_H
#define INSTRUMENTS_H
#include "flight.h"
typedef struct {float tas,groundSpeed,track,heading,altitude,verticalSpeed,windFrom,windSpeed,slip,turnRate,bearing,distance,course,cdi;int to;} PfdData;
float pfdAirspeedLimit(const Flight *flight);
PfdData readPfd(const Flight *flight);
#endif
