#ifndef FLIGHT_AUDIO_H
#define FLIGHT_AUDIO_H
#include "flight.h"
int initFlightAudio(void);
void setFlightAudio(const Flight *s,int playing,int exterior,float volume,int muted);
void freeFlightAudio(void);
#endif
