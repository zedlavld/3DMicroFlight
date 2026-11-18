#ifndef CAMERA_H
#define CAMERA_H
#include "flight.h"
typedef struct {int exterior;float insideYaw,insidePitch,orbitYaw,orbitPitch,distance;} View;
typedef struct {Vec eye,right,up,forward;} CameraPose;
void resetView(View *view);
void zoomView(View *view,float dt,float direction);
void moveView(View *view,float dt,float stickX,float stickY);
CameraPose makeCamera(const Flight *flight,const View *view);
Vec pilotEyeWorld(const Flight *flight);
void aircraftViewRows(const Flight *flight,const CameraPose *camera,float rows[4][4]);
void cameraViewRows(const CameraPose *camera,Vec anchor,float rows[4][4]);
Vec projectCamera(const CameraPose *camera,Vec world);
int cameraSphereVisible(const CameraPose *camera,Vec center,float radius,float iod,float convergence);
void stereoProjectionTerms(float signedIod,float convergence,float scale,float *z,float *w);
#endif
