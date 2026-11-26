/* v1.1p18 moving map background; see include/map_render.h. */
#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <math.h>
#include "map_render.h"
#include "scenery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAP_TEX 128
#define MAP_ROWS_PER_FRAME 16
static C3D_Tex textures[2];static int texturesReady;
static const Tex3DS_SubTexture fullTexture={MAP_TEX,MAP_TEX,0.f,1.f,1.f,0.f};
static int front=-1,building,row;
static float frontEast,frontNorth,frontSide,buildEast,buildNorth,buildSide;
int mapInit(void){
 if(texturesReady)return 1;
 for(int i=0;i<2;i++){
  if(!C3D_TexInit(&textures[i],MAP_TEX,MAP_TEX,GPU_RGB565)){if(i)C3D_TexDelete(&textures[0]);return 0;}
  C3D_TexSetFilter(&textures[i],GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&textures[i],GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
 }
 texturesReady=1;front=-1;building=0;return 1;
}
void mapFree(void){
 if(!texturesReady)return;
 for(int i=0;i<2;i++)C3D_TexDelete(&textures[i]);
 texturesReady=0;front=-1;building=0;
}
static u16 rgb565(float r,float g,float b){
 int R=(int)(fminf(fmaxf(r,0),1)*31+.5f),G=(int)(fminf(fmaxf(g,0),1)*63+.5f),B=(int)(fminf(fmaxf(b,0),1)*31+.5f);
 return (u16)(R<<11|G<<5|B);
}
/* 8 x 8 tiles, Morton order inside a tile; memory row 0 is the top of the image as citro2d
   draws it (checked against the ground map: flipping it mirrored north and south). */
static int texel(int x,int y){
 int ty=y,m=0;
 for(int b=0;b<3;b++)m|=((x>>b)&1)<<(2*b)|((ty>>b)&1)<<(2*b+1);
 return ((ty>>3)*(MAP_TEX>>3)+(x>>3))*64+m;
}
void mapUpdate(float east,float north,float side){
 if(!texturesReady)return;
 if(!building){
  int stale=front<0||hypotf(east-frontEast,north-frontNorth)>frontSide*.1f||fabsf(side-frontSide)>side*.01f;
  if(!stale)return;
  building=1;row=0;buildEast=east;buildNorth=north;buildSide=side;
 }
 int back=front<0?0:front^1;u16 *data=textures[back].data;float step=buildSide/MAP_TEX;
 for(int k=0;k<MAP_ROWS_PER_FRAME&&row<MAP_TEX;k++,row++){
  float n=buildNorth+(MAP_TEX/2-.5f-row)*step;   /* image row 0 is north */
  for(int x=0;x<MAP_TEX;x++){
   float e=buildEast+(x-MAP_TEX/2+.5f)*step,rgb[3];
   /* darkened so the white and magenta symbols stand out; no data: dark slate */
   u16 c=sceneryGroundColor(e,n,rgb)?rgb565(rgb[0]*.62f,rgb[1]*.62f,rgb[2]*.62f):rgb565(.10f,.13f,.18f);
   data[texel(x,row)]=c;
  }
 }
 if(row>=MAP_TEX){
  GSPGPU_FlushDataCache(data,MAP_TEX*MAP_TEX*sizeof(u16));
  front=back;frontEast=buildEast;frontNorth=buildNorth;frontSide=buildSide;building=0;
 }
}
#define AIRPORTS_PATH "sdmc:/3ds/CanyonFlight/scenery/airports_us.bin"
static int aptState; /* 0 not tried, 1 loaded, -1 missing */
static int aptCell,aptE0,aptN0,aptGW,aptGH,aptCount,aptRunways;
static uint32_t *aptFirst;static MapAirport *apt;static MapRunway *aptRw;static unsigned char *aptData;
static int rd32(const unsigned char *p){return p[0]|p[1]<<8|p[2]<<16|p[3]<<24;}
static int aptLoad(void){
 if(aptState)return aptState>0;
 aptState=-1;FILE *f=fopen(AIRPORTS_PATH,"rb");if(!f)return 0;
 unsigned char head[36];
 if(fread(head,1,36,f)!=36||memcmp(head,"CFAPT001",8)){fclose(f);return 0;}
 aptCell=rd32(head+8);aptE0=rd32(head+12);aptN0=rd32(head+16);aptGW=rd32(head+20);aptGH=rd32(head+24);aptCount=rd32(head+28);aptRunways=rd32(head+32);
 if(aptCell<=0||aptGW<=0||aptGH<=0||aptGW*aptGH>200000||aptCount<0||aptCount>100000||aptRunways<0||aptRunways>65535){fclose(f);return 0;}
 size_t bytes=4*((size_t)aptGW*aptGH+1)+sizeof(MapAirport)*aptCount+sizeof(MapRunway)*aptRunways;
 aptData=malloc(bytes);
 if(!aptData||fread(aptData,1,bytes,f)!=bytes){free(aptData);aptData=NULL;fclose(f);return 0;}
 fclose(f);
 aptFirst=(uint32_t*)aptData;apt=(MapAirport*)(aptData+4*((size_t)aptGW*aptGH+1));aptRw=(MapRunway*)((unsigned char*)apt+sizeof(MapAirport)*aptCount);
 if(aptFirst[aptGW*aptGH]!=(uint32_t)aptCount){free(aptData);aptData=NULL;return 0;}
 aptState=1;return 1;
}
int mapAirportCells(float east,float north,float radius,int *x0,int *y0,int *x1,int *y1){
 if(!aptLoad())return 0;
 *x0=(int)floorf((east-radius-aptE0)/aptCell);*x1=(int)floorf((east+radius-aptE0)/aptCell);
 *y0=(int)floorf((north-radius-aptN0)/aptCell);*y1=(int)floorf((north+radius-aptN0)/aptCell);
 if(*x0<0)*x0=0;if(*y0<0)*y0=0;if(*x1>=aptGW)*x1=aptGW-1;if(*y1>=aptGH)*y1=aptGH-1;
 return *x0<=*x1&&*y0<=*y1;
}
const MapAirport *mapAirportsInCell(int cx,int cy,int *count){
 int c=cy*aptGW+cx;*count=(int)(aptFirst[c+1]-aptFirst[c]);return apt+aptFirst[c];
}
const MapRunway *mapRunways(void){return aptRw;}
int mapImage(C2D_Image *image,float *centreEast,float *centreNorth,float *side){
 if(front<0)return 0;
 image->tex=&textures[front];image->subtex=&fullTexture;
 *centreEast=frontEast;*centreNorth=frontNorth;*side=frontSide;return 1;
}
