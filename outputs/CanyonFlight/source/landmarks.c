/* 3DMicroFlight (3DS): US landmarks as simple original models built from boxes, struts,
   cylinders and spheres. The models are the Thor port's (source/thor_landmarks.c on the
   Thor branch), unchanged; positions, outlines and headings from OpenStreetMap (ODbL),
   sizes from published dimensions. Here the primitives become lit triangles
   (landmarkTriangles) that source/aircraft_render.c draws like the city buildings.
   Each landmark is built in its own frame: x east and y north in real metres from the
   anchor, z absolute height (m above sea level); x is stretched to the scenery frame. */
#include "landmarks.h"
#include "terrain.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAXP 6000
#define FRAME_LAT 34.848598480225
#define FRAME_LON (-111.78800201416)
#define DEG (3.14159265358979/180)
typedef struct {float i0[4],i1[4];unsigned char c0[4],c1[4];} CfObjectInstance; /* as the Thor renderer's instances */

typedef struct {unsigned char r,g,b;} RGB;
static CfObjectInstance prims[3][MAXP];static int primCount[3];
static double anchorE,anchorN;static float anchorStretch; /* the landmark being built */

static void prim(int kind,float x,float y,float z,float yaw,float tilt,float sx,float sy,float sz,RGB top,RGB side,float taper){
 int k=kind-1;if(k<0||k>2||primCount[k]>=MAXP)return;
 CfObjectInstance *o=&prims[k][primCount[k]++];
 o->i0[0]=x;o->i0[1]=y;o->i0[2]=z;o->i0[3]=yaw;o->i1[0]=sx;o->i1[1]=sy;o->i1[2]=sz;o->i1[3]=tilt;
 o->c0[0]=top.r;o->c0[1]=top.g;o->c0[2]=top.b;o->c0[3]=255;
 o->c1[0]=side.r;o->c1[1]=side.g;o->c1[2]=side.b;
 o->c1[3]=taper>.995f&&taper<1.005f?255:(unsigned char)fminf(fmaxf(taper*100+.5f,0),254);
}
/* Box: centred on x,y, base at z, length along yaw (radians from east). */
static void box(float x,float y,float z,float yaw,float l,float w,float h,RGB top,RGB side,float taper){prim(1,x,y,z,yaw,0,l,w,h,top,side,taper);}
static void cyl(float x,float y,float z,float d,float h,RGB c,float taper){prim(2,x,y,z,0,0,d,d,h,c,c,taper);}
static void sph(float x,float y,float z,float d,float h,RGB c){prim(3,x,y,z,0,0,d,d,h,c,c,1);}
/* Strut from a to b: w across in the tilt plane, d sideways; extend adds length at b. */
static void strut(float ax,float ay,float az,float bx,float by,float bz,float w,float d,RGB c,float extend){
 float dx=bx-ax,dy=by-ay,dz=bz-az,hl=hypotf(dx,dy),l=sqrtf(dx*dx+dy*dy+dz*dz);if(l<.01f)return;
 prim(1,ax,ay,az,hl>.001f?atan2f(dy,dx):0,atan2f(hl,dz),w,d,l+extend,c,c,1);
}
/* Ground height under a point of the landmark frame. */
static float groundAt(float x,float y){return rawElevation((float)(anchorE+x*anchorStretch),(float)(anchorN+y));}
/* Axis frame: u along heading th (radians from east), v to its left. */
static void axis(float th,float u,float v,float *x,float *y){*x=u*cosf(th)-v*sinf(th);*y=u*sinf(th)+v*cosf(th);}

static const RGB WHITE={234,232,224},MARBLE={226,222,208},CONCRETE={212,205,190},STEEL={196,200,204},ROAD={104,104,104},
 GRANITE={190,180,164};

/* Golden Gate Bridge: 1280 m main span, towers 227 m, deck 67 m over the water. */
static void goldenGate(float g){
 (void)g;const RGB ORANGE={186,62,42};const float th=-84.59f*DEG,deck=75,top=224,cableV=17.5f;float x,y;
 axis(th,0,0,&x,&y);box(x,y,deck-8,th,1960,27,8,ROAD,ORANGE,1);
 for(int s=-1;s<=1;s+=2){float ut=640.f*s;
  axis(th,ut,0,&x,&y);box(x,y,-12,th,48,62,22,CONCRETE,CONCRETE,1); /* pier */
  for(int side=-1;side<=1;side+=2){axis(th,ut,side*cableV,&x,&y);box(x,y,-2,th,16,9,229,ORANGE,ORANGE,.62f);}
  static const float portal[5]={55,105,148,188,219};
  for(int i=0;i<5;i++){axis(th,ut,0,&x,&y);box(x,y,portal[i],th,6,2*cableV,i==4?9:7,ORANGE,ORANGE,1);}
  /* anchorage and approach viaduct */
  float ua=1000.f*s;axis(th,ua,0,&x,&y);float ga=groundAt(x,y);box(x,y,ga-4,th,46,56,fmaxf(70-ga,8)+4,CONCRETE,CONCRETE,1);
  float uEnd=s>0?1330.f:-1370.f;axis(th,uEnd,0,&x,&y);float gEnd=groundAt(x,y)+4;
  for(int k=0;k<10;k++){float t0=k/10.f,t1=(k+1)/10.f,u0=1000.f*s+(uEnd-1000.f*s)*t0,u1=1000.f*s+(uEnd-1000.f*s)*t1,um=(u0+u1)/2;
   float h=deck+(gEnd-deck)*(t0+t1)/2;axis(th,um,0,&x,&y);float gm=groundAt(x,y);if(h<gm+3)h=gm+3;
   box(x,y,h-5,th,fabsf(u1-u0)+1,27,5,ROAD,ORANGE,1);
   if(h-5-gm>3)for(int sv=-1;sv<=1;sv+=2){axis(th,um,sv*9.f,&x,&y);box(x,y,gm-2,th,3,3,h-5-gm+2,ORANGE,ORANGE,1);}}
 }
 for(int side=-1;side<=1;side+=2){float v=side*cableV;
  /* main span parabola, then the side spans to the anchorages */
  for(int i=0;i<32;i++){float u0=-640+40.f*i,u1=u0+40,z0=78+(top-78)*(u0/640)*(u0/640),z1=78+(top-78)*(u1/640)*(u1/640),ax,ay,bx,by;
   axis(th,u0,v,&ax,&ay);axis(th,u1,v,&bx,&by);strut(ax,ay,z0,bx,by,z1,1.5f,1.5f,ORANGE,.6f);}
  for(int s=-1;s<=1;s+=2)for(int i=0;i<8;i++){float t0=i/8.f,t1=(i+1)/8.f,u0=s*(640+340*t0),u1=s*(640+340*t1);
   float z0=top+(68-top)*t0-18*sinf(3.14159f*t0),z1=top+(68-top)*t1-18*sinf(3.14159f*t1),ax,ay,bx,by;
   axis(th,u0,v,&ax,&ay);axis(th,u1,v,&bx,&by);strut(ax,ay,z0,bx,by,z1,1.5f,1.5f,ORANGE,.6f);}
  for(float u=-975;u<=975;u+=15.24f){float a=fabsf(u),z;
   if(a<=640)z=78+(top-78)*(u/640)*(u/640);else{float t=(a-640)/340;z=top+(68-top)*t-18*sinf(3.14159f*t);}
   if(z-deck<2)continue;axis(th,u,v,&x,&y);box(x,y,deck,th,.5f,.5f,z-deck,ORANGE,ORANGE,1);}
 }
}
/* Hoover Dam: concrete arch-gravity dam, crest 375.5 m, 221 m high, crest radius about 152 m,
   bulging upstream (north-east); four intake towers in the lake. */
static void hooverDam(float g){
 (void)g;const float th=-41.73f*DEG,cu=5,cv=-89,R=152,crest=375.5f,base=154.5f;float x,y;
 for(int b=0;b<10;b++){float z0=base+(crest-4-base)*b/10.f,z1=base+(crest-4-base)*(b+1)/10.f,zm=(z0+z1)/2;
  float t=200+(14-200)*(zm-base)/(crest-base),rc=R-t/2,phiMax=72*DEG*(.35f+.65f*(z1-base)/(crest-base));
  const int N=24;for(int i=0;i<N;i++){float p0=-phiMax+2*phiMax*i/N,p1=p0+2*phiMax/N,pm=(p0+p1)/2;
   float u=cu+rc*sinf(pm),v=cv+rc*cosf(pm),tu=cosf(pm),tv=-sinf(pm);axis(th,u,v,&x,&y);float tx,ty;axis(th,tu,tv,&tx,&ty);
   box(x,y,z0,atan2f(ty,tx),2*rc*sinf((p1-p0)/2)*1.06f,t,z1-z0+.5f,CONCRETE,CONCRETE,1);}}
 {const int N=30;float phiMax=72*DEG,rc=R-7;for(int i=0;i<N;i++){float p0=-phiMax+2*phiMax*i/N,p1=p0+2*phiMax/N,pm=(p0+p1)/2;
   float u=cu+rc*sinf(pm),v=cv+rc*cosf(pm),tx,ty;axis(th,u,v,&x,&y);axis(th,cosf(pm),-sinf(pm),&tx,&ty);
   box(x,y,crest-4.5f,atan2f(ty,tx),2*rc*sinf((p1-p0)/2)*1.06f,14,4.5f,ROAD,CONCRETE,1);}}
 static const float tower[4][2]={{-50,48},{-63,95},{50,48},{63,95}};
 for(int i=0;i<4;i++){float pm=tower[i][0]*DEG,r=R+tower[i][1];axis(th,cu+r*sinf(pm),cv+r*cosf(pm),&x,&y);
  cyl(x,y,225,24,160,CONCRETE,.85f);cyl(x,y,385,15,9,CONCRETE,.5f);}
}
/* Mike O'Callaghan-Pat Tillman Memorial Bridge: twin-rib concrete arch, 323 m span,
   deck about 270 m over the river and 579 m long. */
static void tillmanBridge(float g){
 (void)g;const float th=-30.72f*DEG,deck=468,crown=deck-12,rise=110,half=161.5f;float x,y;
 axis(th,0,0,&x,&y);box(x,y,deck-4,th,580,26,4,ROAD,CONCRETE,1);
 for(int side=-1;side<=1;side+=2){float v=side*9.f;
  for(int i=0;i<24;i++){float u0=-half+2*half*i/24,u1=u0+2*half/24,z0=crown-rise*(u0/half)*(u0/half),z1=crown-rise*(u1/half)*(u1/half),ax,ay,bx,by;
   axis(th,u0,v,&ax,&ay);axis(th,u1,v,&bx,&by);strut(ax,ay,z0,bx,by,z1,4.5f,6,CONCRETE,2);}}
 for(float u=-half+13;u<half;u+=27){float z=crown-rise*(u/half)*(u/half);axis(th,u,0,&x,&y);box(x,y,z,th,2,18,2.5f,CONCRETE,CONCRETE,1);}
 for(int k=0;k<16;k++){float u=-274+36.5f*k;
  for(int side=-1;side<=1;side+=2){float v=side*9.f;axis(th,u,v,&x,&y);
   float bottom=fabsf(u)<half?crown-rise*(u/half)*(u/half):groundAt(x,y)-2;
   if(deck-4-bottom>3)box(x,y,bottom,th,3.5f,3.5f,deck-4-bottom,CONCRETE,CONCRETE,1);}
  axis(th,u,0,&x,&y);box(x,y,deck-6,th,2.5f,22,2,CONCRETE,CONCRETE,1);}
}
/* Gateway Arch: weighted catenary, 192 m high and wide; section 16.5 m at the base, 5.2 m at the top. */
static void gatewayArch(float g){
 const float th=72.55f*DEG,f=.3048f;const int N=48;float px=0,py=0,pz=0;
 for(int i=0;i<=N;i++){double xf=-299.2239+598.4478*i/N,yf=693.8597-68.7672*cosh(.0100333*xf);
  float x,y,z=g+(float)yf*f;axis(th,(float)xf*f,0,&x,&y);
  if(i){float s=16.46f-(16.46f-5.18f)*fminf(fmaxf((pz+z)/2-g,0)/190.f,1);strut(px,py,pz,x,y,z,s*.85f,s*.85f,STEEL,s*.35f);}
  px=x;py=y;pz=z;}
}
/* Washington Monument: 152 m shaft, 16.8 m to 10.5 m square, 16.9 m pyramidion. */
static void washingtonMonument(float g){
 box(0,0,g-3,0,16.8f,16.8f,155.4f,MARBLE,MARBLE,.625f);box(0,0,g+152.4f,0,10.5f,10.5f,16.9f,MARBLE,MARBLE,0);
}
/* United States Capitol: 229 m north-south, dome to 88 m. */
static void capitol(float g){
 const float th=90*DEG;const RGB ROOF={204,204,198},BRONZE={88,78,66};float x,y;
 box(0,0,g-3,th,108,70,31,ROOF,WHITE,1);
 for(int s=-1;s<=1;s+=2){axis(th,s*82.f,0,&x,&y);box(x,y,g-3,th,66,100,28,ROOF,WHITE,1);}
 axis(th,0,-40,&x,&y);box(x,y,g-3,th,42,14,25,ROOF,WHITE,1); /* east portico */
 cyl(0,0,g+25,40,22,WHITE,1);cyl(0,0,g+47,33,10,WHITE,1);sph(0,0,g+35,33,44,WHITE);
 cyl(0,0,g+76,6,7,WHITE,.8f);cyl(0,0,g+83,2,5,BRONZE,.5f);
}
/* Lincoln Memorial: 57.8 x 36.1 m, 36 columns, 24 m high. */
static void lincolnMemorial(float g){
 const float th=90*DEG;float x,y;
 box(0,0,g-3,th,62,44,8,MARBLE,MARBLE,1);box(0,0,g+5,th,48,26,19,MARBLE,MARBLE,1);box(0,0,g+18,th,58,36,6.4f,MARBLE,MARBLE,1);
 for(int i=0;i<12;i++)for(int s=-1;s<=1;s+=2){axis(th,-27.5f+5*i,s*16.5f,&x,&y);cyl(x,y,g+5,2.3f,13,MARBLE,1);}
 for(int i=1;i<=6;i++)for(int s=-1;s<=1;s+=2){axis(th,s*27.5f,-16.5f+33*i/7.f,&x,&y);cyl(x,y,g+5,2.3f,13,MARBLE,1);}
}
/* Empire State Building: setbacks to 320 m, mast to 381 m, antenna to 443 m. */
static void empireState(float g){
 const float th=150.9f*DEG;const RGB STONE={196,190,178},MAST={212,208,198};
 static const float stack[6][4]={{-3,25,128,57},{25,85,78,52},{85,112,68,47},{112,300,61,43},{300,322,52,36},{322,345,40,28}};
 for(int i=0;i<6;i++)box(0,0,g+stack[i][0],th,stack[i][2],stack[i][3],stack[i][1]-stack[i][0],STONE,STONE,1);
 box(0,0,g+345,th,20,20,28,MAST,MAST,.6f);cyl(0,0,g+373,9,8,MAST,.6f);cyl(0,0,g+381,2.5f,62,MAST,.3f);
}
/* Willis Tower: nine 22.9 m tubes ending at 206, 272, 368 and 442 m; antennas to 527 m. */
static void willisTower(float g){
 const float th=.9f*DEG,s=22.86f;const RGB DARK={40,40,46},ANT={226,226,226};float x,y;
 static const float h[3][3]={{272,442,206},{368,442,368},{206,368,272}}; /* [west..east][south..north] */
 for(int i=0;i<3;i++)for(int j=0;j<3;j++){axis(th,(i-1)*s,(j-1)*s,&x,&y);box(x,y,g-3,th,s+.1f,s+.1f,h[i][j]+3,DARK,DARK,1);}
 for(int k=0;k<2;k++){axis(th,k?.1f*s:-1.1f*s,0,&x,&y);cyl(x,y,g+442,3,85,ANT,.35f);}
}
/* Luxor: glass pyramid, 182 m base, 109 m high. */
static void luxor(float g){const RGB GLASS={34,34,42};box(0,0,g-2,.3f*DEG,182,182,111,GLASS,GLASS,0);
 /* the Sky Beam from the apex: drawn at night only (c0.a 0) */
 const RGB BEAM={220,230,255};cyl(0,0,g+108,7,3500,BEAM,1);if(primCount[1])prims[1][primCount[1]-1].c0[3]=0;}
/* The STRAT: 350 m tower on three legs, pod at 246-291 m. */
static void stratTower(float g){
 const RGB T={228,226,222},POD={196,202,210};
 cyl(0,0,g-3,20,253,T,.6f);
 for(int i=0;i<3;i++){float a=(90+120*i)*DEG;strut(26*cosf(a),26*sinf(a),g-3,8*cosf(a),8*sinf(a),g+235,9,6,T,0);}
 cyl(0,0,g+246,36,9,T,1);cyl(0,0,g+255,50,28,POD,.92f);cyl(0,0,g+283,46,8,T,.75f);cyl(0,0,g+291,9,59,T,.3f);
}
/* Space Needle: three leg pairs pinched at 112 m, saucer at 145-167 m, spire to 184 m. */
static void spaceNeedle(float g){
 const RGB W={238,238,236},ROOF={222,222,220};
 cyl(0,0,g,7,150,W,1);
 for(int i=0;i<3;i++)for(int s=-1;s<=1;s+=2){float a=(90+120*i+12*s)*DEG,aw=(90+120*i+20*s)*DEG,at=(90+120*i+8*s)*DEG;
  strut(19*cosf(a),19*sinf(a),g,5.5f*cosf(aw),5.5f*sinf(aw),g+112,1.6f,1.6f,W,.5f);
  strut(5.5f*cosf(aw),5.5f*sinf(aw),g+112,10*cosf(at),10*sinf(at),g+147,1.6f,1.6f,W,.5f);}
 cyl(0,0,g+145,20,10,W,2.1f);cyl(0,0,g+155,42,6,W,.97f);cyl(0,0,g+161,40,6,ROOF,.35f);cyl(0,0,g+167,2,17,W,.3f);
}
/* Statue of Liberty: Fort Wood, granite pedestal to 47 m, copper figure and torch to 93 m. */
static void statueOfLiberty(float g){
 const RGB COPPER={112,166,146},GOLD={204,166,72};
 cyl(0,0,g-2,90,8,GRANITE,1);box(0,0,g+6,0,28,28,14,GRANITE,GRANITE,.75f);box(0,0,g+20,0,21,21,27,GRANITE,GRANITE,.85f);
 float face=-55*DEG,rx=cosf(face-90*DEG),ry=sinf(face-90*DEG),fx=cosf(face),fy=sinf(face);
 cyl(0,0,g+47,11,34,COPPER,.5f);sph(fx*.6f,fy*.6f,g+79.5f,5,6.5f,COPPER);
 float hx=rx*4.5f+fx*.8f,hy=ry*4.5f+fy*.8f;strut(rx*2.6f,ry*2.6f,g+76,hx,hy,g+89,1.7f,1.7f,COPPER,.4f);
 cyl(hx,hy,g+89,2.2f,2.5f,GOLD,1.3f);sph(hx,hy,g+91.2f,2,2.8f,GOLD);
}

/* ------------------------------------------------------------ Duluth and Tucson */
/* Real metres east/north of the landmark anchor for a lat/lon (same latitude scale as the frame stretch). */
static double anchorLat,anchorLon;
static void at(double lat,double lon,float *x,float *y){const double m=6371000*3.14159265358979/180;*x=(float)((lon-anchorLon)*m*cos(anchorLat*DEG));*y=(float)((lat-anchorLat)*m);}
/* Elevated road along a polyline (lat/lon pairs), deck top from a height profile,
   column bents every ~45 m where the ground is more than 4 m below the deck. */
typedef float (*DeckProfile)(float s,float total);
static void deckAlong(const double (*ll)[2],int n,float width,DeckProfile profile,RGB top,RGB side){
 float px[64],py[64],acc[64];float total=0;
 for(int i=0;i<n&&i<64;i++){at(ll[i][0],ll[i][1],&px[i],&py[i]);acc[i]=i?acc[i-1]+hypotf(px[i]-px[i-1],py[i]-py[i-1]):0;}
 total=acc[n-1];
 for(int i=1;i<n;i++){float seg=acc[i]-acc[i-1];int k=(int)ceilf(seg/30);if(k<1)k=1;
  for(int j=0;j<k;j++){float t0=(float)j/k,t1=(float)(j+1)/k,s=acc[i-1]+seg*(t0+t1)/2;
   float x0=px[i-1]+(px[i]-px[i-1])*t0,y0=py[i-1]+(py[i]-py[i-1])*t0,x1=px[i-1]+(px[i]-px[i-1])*t1,y1=py[i-1]+(py[i]-py[i-1])*t1;
   float xm=(x0+x1)/2,ym=(y0+y1)/2,yaw=atan2f(y1-y0,x1-x0),z=profile(s,total),g=groundAt(xm,ym);if(z<g+1.5f)z=g+1.5f;
   box(xm,ym,z-3,yaw,seg/k+.6f,width,3,top,side,1);
   if(fmodf(s,45)<seg/k&&z-3-g>4)for(int sv=-1;sv<=1;sv+=2){float cx=xm-sinf(yaw)*sv*width*.3f,cy=ym+cosf(yaw)*sv*width*.3f;
    box(cx,cy,g-2,yaw,3,3.5f,z-3-g+2,side,side,1);}}}
}
/* Point along a polyline at distance s. */
static void alongPoint(const double (*ll)[2],int n,float s,float *x,float *y,float *yaw){
 float ax,ay,bx,by;at(ll[0][0],ll[0][1],&ax,&ay);
 for(int i=1;i<n;i++){at(ll[i][0],ll[i][1],&bx,&by);float seg=hypotf(bx-ax,by-ay);
  if(s<=seg||i==n-1){float t=seg>0?fminf(s/seg,1):0;*x=ax+(bx-ax)*t;*y=ay+(by-ay)*t;*yaw=atan2f(by-ay,bx-ax);return;}
  s-=seg;ax=bx;ay=by;}
}
/* Through truss over a span: top chord a parabola rising `rise` above the deck, verticals and diagonals. */
static void trussSpan(float cx,float cy,float yaw,float deckTop,float span,float rise,float halfWidth,RGB c){
 const int N=12;float ux=cosf(yaw),uy=sinf(yaw),vx=-uy,vy=ux;
 for(int side=-1;side<=1;side+=2){float ox=vx*side*halfWidth,oy=vy*side*halfWidth;
  for(int i=0;i<N;i++){float s0=-span/2+span*i/N,s1=s0+span/N;
   float h0=deckTop+4+rise*(1-(2*s0/span)*(2*s0/span)),h1=deckTop+4+rise*(1-(2*s1/span)*(2*s1/span));
   strut(cx+ux*s0+ox,cy+uy*s0+oy,h0,cx+ux*s1+ox,cy+uy*s1+oy,h1,1.6f,1.6f,c,.8f);                 /* top chord */
   box(cx+ux*s1+ox,cy+uy*s1+oy,deckTop,yaw,1,1,h1-deckTop,c,c,1);                                  /* vertical */
   strut(cx+ux*s0+ox,cy+uy*s0+oy,deckTop,cx+ux*s1+ox,cy+uy*s1+oy,h1,.8f,.8f,c,0);}                 /* diagonal */
  box(cx+ox,cy+oy,deckTop-2,yaw,span,1.4f,2.5f,c,c,1);}                                             /* bottom chord */
 for(int i=1;i<N;i+=2){float s=-span/2+span*i/N,h=deckTop+4+rise*(1-(2*s/span)*(2*s/span));
  box(cx+ux*s,cy+uy*s,h-1,yaw,1,2*halfWidth,1,c,c,1);}                                             /* top bracing */
}
static const RGB BRIDGE_GREEN={70,98,86},BRIDGE_GREY={150,156,160},STONE={96,98,108};
#define LAKE 183.f
/* Duluth Aerial Lift Bridge: two 68.6 m lattice towers 118 m apart, top truss, lift span at the road. */
static void aerialLiftBridge(float g){
 (void)g;float ax,ay,bx,by;at(46.77957,-92.09329,&ax,&ay);at(46.77834,-92.09245,&bx,&by);
 float yaw=atan2f(by-ay,bx-ax),ux=cosf(yaw),uy=sinf(yaw),vx=-uy,vy=ux,cx=(ax+bx)/2,cy=(ay+by)/2,top=LAKE+68.6f;
 const RGB S={206,210,214};
 for(int t=-1;t<=1;t+=2){float tx=cx+ux*t*62,ty=cy+uy*t*62;
  for(int a=-1;a<=1;a+=2)for(int b=-1;b<=1;b+=2){float px=tx+ux*a*3.5f+vx*b*9.5f,py=ty+uy*a*3.5f+vy*b*9.5f;box(px,py,LAKE-2,yaw,1.6f,1.6f,top-LAKE+2,S,S,1);}
  for(float z=LAKE+8;z<top;z+=10)for(int b=-1;b<=1;b+=2){float px=tx+vx*b*9.5f,py=ty+vy*b*9.5f;   /* X bracing on the faces */
   strut(px-ux*3.5f,py-uy*3.5f,z,px+ux*3.5f,py+uy*3.5f,z+10,.6f,.6f,S,0);strut(px+ux*3.5f,py+uy*3.5f,z,px-ux*3.5f,py-uy*3.5f,z+10,.6f,.6f,S,0);}
  box(tx,ty,LAKE-3,yaw,12,24,6,CONCRETE,CONCRETE,1);}
 for(int b=-1;b<=1;b+=2){float ox=vx*b*9.5f,oy=vy*b*9.5f;box(cx+ox,cy+oy,top-13,yaw,131,1.5f,13,S,S,1);
  for(int i=0;i<12;i++){float s0=-62+124.f*i/12,s1=s0+124.f/12;strut(cx+ux*s0+ox,cy+uy*s0+oy,top-13,cx+ux*s1+ox,cy+uy*s1+oy,top,.7f,.7f,S,0);}}
 box(cx,cy,LAKE+5,yaw,118,14,1.5f,ROAD,S,1);
 for(int b=-1;b<=1;b+=2)box(cx+vx*b*7,cy+vy*b*7,LAKE+5,yaw,118,1,7,S,S,1);
}
static const double BLATNIK[][2]={{46.73488,-92.09772},{46.73780,-92.09840},{46.74100,-92.09905},{46.74500,-92.09960},{46.74922,-92.10098},{46.75200,-92.10290},{46.75530,-92.10528}};
static float blatnikProfile(float s,float total){(void)total;float peak=LAKE+40,d=fabsf(s-1640);return peak-(d<120?0:.045f*(d-120));}
static void blatnikBridge(float g){
 (void)g;anchorLat=46.74922;anchorLon=-92.10098;
 deckAlong(BLATNIK,7,23,blatnikProfile,ROAD,BRIDGE_GREEN);
 float x,y,yaw;alongPoint(BLATNIK,7,1640,&x,&y,&yaw);trussSpan(x,y,yaw,LAKE+40,183,26,11,BRIDGE_GREEN);
}
static const double BONG[][2]={{46.72778,-92.13416},{46.72960,-92.13860},{46.73158,-92.14334},{46.73300,-92.14600},{46.73474,-92.14919}};
static float bongProfile(float s,float total){(void)total;float peak=LAKE+40,d=fabsf(s-1035);return peak-(d<90?0:.05f*(d-90));}
static void bongBridge(float g){
 (void)g;anchorLat=46.73158;anchorLon=-92.14334;
 deckAlong(BONG,5,24,bongProfile,ROAD,BRIDGE_GREY);
 float x,y,yaw;alongPoint(BONG,5,1035,&x,&y,&yaw);
 float ux=cosf(yaw),uy=sinf(yaw),vx=-uy,vy=ux;const float span=165,rise=34,deck=LAKE+40;
 for(int side=-1;side<=1;side+=2){float ox=vx*side*13,oy=vy*side*13;   /* tied arch with hangers */
  for(int i=0;i<16;i++){float s0=-span/2+span*i/16,s1=s0+span/16,h0=deck+rise*(1-(2*s0/span)*(2*s0/span)),h1=deck+rise*(1-(2*s1/span)*(2*s1/span));
   strut(x+ux*s0+ox,y+uy*s0+oy,h0,x+ux*s1+ox,y+uy*s1+oy,h1,2.2f,2.2f,BRIDGE_GREY,1);
   if(i)box(x+ux*s0+ox,y+uy*s0+oy,deck,yaw,.5f,.5f,h0-deck,BRIDGE_GREY,BRIDGE_GREY,1);}}
 for(int i=2;i<15;i+=3){float s=-span/2+span*i/16,h=deck+rise*(1-(2*s/span)*(2*s/span));box(x+ux*s,y+uy*s,h-1,yaw,1.2f,26,1.2f,BRIDGE_GREY,BRIDGE_GREY,1);}
}
/* Canal Park lights: North Pier Light, South Breakwater Inner and Outer Lights. */
static void canalLights(float g){
 (void)g;anchorLat=46.7795;anchorLon=-92.0905;float x,y;const RGB LANTERN={40,42,46},RED={170,46,40},BUFF={214,200,170};
 at(46.780935,-92.088255,&x,&y);cyl(x,y,LAKE+2,4.2f,10,WHITE,.62f);cyl(x,y,LAKE+12,2.4f,2.4f,LANTERN,1);cyl(x,y,LAKE+14.4f,2.6f,1.2f,RED,.3f);
 at(46.78007,-92.087705,&x,&y);box(x,y,LAKE+2,0,12,7,5,RED,BUFF,1);box(x+3,y,LAKE+2,0,4.5f,4.5f,13,RED,BUFF,1);cyl(x+3,y,LAKE+15,2.6f,2.5f,LANTERN,1);
 at(46.77871,-92.09202,&x,&y);box(x,y,LAKE+2,0,5,5,19,WHITE,WHITE,.45f);cyl(x,y,LAKE+21,2.2f,2.2f,LANTERN,1);
}
/* Grain elevators (OpenStreetMap silo outlines) and the CN ore docks in the harbour. */
static const float DULUTH_SILOS[][3]={{46.775930f,-92.103383f,33.8f},{46.769153f,-92.113377f,28.1f},{46.769193f,-92.112998f,28.0f},{46.769228f,-92.112643f,23.8f},
 {46.769024f,-92.112636f,19.2f},{46.769132f,-92.112380f,18.8f},{46.765962f,-92.106286f,11.2f},{46.766008f,-92.106149f,10.4f},{46.766058f,-92.105994f,10.8f},
 {46.766119f,-92.105848f,10.7f},{46.766172f,-92.105705f,10.6f},{46.766219f,-92.105555f,10.7f},{46.766276f,-92.105408f,10.8f},{46.766328f,-92.105263f,10.2f},
 {46.766230f,-92.105183f,10.8f},{46.766172f,-92.105338f,9.6f},{46.766124f,-92.105481f,10.5f},{46.766071f,-92.105631f,9.9f},{46.766020f,-92.105772f,10.7f},
 {46.765965f,-92.105922f,10.5f},{46.765912f,-92.106068f,10.1f},{46.765856f,-92.106218f,10.6f}};
static void duluthHarbour(float g){
 (void)g;anchorLat=46.7600;anchorLon=-92.1150;float x,y;const RGB BIN={206,200,186},DOCK={118,100,88},RUST={132,84,60};
 for(int i=0;i<(int)(sizeof(DULUTH_SILOS)/sizeof(DULUTH_SILOS[0]));i++){at(DULUTH_SILOS[i][0],DULUTH_SILOS[i][1],&x,&y);
  float gg=groundAt(x,y);cyl(x,y,gg-2,DULUTH_SILOS[i][2],DULUTH_SILOS[i][2]>20?32:38,BIN,1);}
 /* elevator bin blocks with headhouses */
 at(46.769412,-92.111727,&x,&y);{float gg=groundAt(x,y),yaw=23.7f*DEG;box(x,y,gg-2,yaw,97,28,40,BIN,BIN,1);box(x+cosf(yaw)*40,y+sinf(yaw)*40,gg-2,yaw,16,20,62,BIN,BIN,1);}
 at(46.767618,-92.107931,&x,&y);{float gg=groundAt(x,y),yaw=27.1f*DEG;box(x,y,gg-2,yaw,327,25,38,BIN,BIN,1);
  for(int k=-1;k<=1;k+=2)box(x+cosf(yaw)*k*120,y+sinf(yaw)*k*120,gg-2,yaw,18,24,58,BIN,BIN,1);}
 /* CN ore docks 5 and 6: 755 m concrete docks with the steel pocket superstructure */
 static const float ore[2][5]={{46.748561f,-92.134529f,-46.7f,755,30},{46.750056f,-92.132283f,-46.0f,758,32.7f}};
 for(int i=0;i<2;i++){at(ore[i][0],ore[i][1],&x,&y);float yaw=ore[i][2]*DEG;
  box(x,y,LAKE-3,yaw,ore[i][3],ore[i][4],17,DOCK,DOCK,1);box(x,y,LAKE+14,yaw,ore[i][3]-6,ore[i][4]*.7f,9,RUST,RUST,1);
  for(int k=0;k<24;k++){float s=-ore[i][3]/2+ore[i][3]*(k+.5f)/24;box(x+cosf(yaw)*s,y+sinf(yaw)*s,LAKE+23,yaw,3,ore[i][4]*.9f,4,RUST,RUST,1);}}
}
/* Enger Tower: five-storey octagonal bluestone tower, 24 m. */
static void engerTower(float g){cyl(0,0,g-1,9,24,STONE,.88f);cyl(0,0,g+23,9.4f,1.6f,STONE,1);cyl(0,0,g+24.6f,4,3,(RGB){60,64,70},.2f);}

/* Mission San Xavier del Bac: white church facing south, two bell towers (the east one without
   its dome), crossing dome, convento to the east. */
static void sanXavier(float g){
 const RGB W={238,234,224},TRIM={200,170,120};float cx=-18;
 box(cx,0,g-1,90*DEG,38,12,14,W,W,1);                                         /* nave, north-south */
 box(cx,-19,g-1,0,27,4,16,W,TRIM,1);                                          /* facade */
 for(int s=-1;s<=1;s+=2){box(cx+s*11,-17,g-1,0,7,7,20,W,W,1);box(cx+s*11,-17,g+19,0,5,5,5,W,W,1);}
 cyl(cx-11,-17,g+24,4,1.5f,W,1);sph(cx-11,-17,g+23,4.4f,5.5f,W);cyl(cx-11,-17,g+28,1.2f,2,W,.4f);  /* west tower dome and lantern */
 cyl(cx,6,g+13,10,3,W,1);sph(cx,6,g+10,10,13,W);cyl(cx,6,g+22.5f,1.6f,2.5f,W,.4f);                 /* crossing dome */
 box(18,4,g-1,0,40,48,5.5f,(RGB){214,198,170},(RGB){226,214,190},1);                                 /* convento */
}
/* Sentinel Peak "A": whitewashed rock letter about 49 m tall on the east slope, readable from the city. */
static void sentinelA(float g){
 (void)g;const RGB A={240,240,236};const float bx=170,tx=128,half=14;   /* x east of the peak: letter bottom (east) and apex (west) */
 for(int side=-1;side<=1;side+=2)for(int k=0;k<4;k++){
  float t0=k/4.f,t1=(k+1)/4.f,x0=bx+(tx-bx)*t0,x1=bx+(tx-bx)*t1,y0=side*half*(1-t0),y1=side*half*(1-t1);
  strut(x0,y0,groundAt(x0,y0)+.6f,x1,y1,groundAt(x1,y1)+.6f,1.2f,7,A,1);}
 float xm=bx+(tx-bx)*.4f,ym=half*.6f;strut(xm,-ym,groundAt(xm,-ym)+.6f,xm,ym,groundAt(xm,ym)+.6f,1.2f,6,A,0);
}
/* Arizona Stadium: oval bowl of raked stands around the north-south field, west-side press box. */
static void arizonaStadium(float g){
 const RGB SEAT={150,40,52},CONC={196,190,180};const int N=28;const float ia=78,ib=46,oa=104,ob=110;   /* a: north-south, b: east-west */
 for(int i=0;i<N;i++){float a0=2*3.14159265f*i/N,a1=2*3.14159265f*(i+1)/N,am=(a0+a1)/2;
  float ix=ib*cosf(am),iy=ia*sinf(am),ox=ob*cosf(am),oy=oa*sinf(am);
  float w=hypotf(ob*(cosf(a1)-cosf(a0)),oa*(sinf(a1)-sinf(a0)))*1.08f;
  strut(ix,iy,g+2,ox,oy,g+27,1.5f,w*.85f,SEAT,0);
  box(ox,oy,g-1,atan2f(oy,ox)+1.5707963f,w,2,28,CONC,CONC,1);}
 box(-ob-6,0,g-1,90*DEG,150,16,46,CONC,(RGB){170,176,186},1);                   /* west press box and suites */
}
/* Davis-Monthan AMARG storage: rows of parked aircraft inside the storage areas (OpenStreetMap outlines, ODbL). */
static const double AMARG_A[][2]={{32.15597,-110.84116},{32.14979,-110.84120},{32.14952,-110.84128},{32.14852,-110.84172},{32.14811,-110.84174},{32.14811,-110.84215},{32.14930,-110.84519},{32.14934,-110.84530},{32.15048,-110.84814},{32.15073,-110.84867},{32.15431,-110.85479},{32.15635,-110.85331},{32.15629,-110.84610},{32.15793,-110.84604},{32.15803,-110.84485},{32.15792,-110.84485},{32.15788,-110.84389},{32.15788,-110.84319},{32.15598,-110.84321},{32.15597,-110.84116}};
static const double AMARG_B[][2]={{32.14706,-110.84036},{32.14683,-110.83994},{32.14658,-110.83949},{32.14611,-110.83872},{32.14606,-110.83835},{32.14577,-110.83819},{32.14415,-110.83554},{32.14349,-110.83452},{32.14634,-110.83120},{32.14654,-110.83143},{32.14928,-110.82813},{32.14813,-110.82680},{32.14807,-110.81813},{32.14806,-110.81612},{32.14655,-110.81614},{32.14651,-110.81210},{32.14803,-110.81208},{32.14801,-110.80978},{32.15569,-110.80974},{32.16224,-110.80976},{32.16225,-110.81819},{32.16226,-110.82339},{32.16227,-110.82832},{32.16227,-110.83259},{32.16228,-110.83621},{32.16228,-110.84027},{32.16154,-110.84026},{32.16068,-110.84019},{32.15982,-110.84013},{32.15612,-110.84015},{32.15612,-110.84029},{32.15531,-110.84030},{32.15505,-110.84030},{32.15037,-110.84036},{32.14737,-110.84036},{32.14706,-110.84036}};
static int insideLL(const double (*p)[2],int n,double lat,double lon){int in=0;
 for(int i=0,j=n-1;i<n;j=i++)if((p[i][0]>lat)!=(p[j][0]>lat)&&lon<(p[j][1]-p[i][1])*(lat-p[i][0])/(p[j][0]-p[i][0])+p[i][1])in=!in;return in;}
__attribute__((unused)) /* thousands of boxes: Thor only */
static void boneyard(float g){
 (void)g;anchorLat=32.1600;anchorLon=-110.8400;
 static const float type[5][4]={{16,11,4.5f,0},{16,17.5f,4.5f,0},{30,40,11.6f,1},{41,40,12.7f,1},{48,56,12.4f,1}}; /* length, span, tail, big */
 const RGB PAINT[3]={{230,230,226},{214,214,210},{142,146,152}};
 const double *polys[2]={&AMARG_A[0][0],&AMARG_B[0][0]};const int counts[2]={20,36};
 const double m=6371000*3.14159265358979/180;
 for(int p=0;p<2;p++){const double (*poly)[2]=(const double (*)[2])polys[p];
  double la0=90,la1=-90,lo0=180,lo1=-180;for(int i=0;i<counts[p];i++){la0=fmin(la0,poly[i][0]);la1=fmax(la1,poly[i][0]);lo0=fmin(lo0,poly[i][1]);lo1=fmax(lo1,poly[i][1]);}
  int row=0;for(double lat=la0+30/m;lat<la1-20/m;lat+=58/m,row++){int col=0;
   for(double lon=lo0+25/(m*cos(lat*DEG));lon<lo1-25/(m*cos(lat*DEG));lon+=44/(m*cos(lat*DEG)),col++){
    if(!insideLL(poly,counts[p],lat,lon))continue;unsigned h=(unsigned)(row*7919+col*104729+p*31337);h^=h>>13;h*=2654435761u;h^=h>>16;
    if(h%10<3)continue;                                                          /* gaps */
    int t=(int)((row/3+p)%5);if(type[t][3]>0&&col%2)continue;                     /* big aircraft need two slots */
    const float *T=type[t];float x,y;at(lat,lon,&x,&y);float gg=groundAt(x,y),yaw=(h&16?90:-90)*DEG,cx=cosf(yaw),cy=sinf(yaw);
    RGB c=PAINT[h%3];
    box(x,y,gg+.8f,yaw,T[0],T[0]*.11f,T[0]*.1f,c,c,.8f);                                         /* fuselage */
    box(x+cx*T[0]*.04f,y+cy*T[0]*.04f,gg+.8f+T[0]*.05f,yaw,T[0]*.17f,T[1],.6f,c,c,1);           /* wing */
    box(x-cx*T[0]*.42f,y-cy*T[0]*.42f,gg+.8f+T[0]*.08f,yaw,T[0]*.13f,.5f,T[2]-T[0]*.08f-.8f,c,c,.6f);}}} /* fin */
}
/* Kitt Peak National Observatory: Mayall 4 m and the other domes, the McMath-Pierce solar telescope, a VLBA dish. */
static void kittPeak(float g){
 (void)g;anchorLat=31.9600;anchorLon=-111.6000;float x,y;const RGB D={240,240,238};
 static const float domes[][4]={{31.964030f,-111.600035f,32,36},{31.963075f,-111.600195f,13,13},{31.958065f,-111.600720f,15,12},
  {31.958110f,-111.598350f,14,14},{31.959535f,-111.598490f,9,8},{31.958140f,-111.602215f,8,7},{31.951570f,-111.615890f,10,9},
  {31.951725f,-111.616680f,8,7},{31.961700f,-111.599980f,7,6},{31.962220f,-111.600485f,6,5}};
 for(int i=0;i<(int)(sizeof(domes)/sizeof(domes[0]));i++){at(domes[i][0],domes[i][1],&x,&y);float gg=groundAt(x,y),dd=domes[i][2],hh=domes[i][3];
  cyl(x,y,gg-2,dd,hh+2,D,1);sph(x,y,gg+hh-dd/2,dd,dd,D);}
 at(31.95825,-111.59540,&x,&y);{float gg=groundAt(x,y);box(x,y+28,gg-2,0,9,9,35.5f,D,D,1);strut(x,y+26,gg+30,x,y-36,gg-3,8,8,D,0);}
 at(31.956315,-111.612425,&x,&y);{float gg=groundAt(x,y);box(x,y,gg-1,0,5,5,12,D,D,.8f);prim(3,x,y,gg+9,0,50*DEG,25,25,5,D,D,1);}
}

/* ------------------------------------------------------------ Las Vegas Strip resorts */
/* Outlines and heights from OpenStreetMap (ODbL; the OSM boxes under them are dropped in
   thor_objects.c), shapes modelled by hand. c0.a flags for the object shader: */
#define HOTEL 204 /* night windows, floodlight wash in the wall colour, LED crown */
#define NEON 102  /* lights itself at night; the Sphere shows a picture day and night */
static int markK[3];
static void markStart(void){memcpy(markK,primCount,sizeof(markK));}
static void markAs(unsigned char a){for(int k=0;k<3;k++)for(int i=markK[k];i<primCount[k];i++)prims[k][i].c0[3]=a;}
typedef struct {float end,h;} Step;
/* A tower wing from the hub (x,y) along th, stepping down: {end m from the hub, height}; cap: hip roof height. */
static void wing(float x,float y,float g,float th,float w,const Step *st,int n,RGB wall,RGB roof,float cap){
 float a=0,px,py;
 for(int i=0;i<n;i++){float b=st[i].end,h=st[i].h;axis(th,(a+b)/2,0,&px,&py);
  markStart();box(x+px,y+py,g-2,th,b-a,w,h+2,roof,wall,1);markAs(HOTEL);
  if(cap>0)box(x+px,y+py,g+h,th,b-a,w,cap,roof,roof,.55f);
  a=b;}
}
/* A curved slab along a centreline (local metres from (x,y)). */
static void slab(float x,float y,float g,const float (*c)[2],int n,float w,float h,RGB wall,RGB top){
 markStart();
 for(int i=0;i+1<n;i++){float dx=c[i+1][0]-c[i][0],dy=c[i+1][1]-c[i][1];
  box(x+(c[i][0]+c[i+1][0])/2,y+(c[i][1]+c[i+1][1])/2,g-2,atan2f(dy,dx),hypotf(dx,dy)+w*.35f,w,h+2,top,wall,1);}
 markAs(HOTEL);
}
static void site(double lat,double lon,float *x,float *y,float *g){at(lat,lon,x,y);*g=groundAt(*x,*y);}

static void mandalayBay(void){
 float x,y,g;site(36.09183,-115.17482,&x,&y,&g);const RGB GOLD={204,162,78},ROOF={150,128,96};
 static const Step st[3]={{55,146},{80,128},{101,108}};
 const float th[3]={90*DEG,-28*DEG,208*DEG};for(int i=0;i<3;i++)wing(x,y,g,th[i],20,st,3,GOLD,ROOF,0);
 markStart();cyl(x,y,g-2,30,152,GOLD,.8f);markAs(HOTEL);
}
static void excalibur(void){
 float x,y,g;site(36.10469,-115.17622,&x,&y,&g);x+=20;y+=1;
 const RGB W={236,232,222},ROOF={196,194,188},RED={196,52,44},BLUE={52,84,168},GOLD={214,170,64};
 const Step w1[1]={{103,81}},w2[1]={{90,81}};
 wing(x,y,g,180*DEG,19,w1,1,W,ROOF,0);wing(x,y,g,64*DEG,18,w2,1,W,ROOF,0);wing(x,y,g,-64*DEG,18,w2,1,W,ROOF,0);
 /* the castle front towards the Strip: a wall and turrets with coloured cone roofs */
 box(x+60,y,g-2,90*DEG,70,26,24,W,W,1);
 static const float tur[][3]={{48,30,36},{48,-30,36},{62,15,44},{62,-15,44},{74,0,52},{54,0,40},{78,24,30},{78,-24,30}};
 const RGB cone[3]={RED,BLUE,GOLD};
 for(int i=0;i<8;i++){float h=tur[i][2];cyl(x+tur[i][0],y+tur[i][1],g-2,14,h+2,W,1);cyl(x+tur[i][0],y+tur[i][1],g+h,17,20,cone[i%3],0);}
 for(int i=0;i<3;i++){float a=(i==0?180:i==1?64:-64)*DEG,l=i?90:103,px,py;axis(a,l-9,0,&px,&py);cyl(x+px,y+py,g+79,16,4,W,1);cyl(x+px,y+py,g+83,19,24,cone[i],0);
  axis(a,l*.55f,0,&px,&py);cyl(x+px,y+py,g+79,12,3,W,1);cyl(x+px,y+py,g+82,14,16,cone[(i+1)%3],0);}
}
static void parisLasVegas(void){
 float x,y,g;const RGB CREAM={228,214,186},SLATE={96,108,122},BRONZE={138,96,64},TAN={206,190,160};
 /* casino podium, then the hotel tower: four wings */
 site(36.11222,-115.17110,&x,&y,&g);box(x+38,y+1,g-2,0,300,170,20,TAN,TAN,1);
 site(36.11249,-115.17051,&x,&y,&g);
 static const Step st[2]={{55,112},{80,100}};
 for(int i=0;i<4;i++)wing(x,y,g,(45+90*i)*DEG,20,st,2,CREAM,SLATE,7);
 /* the Eiffel Tower at half scale: legs to the first and second levels, the upper column, the mast */
 site(36.11243,-115.17216,&x,&y,&g);markStart();
 for(int i=0;i<4;i++){float a=90*i*DEG,c=cosf(a),s=sinf(a);
  strut(x+38*c,y+38*s,g-1,x+22*c,y+22*s,g+29,9,9,BRONZE,0);strut(x+20*c,y+20*s,g+33,x+11*c,y+11*s,g+58,6,6,BRONZE,0);}
 box(x,y,g+29,45*DEG,32,32,4,BRONZE,BRONZE,1);box(x,y,g+58,45*DEG,17,17,3,BRONZE,BRONZE,1);
 box(x,y,g+61,45*DEG,13,13,79,BRONZE,BRONZE,.3f);box(x,y,g+140,45*DEG,5,5,8,BRONZE,BRONZE,.8f);cyl(x,y,g+148,1.5f,17,BRONZE,.5f);
 markAs(NEON);
 /* the Montgolfier balloon sign */
 site(36.11180,-115.17195,&x,&y,&g);cyl(x,y,g-1,5,18,CREAM,1);markStart();sph(x,y,g+16,22,26,(RGB){36,72,170});markAs(NEON);
}
static void bellagio(void){
 float x,y,g;site(36.11312,-115.17646,&x,&y,&g);const RGB WALL={226,206,168},TERRA={176,96,64};
 static const Step st[3]={{50,155},{85,138},{118,118}};
 wing(x,y,g,76*DEG,26,st,3,WALL,TERRA,6);wing(x,y,g,-76*DEG,26,st,3,WALL,TERRA,6);wing(x,y,g,180*DEG,26,st,3,WALL,TERRA,6);
}
static void sphere(void){
 float x,y,g;site(36.12124,-115.16210,&x,&y,&g);
 markStart();sph(x+3,y,g+112-157,157,157,(RGB){58,60,68});markAs(NEON);
}
static void venetian(void){
 float x,y,g;const RGB WALL={228,214,186},TILE={176,96,70};
 site(36.12151,-115.16900,&x,&y,&g);x+=5;y+=3;
 static const Step a[2]={{70,145},{110,130}},b[2]={{55,145},{85,130}};
 wing(x,y,g,180*DEG,31,a,2,WALL,TILE,6);wing(x,y,g,45*DEG,30,b,2,WALL,TILE,6);wing(x,y,g,-45*DEG,30,b,2,WALL,TILE,6);
 site(36.12175,-115.17110,&x,&y,&g);box(x,y,g-2,0,11,11,74,(RGB){190,110,80},(RGB){190,110,80},1);
 box(x,y,g+72,0,11,11,13,(RGB){110,150,120},(RGB){110,150,120},0);
 /* the Palazzo */
 const RGB PW={214,194,150},PR={176,150,110};
 site(36.12399,-115.16784,&x,&y,&g);x+=15;
 static const Step n[2]={{50,196},{82,170}},e[1]={{57,180}},sw[2]={{60,190},{94,160}};
 wing(x,y,g,89*DEG,33,n,2,PW,PR,0);wing(x,y,g,-36*DEG,30,e,1,PW,PR,0);wing(x,y,g,217*DEG,26,sw,2,PW,PR,0);
}
static void wynnEncore(void){
 float x,y,g;const RGB BRONZE={176,132,82},TOP={150,116,80};
 static const float wy[][2]={{-1,-100},{12,-60},{13,-20},{10,20},{2,55},{-12,90},{-27,115}};
 static const float en[][2]={{-62,55},{-40,25},{-23,5},{-5,-15},{20,-32},{45,-40},{67,-47}};
 site(36.12658,-115.16555,&x,&y,&g);slab(x,y,g,wy,7,26,187,BRONZE,TOP);
 site(36.12944,-115.16471,&x,&y,&g);slab(x,y,g,en,7,22,192,BRONZE,TOP);
}
static void fontainebleau(void){
 float x,y,g;site(36.13794,-115.15946,&x,&y,&g);
 static const float c[][2]={{-57,50},{-46,10},{-30,-5},{5,-18},{30,-14},{55,5},{89,26}};
 slab(x,y,g,c,7,26,224,(RGB){70,110,160},(RGB){90,110,130});
}
/* The High Roller: 158 m wheel, top at 167 m, 28 cabins, on an A-frame. */
static void highRoller(void){
 float x,y,g;site(36.117624,-115.168221,&x,&y,&g);const RGB W={232,232,236},CAB={180,210,230};
 const float R=79,hz=g+88.5f;
 markStart();
 for(int i=0;i<36;i++){float a=i*10*DEG,b=(i+1)*10*DEG;strut(x,y+R*cosf(a),hz+R*sinf(a),x,y+R*cosf(b),hz+R*sinf(b),2.6f,2.6f,W,.5f);}
 markAs(NEON);
 for(int i=0;i<18;i++){float a=i*20*DEG;strut(x,y,hz,x,y+R*cosf(a),hz+R*sinf(a),.5f,.5f,W,0);}
 strut(x-7,y,hz,x+7,y,hz,7,7,W,0);
 for(int s=-1;s<=1;s+=2){strut(x-20,y+44*s,g-1,x-6,y,hz,4,4,W,0);strut(x+20,y+44*s,g-1,x+6,y,hz,4,4,W,0);}
 markStart();for(int i=0;i<28;i++){float a=i*(360.f/28)*DEG;sph(x,y+(R+5)*cosf(a),hz+(R+5)*sinf(a)-4,8,8,CAB);}markAs(NEON);
}
static void stripResorts(float g){
 (void)g;anchorLat=36.1150;anchorLon=-115.1700;
 mandalayBay();excalibur();parisLasVegas();bellagio();sphere();venetian();wynnEncore();fontainebleau();highRoller();
}

/* ------------------------------------------------------------ second batch (3DS, v1.1p11)
   Positions, outlines and headings from OpenStreetMap (ODbL); sizes from published dimensions. */
static const RGB IVORY={230,226,204},FOAM={118,168,140},SKY_YELLOW={232,196,48},
 SANDSTONE={214,200,170},ROCK={150,132,112},PALE={226,222,210},GRANITE_B={190,176,150},ROOF_GREY={150,150,148},COPPER_G={112,140,126},
 SIGN_WHITE={240,240,240},GREY_B={170,170,166};
/* Suspension span along heading th through (cx,cy): main span 2*half between the towers, side
   spans of 'side' to the anchorages; heights absolute. withDeck 0 when a deck is drawn elsewhere;
   arches: masonry towers with two pointed openings, else steel legs with cross beams. */
static void suspension(float cx,float cy,float th,float half,float side,float deck,float top,float low,float cableV,float towerW,float towerL,
                       RGB tower,RGB cable,RGB road,float water,float step,int withDeck,int arches){
 float x,y;
 if(withDeck){axis(th,0,0,&x,&y);box(cx+x,cy+y,deck-6,th,2*(half+side),2*cableV+4,6,road,cable,1);}
 for(int s=-1;s<=1;s+=2){float ut=half*s;
  axis(th,ut,0,&x,&y);box(cx+x,cy+y,water-8,th,towerL+8,towerW+8,10,ROOF_GREY,ROOF_GREY,1); /* pier */
  float legW=arches?towerW*.32f:towerW*.22f;
  for(int v=-1;v<=1;v+=2){axis(th,ut,v*(towerW-legW)/2,&x,&y);box(cx+x,cy+y,water,th,towerL,legW,top-water,tower,tower,arches?.9f:.85f);}
  axis(th,ut,0,&x,&y);
  if(arches)box(cx+x,cy+y,top-(top-deck)*.45f,th,towerL*.9f,towerW*.9f,(top-deck)*.45f,tower,tower,1); /* crown over the arches */
  else for(float z=deck+20;z<top-5;z+=(top-deck)/3)box(cx+x,cy+y,z,th,towerL*.7f,towerW,5,tower,tower,1);
  box(cx+x,cy+y,deck-12,th,towerL*.8f,towerW,6,tower,tower,1);
  float ua=(half+side)*s;axis(th,ua,0,&x,&y);float ga=groundAt(cx+x,cy+y);if(ga<water)ga=water;
  box(cx+x,cy+y,ga-3,th,36,2*cableV+12,deck+6-ga,GREY_B,GREY_B,1); /* anchorage */
 }
 for(int v=-1;v<=1;v+=2){float cv=v*cableV;
  for(int i=0;i<24;i++){float u0=-half+2*half*i/24,u1=u0+2*half/24,z0=low+(top-low)*(u0/half)*(u0/half),z1=low+(top-low)*(u1/half)*(u1/half),ax,ay,bx,by;
   axis(th,u0,cv,&ax,&ay);axis(th,u1,cv,&bx,&by);strut(cx+ax,cy+ay,z0,cx+bx,cy+by,z1,1.2f,1.2f,cable,.5f);}
  for(int s=-1;s<=1;s+=2)for(int i=0;i<8;i++){float t0=i/8.f,t1=(i+1)/8.f,u0=s*(half+side*t0),u1=s*(half+side*t1);
   float z0=top+(deck+4-top)*t0-.12f*(top-deck)*sinf(3.14159f*t0),z1=top+(deck+4-top)*t1-.12f*(top-deck)*sinf(3.14159f*t1),ax,ay,bx,by;
   axis(th,u0,cv,&ax,&ay);axis(th,u1,cv,&bx,&by);strut(cx+ax,cy+ay,z0,cx+bx,cy+by,z1,1.2f,1.2f,cable,.5f);}
  for(float u=-half+step;u<half;u+=step){float z=low+(top-low)*(u/half)*(u/half);if(z-deck<2)continue;axis(th,u,cv,&x,&y);box(cx+x,cy+y,deck,th,.4f,.4f,z-deck,cable,cable,1);}
 }
}
/* Brooklyn Bridge (1883): 486 m main span, 283 m side spans, granite towers 84 m with two
   pointed arches, deck about 41 m over the East River; centre and heading from the OSM way of
   the river spans (1,053 m). */
static void brooklynBridge(float g){
 const float th=-46.38f*DEG,water=g<2?g:0,deck=water+42;float x,y;
 suspension(0,0,th,243,283,deck,water+84,deck+3,12,42,18,GRANITE_B,(RGB){196,190,170},ROAD,water,10,1,1);
 for(int s=-1;s<=1;s+=2)for(int k=0;k<6;k++){float um=s*(526+45.f*(k+.5f));axis(th,um,0,&x,&y);
  float gm=fmaxf(groundAt(x,y),water),h=deck+(gm+6-deck)*(k+.5f)/6;box(x,y,gm-2,th,46,26,h-gm+2,ROAD,GRANITE_B,1);}
}
/* Mackinac Bridge (1957): 1,158 m main span, 549 m side spans, towers 168 m over the Straits,
   deck about 60 m up over the suspension part; ivory towers, green cables and truss. Deck along
   the OSM way of the bridge (4.2 km), suspension part centred on it. */
static const double MACKINAC[][2]={{45.7990974,-84.7304433},{45.8037593,-84.7297501},{45.8101355,-84.7287448},{45.8206265,-84.7271736},{45.8269717,-84.7262660},{45.8288523,-84.7259146},{45.8369196,-84.7248113}};
#define STRAITS 176.5f
static float mackinacProfile(float s,float total){float d=fabsf(s-total/2);return d<1128?STRAITS+60:STRAITS+60-(d-1128)*(35.f/(total/2-1128));}
static void mackinacBridge(float g){
 (void)g;float x,y,yaw,total=0,ax,ay,bx,by;const int n=7;
 for(int i=1;i<n;i++){at(MACKINAC[i-1][0],MACKINAC[i-1][1],&ax,&ay);at(MACKINAC[i][0],MACKINAC[i][1],&bx,&by);total+=hypotf(bx-ax,by-ay);}
 deckAlong(MACKINAC,n,22,mackinacProfile,ROAD,FOAM);
 alongPoint(MACKINAC,n,total/2,&x,&y,&yaw);
 suspension(x,y,yaw,579,549,STRAITS+60,STRAITS+168,STRAITS+64,10.5f,24,12,IVORY,FOAM,ROAD,STRAITS,15,0,0);
}
/* Sunshine Skyway Bridge (1987): cable-stayed, 366 m main span, single pylons 131 m over Tampa
   Bay with yellow stays in one central plane, deck 59 m clear; deck along the OSM way of the
   high-level bridge (6.7 km). */
static const double SKYWAY[][2]={{27.6405774,-82.6701936},{27.6399187,-82.6697623},{27.6389606,-82.6691094},{27.6380295,-82.6684578},{27.6370777,-82.6677836},{27.6361515,-82.6671051},{27.6352203,-82.6664073},{27.6342933,-82.6657535},{27.6333764,-82.6650855},{27.6324386,-82.6644038},{27.6315213,-82.6637260},{27.6306054,-82.6630771},{27.6293594,-82.6621954},{27.6281275,-82.6612836},{27.6257423,-82.6595601},{27.6250032,-82.6590305},{27.6232649,-82.6577501},{27.6216795,-82.6565807},{27.6189865,-82.6546578},{27.6171872,-82.6533528},{27.6154384,-82.6520415},{27.6139003,-82.6509224},{27.6127052,-82.6500630},{27.6121547,-82.6496651},{27.6114456,-82.6491554},{27.6101355,-82.6481966},{27.6092103,-82.6475255},{27.6082685,-82.6468279},{27.6073532,-82.6461318},{27.6064410,-82.6454417},{27.6055328,-82.6446892},{27.6046503,-82.6439297},{27.6037754,-82.6431445},{27.6029258,-82.6423978},{27.6020742,-82.6416009},{27.6012357,-82.6407625},{27.6004187,-82.6399366},{27.5995891,-82.6390765},{27.5988069,-82.6382506},{27.5980302,-82.6373866},{27.5972487,-82.6364951},{27.5964951,-82.6355871},{27.5957492,-82.6346695},{27.5950121,-82.6337190},{27.5943080,-82.6327656},{27.5936056,-82.6317916},{27.5929190,-82.6308058},{27.5924258,-82.6300725}};
static float skywayProfile(float s,float total){float d=fabsf(s-total/2);return d<366?62:62-(d-366)*(52.f/(total/2-366));}
static void sunshineSkyway(float g){
 (void)g;const int n=48;float total=0,ax,ay,bx,by,x,y,yaw;
 for(int i=1;i<n;i++){at(SKYWAY[i-1][0],SKYWAY[i-1][1],&ax,&ay);at(SKYWAY[i][0],SKYWAY[i][1],&bx,&by);total+=hypotf(bx-ax,by-ay);}
 deckAlong(SKYWAY,n,29,skywayProfile,ROAD,CONCRETE);
 for(int s=-1;s<=1;s+=2){alongPoint(SKYWAY,n,total/2+183*s,&x,&y,&yaw);
  box(x,y,-6,yaw,30,36,9,CONCRETE,CONCRETE,1);                       /* pier and fender */
  box(x,y,0,yaw,9,6,131,CONCRETE,CONCRETE,.55f);                     /* pylon in the median */
  for(int side=-1;side<=1;side+=2)for(int k=0;k<21;k++){float d=18+8.f*k,zt=72+2.8f*k;
   strut(x,y,zt,x+cosf(yaw)*side*d,y+sinf(yaw)*side*d,62,.6f,.6f,SKY_YELLOW,0);}}
}
/* Thomas Jefferson Memorial (1943): round colonnade of 26 Ionic columns, dome to 39 m, portico
   facing the Tidal Basin (north). */
static void jeffersonMemorial(float g){
 const float R=24.5f;
 cyl(0,0,g-2,84,5,MARBLE,1);cyl(0,0,g+3,58,3,MARBLE,1);
 for(int i=0;i<26;i++){float a=i*(360.f/26)*DEG;cyl(R*cosf(a),R*sinf(a),g+6,1.8f,12.7f,MARBLE,1);}
 cyl(0,0,g+6,40,15,MARBLE,1);cyl(0,0,g+18.7f,54,3.5f,MARBLE,1);cyl(0,0,g+22,44,4,MARBLE,1);sph(0,0,g+13,46,26,MARBLE);
 box(0,30,g+3,0,30,16,19,MARBLE,MARBLE,1);box(0,30,g+22,0,30,16,5,MARBLE,MARBLE,.3f);
}
/* The Pentagon (1943): five rings, outer sides 281 m, 23 m high; centre and corners from the
   OSM outline (a corner at -153.3 degrees from east). */
static void pentagon(float g){
 const RGB WALL={204,198,182};
 for(int k=0;k<5;k++){float nrm=(-153.3f+36+72*k)*DEG;
  for(int r=0;r<4;r++){float ap=179-30.f*r,len=2*ap*tanf(36*DEG)+12;
   box(ap*cosf(nrm),ap*sinf(nrm),g-2,nrm+90*DEG,len,26,25,ROOF_GREY,WALL,1);}}
}
/* The White House: residence 51 x 26 m, 21 m to the balustrade; north portico, rounded south
   portico, West and East Wings with their colonnades. */
static void whiteHouse(float g){
 const RGB W={238,236,230};
 box(0,0,g-2,0,51,26,23,ROOF_GREY,W,1);box(0,0,g+21,0,46,20,3,ROOF_GREY,ROOF_GREY,.7f);
 box(0,16,g-2,0,30,8,19,W,W,1);box(0,16,g+17,0,30,8,3,W,W,.4f);
 cyl(0,-15,g-2,17,19,W,1);
 box(-118,-6,g-2,0,50,32,11,ROOF_GREY,W,1);box(-55,0,g-2,0,72,9,7,W,W,1);
 box(112,-2,g-2,0,52,26,10,ROOF_GREY,W,1);box(50,0,g-2,0,70,9,7,W,W,1);
}
/* Alcatraz: the cellhouse (65 x 49 m outline from OSM) on the island crest, the 26 m lighthouse
   and the barracks; the island itself (541 x 205 m) as rock terraces when the elevation grid
   does not show it. */
static void alcatraz(float g){
 (void)g;float x,y,gi;const RGB CELL={214,206,188};
 site(37.826810,-122.423081,&x,&y,&gi);float base=gi;
 if(base<30){box(x,y,-4,-33.4f*DEG,520,190,18,ROCK,ROCK,.85f);box(x,y,14,-33.4f*DEG,380,130,16,ROCK,ROCK,.8f);
  box(x,y,30,-33.4f*DEG,200,70,8,ROCK,ROCK,.85f);base=38;}
 site(37.826629,-122.422931,&x,&y,&gi);box(x,y,base-2,134.1f*DEG,65,49,15,ROOF_GREY,CELL,1);
 site(37.826254,-122.422296,&x,&y,&gi);cyl(x,y,base,5,26,PALE,.7f);
 site(37.826707,-122.421705,&x,&y,&gi);box(x,y,base-14,40*DEG,60,26,16,ROOF_GREY,CELL,1);
}
/* Coit Tower (1933): fluted concrete column 64 m on Telegraph Hill, 11.3 m across. */
static void coitTower(float g){cyl(0,0,g-1,26,6,PALE,1);cyl(0,0,g,11.3f,58,PALE,1);cyl(0,0,g+58,12.4f,6,PALE,1);}
/* Hollywood Sign: nine white letters 13.7 m high along about 105 m of Mount Lee, facing south. */
static void letterBar(float u,float v0,float v1,float w){
 float x=u,y=0;box(x,y,groundAt(x,y)-1+v0,0,w,.9f,v1-v0+1,SIGN_WHITE,SIGN_WHITE,1);
}
static void letterStroke(float u0,float v0,float u1,float v1,float b){
 float g0=groundAt(u0,0),g1=groundAt(u1,0);strut(u0,0,g0+v0,u1,0,g1+v1,b,.9f,SIGN_WHITE,0);
}
static void hollywoodSign(float g){
 (void)g;const char *word="HOLLYWOOD";static const float wd[9]={10,10,9,9,11,12,10,10,10};float u=-52;const float b=2.4f,H=13.7f;
 for(int i=0;i<9;i++){float w=wd[i],c=u+w/2;char ch=word[i];
  if(ch=='H'){letterBar(u+b/2,0,H,b);letterBar(u+w-b/2,0,H,b);letterBar(c,5.6f,8.1f,w);}
  else if(ch=='O'){letterBar(u+b/2,0,H,b);letterBar(u+w-b/2,0,H,b);letterBar(c,0,b,w);letterBar(c,H-b,H,w);}
  else if(ch=='L'){letterBar(u+b/2,0,H,b);letterBar(c,0,b,w);}
  else if(ch=='Y'){letterBar(c,0,7,b);letterStroke(c,7,u+1,H,b);letterStroke(c,7,u+w-1,H,b);}
  else if(ch=='W'){static const float pu[5]={0,.25f,.5f,.75f,1},pv[5]={13.7f,0,9,0,13.7f};
   for(int k=0;k<4;k++)letterStroke(u+w*pu[k],pv[k],u+w*pu[k+1],pv[k+1],b);}
  else if(ch=='D'){letterBar(u+b/2,0,H,b);letterBar(u+w*.35f,0,b,w*.7f);letterBar(u+w*.35f,H-b,H,w*.7f);letterBar(u+w-b/2,2.2f,H-2.2f,b);}
  u+=w+1.6f;}
}
/* Griffith Observatory (1935): white building with the planetarium dome (25 m) in the middle
   and a telescope dome at each end, on the slope of Mount Hollywood, facing south. */
static void griffith(float g){
 const RGB W={232,228,214};
 box(0,0,g-2,0,46,30,14,ROOF_GREY,W,1);box(0,-4,g-2,0,72,14,9,ROOF_GREY,W,1);
 cyl(0,4,g+12,27,5,W,1);sph(0,4,g+12,26,22,COPPER_G);
 for(int s=-1;s<=1;s+=2){box(30.f*s,-4,g-2,0,12,12,15,W,W,1);sph(30.f*s,-4,g+12,11,10,COPPER_G);}
}
/* Vehicle Assembly Building (1966): 218 x 158 m, 160 m high (outline 221 x 157 m from OSM);
   low bay on the south side, high-bay doors. */
static void vab(float g){
 const float th=-76.6f*DEG;const RGB W={204,204,198},DOOR={150,150,150};float x,y;
 box(0,0,g-2,th,160,157,162,ROOF_GREY,W,1);
 axis(th,-95,0,&x,&y);box(x,y,g-2,th,60,140,66,ROOF_GREY,W,1);
 for(int s=-1;s<=1;s+=2)for(int k=-1;k<=1;k+=2){axis(th,40.f*k,s*78.6f,&x,&y);box(x,y,g,th,24,1.5f,140,DOOR,DOOR,1);}
}
/* Launch pads 39A and 39B: the 15 m pad mound; 39A's service tower, 39B's three lightning towers. */
static void padMound(float g){box(0,0,g-2,0,210,210,17,CONCRETE,(RGB){176,170,150},.6f);box(0,0,g+15,0,70,70,1,CONCRETE,CONCRETE,1);}
static void pad39A(float g){padMound(g);box(0,40,g+15,0,14,14,87,(RGB){120,120,124},(RGB){120,120,124},1);cyl(0,40,g+102,3,58,PALE,.3f);
 box(-30,-10,g+15,0,30,10,12,PALE,PALE,1);}
static void pad39B(float g){padMound(g);for(int i=0;i<3;i++){float a=(90+120*i)*DEG;cyl(150*cosf(a),150*sinf(a),g,7,170,PALE,.35f);cyl(150*cosf(a),150*sinf(a),g+170,1.5f,15,PALE,.4f);}}
/* Devils Tower: a 265 m column of igneous rock, about 300 m across at the base and 90 m at the
   summit (1,558 m), over a talus apron. The 100 m elevation grid smooths it away. */
static void devilsTower(float g){
 float base=g;for(int i=0;i<12;i++){float a=i*30*DEG,h=groundAt(200*cosf(a),200*sinf(a));if(h<base)base=h;}
 cyl(0,0,base-5,430,45,ROCK,.62f);cyl(0,0,base+20,275,1558-base-20,(RGB){156,138,118},.33f);
}
/* San Jacinto Monument (1939): 173.7 m, an octagonal shaft on a museum base, with a 10 m star. */
static void sanJacinto(float g){
 const float th=65.05f*DEG;
 box(0,0,g-2,th,38,38,14,SANDSTONE,SANDSTONE,1);cyl(0,0,g+12,14,150,SANDSTONE,.68f);
 box(0,0,g+162,th,9.5f,9.5f,1.5f,SANDSTONE,SANDSTONE,1);sph(0,0,g+163.5f,8,10.2f,(RGB){200,200,196});
}

typedef struct {const char *name;double lat,lon;void (*build)(float ground);double radius;int built,fine;} Landmark; /* radius 0: DRAW_RADIUS */
static Landmark landmarks[]={
 {"Golden Gate Bridge",37.819777,-122.478551,goldenGate},
 {"Hoover Dam",36.015930,-114.737789,hooverDam},
 {"Hoover Dam Bypass bridge",36.012744,-114.741840,tillmanBridge},
 {"Gateway Arch",38.624611,-90.184971,gatewayArch},
 {"Washington Monument",38.889475,-77.035242,washingtonMonument},
 {"US Capitol",38.889811,-77.008940,capitol},
 {"Lincoln Memorial",38.889272,-77.050172,lincolnMemorial},
 {"Empire State Building",40.748442,-73.985659,empireState},
 {"Willis Tower",41.878735,-87.635962,willisTower},
 {"Luxor",36.095471,-115.175804,luxor},
 {"The STRAT",36.147409,-115.155521,stratTower},
 {"Las Vegas Strip resorts",36.1150,-115.1700,stripResorts},
 {"Space Needle",47.620506,-122.349303,spaceNeedle},
 {"Statue of Liberty",40.689243,-74.044542,statueOfLiberty},
 {"Aerial Lift Bridge",46.778955,-92.092870,aerialLiftBridge},
 {"Blatnik Bridge",46.74922,-92.10098,blatnikBridge},
 {"Bong Bridge",46.73158,-92.14334,bongBridge},
 {"Canal Park lights",46.7795,-92.0905,canalLights},
 {"Duluth elevators and ore docks",46.7600,-92.1150,duluthHarbour},
 {"Enger Tower",46.776111,-92.124955,engerTower},
 {"Mission San Xavier del Bac",32.107284,-111.007835,sanXavier},
 {"Sentinel Peak A",32.21025,-110.99234,sentinelA},
 {"Arizona Stadium",32.228857,-110.948779,arizonaStadium},
 {"Kitt Peak",31.9600,-111.6000,kittPeak,130000},
 /* second batch (3DS) */
 {"Brooklyn Bridge",40.705647,-73.996419,brooklynBridge},
 {"Mackinac Bridge",45.818005,-84.727566,mackinacBridge},
 {"Sunshine Skyway Bridge",27.615227,-82.651888,sunshineSkyway},
 {"Jefferson Memorial",38.881369,-77.036571,jeffersonMemorial},
 {"The Pentagon",38.871019,-77.055947,pentagon},
 {"White House",38.897642,-77.036540,whiteHouse},
 {"Alcatraz",37.826810,-122.423081,alcatraz},
 {"Coit Tower",37.802373,-122.405834,coitTower},
 {"Hollywood Sign",34.1340628,-118.3216464,hollywoodSign},
 {"Griffith Observatory",34.118213,-118.300372,griffith},
 {"Vehicle Assembly Building",28.585747,-80.650913,vab},
 {"Launch Complex 39A",28.608266,-80.604161,pad39A},
 {"Launch Complex 39B",28.627176,-80.620856,pad39B},
 {"Devils Tower",44.590534,-104.715096,devilsTower,40000},
 {"San Jacinto Monument",29.749856,-95.080712,sanJacinto},
};
#define LANDMARKS ((int)(sizeof(landmarks)/sizeof(landmarks[0])))

int landmarkCount(void){return LANDMARKS;}
const char *landmarkName(int i){return landmarks[i].name;}
void landmarkAnchor(int i,float *east,float *north,float *stretch){
 const double m=6371000*3.14159265358979/180;const Landmark *l=&landmarks[i];
 *north=(float)((l->lat-FRAME_LAT)*m);*east=(float)((l->lon-FRAME_LON)*m*cos(FRAME_LAT*DEG));*stretch=(float)(cos(FRAME_LAT*DEG)/cos(l->lat*DEG));
}
float landmarkRadius(int i){return landmarks[i].radius>0?(float)landmarks[i].radius:0;}

/* ---------------------------------------------------------------- primitives to triangles
   Unit meshes as the Thor renderer's (android/shim/objects.c): box x along the length,
   centred, z 0..1, walls then roof; cylinder and sphere fewer sides for the 3DS. Every
   triangle is anticlockwise seen from outside. Part 1 (roofs, caps, spheres) takes the
   top colour, part 0 the side colour. Lighting is baked from a fixed sun, as for the
   building tiles (tools/build_3ds_buildings.py). */
#define CYL_SIDES 12
#define SPH_SIDES 10
#define SPH_RINGS 6
static const float SUN[3]={-0.45f,-0.55f,0.70f};
typedef struct {LandmarkVertex *v;int n,cap;float stretch,z0;} Out;
static void emit(Out *o,const CfObjectInstance *p,const float q[3][3],int part){
 if(o->n+3>o->cap)return;
 float w[3][3];
 for(int k=0;k<3;k++){float x=q[k][0],y=q[k][1],z=q[k][2];
  if(z>.99f&&p->c1[3]<255){float t=p->c1[3]/100.f;x*=t;y*=t;} /* top taper */
  x*=p->i1[0];y*=p->i1[1];z*=p->i1[2];
  if(p->i1[3]!=0){float ct=cosf(p->i1[3]),st=sinf(p->i1[3]),nx=x*ct+z*st;z=-x*st+z*ct;x=nx;}
  float c=cosf(p->i0[3]),s=sinf(p->i0[3]),rx=x*c-y*s,ry=x*s+y*c;
  w[k][0]=p->i0[0]+rx;w[k][1]=p->i0[1]+ry;w[k][2]=p->i0[2]+z;}
 float ax=w[1][0]-w[0][0],ay=w[1][1]-w[0][1],az=w[1][2]-w[0][2],bx=w[2][0]-w[0][0],by=w[2][1]-w[0][1],bz=w[2][2]-w[0][2];
 float nx=ay*bz-az*by,ny=az*bx-ax*bz,nz=ax*by-ay*bx,l=sqrtf(nx*nx+ny*ny+nz*nz);
 if(l<1e-6f)return; /* degenerate (a tapered point) */
 float sl=sqrtf(SUN[0]*SUN[0]+SUN[1]*SUN[1]+SUN[2]*SUN[2]),d=(nx*SUN[0]+ny*SUN[1]+nz*SUN[2])/(l*sl),k=.58f+.42f*(d>0?d:0);
 const unsigned char *c=part?p->c0:p->c1;
 for(int j=0;j<3;j++){LandmarkVertex *v=&o->v[o->n++];
  v->x=w[j][0]*o->stretch;v->y=w[j][1];v->z=w[j][2]-o->z0;
  v->r=(unsigned char)fminf(c[0]*k,255);v->g=(unsigned char)fminf(c[1]*k,255);v->b=(unsigned char)fminf(c[2]*k,255);}
}
static void quad(Out *o,const CfObjectInstance *p,const float *a,const float *b,const float *c,const float *d,int part){
 float t1[3][3]={{a[0],a[1],a[2]},{b[0],b[1],b[2]},{c[0],c[1],c[2]}},t2[3][3]={{a[0],a[1],a[2]},{c[0],c[1],c[2]},{d[0],d[1],d[2]}};
 emit(o,p,t1,part);emit(o,p,t2,part);
}
static void expandBox(Out *o,const CfObjectInstance *p){
 static const float c[8][3]={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0},{-.5f,-.5f,1},{.5f,-.5f,1},{.5f,.5f,1},{-.5f,.5f,1}};
 quad(o,p,c[0],c[1],c[5],c[4],0);quad(o,p,c[1],c[2],c[6],c[5],0);quad(o,p,c[2],c[3],c[7],c[6],0);quad(o,p,c[3],c[0],c[4],c[7],0);
 quad(o,p,c[4],c[5],c[6],c[7],1);
}
static void expandCylinder(Out *o,const CfObjectInstance *p){
 for(int i=0;i<CYL_SIDES;i++){float a=i*6.2831853f/CYL_SIDES,b=(i+1)*6.2831853f/CYL_SIDES;
  float p0[3]={.5f*cosf(a),.5f*sinf(a),0},p1[3]={.5f*cosf(b),.5f*sinf(b),0},p2[3]={.5f*cosf(b),.5f*sinf(b),1},p3[3]={.5f*cosf(a),.5f*sinf(a),1};
  quad(o,p,p0,p1,p2,p3,0);
  float cap[3][3]={{0,0,1},{p3[0],p3[1],1},{p2[0],p2[1],1}};emit(o,p,cap,1);}
}
static void expandSphere(Out *o,const CfObjectInstance *p){
 for(int j=0;j<SPH_RINGS;j++)for(int i=0;i<SPH_SIDES;i++){
  float t0=3.14159265f*j/SPH_RINGS-1.5707963f,t1=3.14159265f*(j+1)/SPH_RINGS-1.5707963f,a=i*6.2831853f/SPH_SIDES,b=(i+1)*6.2831853f/SPH_SIDES;
  const float tt[4]={t0,t0,t1,t1},aa[4]={a,b,b,a};float q[4][3];
  for(int c=0;c<4;c++){q[c][0]=.5f*cosf(tt[c])*cosf(aa[c]);q[c][1]=.5f*cosf(tt[c])*sinf(aa[c]);q[c][2]=.5f+.5f*sinf(tt[c]);}
  quad(o,p,q[0],q[1],q[2],q[3],1);}
}
/* Thin parts (both cross dimensions under DETAIL_M: cables, suspenders, struts, lamp posts)
   go last, so far away the renderer can draw just the leading, larger parts. */
#define DETAIL_M 3.0f
static int isDetail(const CfObjectInstance *p){return p->i1[0]<DETAIL_M&&p->i1[1]<DETAIL_M;}
int landmarkTriangles(int i,float ground,LandmarkVertex **out,float *baseZ,int *farCount){
 Landmark *l=&landmarks[i];float e,n,stretch;landmarkAnchor(i,&e,&n,&stretch);
 anchorE=e;anchorN=n;anchorStretch=stretch;anchorLat=l->lat;anchorLon=l->lon;
 memset(primCount,0,sizeof(primCount));l->build(ground);
 int tris=primCount[0]*10+primCount[1]*CYL_SIDES*3+primCount[2]*SPH_SIDES*SPH_RINGS*2;
 Out o={malloc((size_t)tris*3*sizeof(LandmarkVertex)),0,tris*3,stretch,floorf(ground/64)*64};
 if(!o.v){*out=NULL;return 0;}
 for(int pass=0;pass<2;pass++){
  for(int k=0;k<primCount[0];k++)if(isDetail(&prims[0][k])==pass)expandBox(&o,&prims[0][k]);
  for(int k=0;k<primCount[1];k++)if(prims[1][k].c0[3]&&isDetail(&prims[1][k])==pass)expandCylinder(&o,&prims[1][k]); /* c0.a 0: night-only beam, not drawn here */
  for(int k=0;k<primCount[2];k++)if(isDetail(&prims[2][k])==pass)expandSphere(&o,&prims[2][k]);
  if(!pass)*farCount=o.n;
 }
 *out=o.v;*baseZ=o.z0;return o.n;
}
