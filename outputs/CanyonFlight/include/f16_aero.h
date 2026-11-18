#ifndef F16_AERO_H
#define F16_AERO_H
/* v33 F-16 tables (source/f16_aero.c): Stevens & Lewis condensed model of
   the NASA TP-1538 wind-tunnel data. Body-axis coefficients (x forward,
   y right, z down); moments about the 0.35 cbar cg. */
#include "flight.h"
typedef struct {float cx,cy,cz,cl,cm,cn;} F16Coefficients;
/* alpha, beta (rad), true airspeed (m/s), body rates (rad/s); controls in
   the simulator's -1..1 convention (positive = nose up / roll right /
   nose right). */
F16Coefficients f16Coefficients(float alpha,float beta,float speed,float p,float q,float r,float elevator,float aileron,float rudder);
float f16Power(float throttle);                       /* percent: 50 = military, 100 = full afterburner */
float f16ThrustNewtons(float throttle,float mach,float altitude);
/* Angle of attack and elevator (simulator units) for steady flight at true
   airspeed v, density rho, flight path gamma and the given mass. */
void f16Trim(float v,float rho,float mass,float gamma,float *alpha,float *elevator);
#define F16_AREA 27.871f
#define F16_SPAN 9.144f
#define F16_CHORD 3.4503f
#endif
