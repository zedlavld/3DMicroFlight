#ifndef AIRCRAFT_MESH_H
#define AIRCRAFT_MESH_H
#include "flight.h"
/* v33: 16000 for the imported 737 (13,332) and F-16 (14,593) exteriors. */
#define AIRCRAFT_FACE_CAPACITY 16000
typedef struct {Vec p;float u,v,light;} ModelVertex;
typedef struct {ModelVertex v[3];float depth;} AircraftFace;
typedef struct {int firstTriangle,triangleCount;Vec pivot;float rate;} AircraftDynamicPart;
AircraftDynamicPart aircraftDynamicPart(void);
int buildAircraftMesh(const Flight *s,float propAngle,AircraftFace *destination,int capacity);
#endif
