#pragma once
#include "3ds.h"
typedef struct {void *data;unsigned size,maxLevel;} C3D_Tex;
#define GPU_RGB565 0
#define GPU_ETC1 12
#define GPU_LINEAR 0
#define GPU_CLAMP_TO_EDGE 0
bool C3D_TexInitMipmap(C3D_Tex*,int,int,int);
void C3D_TexDelete(C3D_Tex*);
unsigned C3D_TexCalcTotalSize(unsigned,unsigned);
void C3D_TexSetFilter(C3D_Tex*,int,int);void C3D_TexSetFilterMipmap(C3D_Tex*,int);void C3D_TexSetWrap(C3D_Tex*,int,int);
