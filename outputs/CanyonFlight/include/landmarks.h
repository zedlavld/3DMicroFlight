#ifndef LANDMARKS_H
#define LANDMARKS_H
/* US landmarks as simple original models (source/landmarks.c, from the Thor port). */
typedef struct {float x,y,z;unsigned char r,g,b,pad;} LandmarkVertex;
int landmarkCount(void);
const char *landmarkName(int i);
/* Scenery-frame position of the landmark's anchor, and its east-west stretch. */
void landmarkAnchor(int i,float *east,float *north,float *stretch);
/* Draw radius in metres, or 0 for the default. */
float landmarkRadius(int i);
/* Builds landmark i on ground height 'ground' (m) at its anchor: returns the vertex count
   (triangle list, anticlockwise from outside) in a malloc'd array the caller frees; x, y in
   scenery metres from the anchor, z from *baseZ. The first *farCount vertices are the parts
   at least 3 m thick (thin cables and struts follow): enough from far away. */
int landmarkTriangles(int i,float ground,LandmarkVertex **out,float *baseZ,int *farCount);
#endif
