/* Pixel font; see include/pixel_font.h. The glyphs are packed into one
   128x64 RGBA8 texture (white, alpha = coverage) at start-up and drawn as
   citro2d images tinted to the label colour. */
#include <citro2d.h>
#include <string.h>
#include <math.h>
#include "pixel_font.h"
#include "pixel_font_glyphs.h"
#define PF_TEX_W 128
#define PF_TEX_H 64
#define PF_CELL_W 6
#define PF_CELL_H 10
#define PF_PER_ROW (PF_TEX_W/PF_CELL_W)
static C3D_Tex texture;static Tex3DS_SubTexture glyphs[PF_COUNT];static int ready;
/* Texels in 8x8 Morton-ordered tiles. */
static void setTexel(u32 *data,int x,int y){
 int ty=y; /* memory row 0 is the top (v=1) as citro2d samples it */
 unsigned tile=((ty>>3)*(PF_TEX_W>>3)+(x>>3))*64;
 unsigned m=(x&1)|((ty&1)<<1)|((x&2)<<1)|((ty&2)<<2)|((x&4)<<2)|((ty&4)<<3);
 data[tile+m]=0xFFFFFFFFu;
}
int pixelFontInit(void){
 if(ready)return 1;
 if(!C3D_TexInit(&texture,PF_TEX_W,PF_TEX_H,GPU_RGBA8))return 0;
 memset(texture.data,0,texture.size);
 for(int i=0;i<PF_COUNT;i++){
  int cx=(i%PF_PER_ROW)*PF_CELL_W,cy=(i/PF_PER_ROW)*PF_CELL_H,w=pfWidth[i];
  for(int y=0;y<PF_ROWS;y++)for(int x=0;x<w;x++)if(pfRows[i][y]&(1<<x))setTexel((u32*)texture.data,cx+x,cy+y);
  glyphs[i]=(Tex3DS_SubTexture){(u16)(w?w:1),PF_ROWS,(float)cx/PF_TEX_W,1-(float)cy/PF_TEX_H,(float)(cx+(w?w:1))/PF_TEX_W,1-(float)(cy+PF_ROWS)/PF_TEX_H};
 }
 C3D_TexFlush(&texture);C3D_TexSetFilter(&texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
 ready=1;return 1;
}
void pixelFontFree(void){if(ready){C3D_TexDelete(&texture);ready=0;}}
int pixelFontReady(void){return ready;}
static int glyphOf(unsigned char c){return c>=PF_FIRST&&c<PF_FIRST+PF_COUNT&&pfWidth[c-PF_FIRST]?c-PF_FIRST:-1;}
int pixelFontSupports(const char *text){for(;*text;text++)if(glyphOf((unsigned char)*text)<0)return 0;return 1;}
int pixelFontWidth(const char *text){
 int w=0,n=0;for(;*text;text++){int g=glyphOf((unsigned char)*text);if(g>=0){w+=pfWidth[g]+1;n++;}}
 return n?w-1:0;
}
void pixelFontDraw(float x,float y,u32 color,const char *text){
 if(!ready)return;
 C2D_ImageTint tint;C2D_PlainImageTint(&tint,color,1);
 float px=roundf(x),py=roundf(y);
 for(;*text;text++){
  int g=glyphOf((unsigned char)*text);if(g<0)continue;
  if(*text!=' ')C2D_DrawImageAt((C2D_Image){&texture,&glyphs[g]},px,py,0,&tint,1,1);
  px+=pfWidth[g]+1;
 }
}
