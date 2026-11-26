/* Host copy of pfdAirspeedLimit (source/instruments.c draws with citro2d) for tests/route_landing_test.c. Keep in step with instruments.c. */
#include "flight.h"
#include <math.h>
float pfdAirspeedLimit(const Flight *s){
 const Aircraft *a=&aircraft[s->aircraft];Atmosphere atm=atmosphere(s->pos.z,s->isaOffset);
 float limit=a->mmo>0?fminf(a->vmo,calibratedAirspeed(atm.sound*a->mmo,atm)):a->vmo;
 if(a->gearLimit>0&&s->gearActual>.05f)limit=fminf(limit,a->gearLimit);
 if(s->flapActual>.01f)limit=fminf(limit,s->flapActual>.51f?a->flapLimit[1]:a->flapLimit[0]);
 return limit;
}
