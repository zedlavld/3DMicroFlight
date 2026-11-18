#ifndef AIRPORT_LIGHTS_H
#define AIRPORT_LIGHTS_H
#include "flight.h"
/* v34.02: standard airport lighting, generated from the runway records.
   Runways: edge lights (white, last 600 m yellow ahead of the viewer),
   threshold green / end red seen from the right side, centreline (white,
   alternating red/white 900-300 m from the far end, red the last 300 m) and
   touchdown-zone bars on runways 2,400 m and longer, a PAPI on the left of
   each threshold. Approach lights: ALSF-2 (720 m, sequenced flashers,
   crossbars, red side rows) on runways 2,700 m and longer, MALSR (420 m
   with flashers to 720 m) from 1,800 m, REIL strobes below that.
   Taxiways at the primary runway: blue edge lights, green centreline on the
   connectors, warm apron floodlights.
   emit(p, r, g, b, lamp size m, minimum on-screen pixels). */
typedef void (*LightEmit)(Vec p,float r,float g,float b,float size,float minPixels);
/* animated 0: the steady lamps (refresh a few times a second: colours that
   depend on where the viewer is change slowly); 1: only the flashing ones
   (sequenced flashers, REIL), every frame. */
void airportLights(Vec eye,float time,float night,LightEmit emit,int animated);
#endif
