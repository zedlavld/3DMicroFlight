#ifndef FBW_H
#define FBW_H
/* v33 flight control laws (see source/fbw.c). The pilot's stick/pedal
   inputs go in; surface commands come out. Direct-law aircraft pass
   through unchanged (the 737 adds its yaw damper). */
#include "flight.h"
void fbwReset(void);
void fbwControl(Flight *s,float dt,float *elevator,float *aileron,float *rudder);
/* Short status for the PFD: "NORMAL LAW", "ALPHA PROT", ... or NULL. */
const char *fbwStatus(const Flight *s);
#endif
