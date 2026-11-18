#ifndef FLIGHT_RECORDER_H
#define FLIGHT_RECORDER_H
#include "flight.h"
#define FLIGHT_RECORD_CAPACITY 4096
void flightRecorderBegin(const Flight *s);
void flightRecorderStep(const Flight *s,float dt);
void flightRecorderStop(void);
int flightRecorderActive(void);
int flightRecorderCount(void);
int flightRecorderSave(const char *path);
#endif
