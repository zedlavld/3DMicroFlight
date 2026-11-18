#ifndef FLIGHT_SAVE_H
#define FLIGHT_SAVE_H
#include "camera.h"
/* One versioned slot with transactional replacement and a previous-save backup.
   Caller pauses physics before any filesystem operation. */
int saveFlightState(const char *path,const Flight *flight,const View *view);
int loadFlightState(const char *path,Flight *flight,View *view);
#endif
