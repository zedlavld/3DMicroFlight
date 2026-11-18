/* Host preview: procedural ground colour over the whole statewide grid
   (500 m base; no fog, with baked shadows). Writes a binary PPM.
   gcc -O2 -Iinclude tests/statewide_colour_map.c source/scenery.c
       source/terrain.c source/terrain_stream.c source/terrain_data.c
       source/airports.c source/flight.c source/ground.c source/imagery.c
       source/weather_visual.c -lm */
#include "scenery.h"
#include "terrain_stream.h"
#include <stdio.h>
int main(int argc,char **argv){
 const char *out=argc>1?argv[1]:"colour_map.ppm";float step=argc>2?(float)atof(argv[2]):1000;
 int w=(int)(ELEV_TX*ELEV_TILE_METRES/step),h=(int)(ELEV_TY*ELEV_TILE_METRES/step);
 FILE *f=fopen(out,"wb");if(!f)return 1;fprintf(f,"P6 %d %d 255\n",w,h);
 for(int y=h-1;y>=0;y--)for(int x=0;x<w;x++){
  float rgb[3];sceneryGroundColorAt(ELEV_E0+(x+.5f)*step,ELEV_N0+(y+.5f)*step,rgb);
  unsigned char c[3]={(unsigned char)(rgb[0]*255),(unsigned char)(rgb[1]*255),(unsigned char)(rgb[2]*255)};fwrite(c,1,3,f);
 }
 fclose(f);printf("%dx%d\n",w,h);return 0;
}
