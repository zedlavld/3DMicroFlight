#ifndef MAP_RENDER_H
#define MAP_RENDER_H
#include <citro2d.h>
/* v1.1p18 moving map (lower screen, PFD + MAP). The background is a 128 x 128 north-up
   texture of the ground colour map (scenery/ground_us4000.bin, 4 km per pixel) around a
   centre point; mapUpdate rebuilds it 16 rows per frame into the second texture when the
   aircraft has moved a tenth of the way to its edge or the range changed, then swaps. */
int mapInit(void);
void mapFree(void);
/* sideMetres: the ground width the texture must cover. */
void mapUpdate(float east,float north,float sideMetres);
/* The current texture and the ground point at its centre; 0 until the first is built. */
int mapImage(C2D_Image *image,float *centreEast,float *centreNorth,float *sideMetres);
/* v1.1p19: every lower-48 airport for the map, from scenery/airports_us.bin
   (tools/build_us_airports_map.py; the compiled airports are not in it). Class 0 large,
   1 medium, 2 small with 3000 ft+ paved, 3 other small. Runway ends are metres from the
   airport. Loaded on first use; 0 when the file is missing. */
typedef struct {int32_t east,north;uint16_t first;uint8_t runways,cls;char id[8];} MapAirport;
typedef struct {int16_t east1,north1,east2,north2;uint8_t paved,width;} MapRunway;
/* Grid cells covering east/north +- radius; 0 without the file. */
int mapAirportCells(float east,float north,float radius,int *x0,int *y0,int *x1,int *y1);
const MapAirport *mapAirportsInCell(int cx,int cy,int *count);
const MapRunway *mapRunways(void);
#endif
