#ifndef PIXEL_FONT_H
#define PIXEL_FONT_H
/* v31.18: original 5x7 proportional pixel font for small text, drawn 1:1
   with nearest filtering so it stays sharp at sizes where the scaled-down
   system font breaks up. Glyphs: tools/build_pixel_font.py. Cap height 7 px,
   descenders 2 px, 1 px between glyphs. */
#include <3ds.h>
int pixelFontInit(void);
void pixelFontFree(void);
int pixelFontReady(void);
/* 1 when every character has a glyph. */
int pixelFontSupports(const char *text);
int pixelFontWidth(const char *text);
/* Top-left of the cap height at (x,y), whole pixels. */
void pixelFontDraw(float x,float y,u32 color,const char *text);
#endif
