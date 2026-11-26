#include "scenery.h"
#include "terrain.h"
#include "terrain_stream.h"
#include "airport_index.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define PI 3.14159265359f
#define SCENERY_NEAR_RUNWAYS 256
int sceneryRunwayStats[4][2];
/* v35.13: ground colour for the whole grid on the 4 km US base lattice (tools/build_west_v35.py colormap):
   the mean photo colour, open water where the elevation is exactly level (hydro-flattened lakes, sea),
   gaps filled from neighbours. Used beyond the photo tiles instead of the Arizona desert palette.
   v35.14: read from the card, not embedded - embedding it took the program image past 32 MB and
   v35.13 stopped at its first instruction on hardware. Missing file: the Arizona palette as before. */
#define GROUND_SPACING 4000.f
#define GROUND_W ((int)(ELEV_TX*ELEV_TILE_METRES/4000)+1)
#define GROUND_H ((int)(ELEV_TY*ELEV_TILE_METRES/4000)+1)
#define GROUND_PATH "sdmc:/3ds/CanyonFlight/scenery/ground_us4000.bin"
static unsigned char *GROUND_MAP;static int groundTried;
static int groundMapLoad(void){
 if(groundTried)return GROUND_MAP!=0;
 groundTried=1;size_t bytes=(size_t)GROUND_W*GROUND_H*3;
 FILE *f=fopen(GROUND_PATH,"rb");if(!f)return 0;
 unsigned char *m=malloc(bytes);
 if(m&&fread(m,1,bytes,f)==bytes&&fgetc(f)==EOF)GROUND_MAP=m;else free(m);
 fclose(f);return GROUND_MAP!=0;
}
int sceneryGroundMapLoaded(void){return groundMapLoad();}
static int groundMapColor(float e,float n,float rgb[3]){
 if(!groundMapLoad())return 0;
 float fx=(e-ELEV_E0)/GROUND_SPACING,fy=(n-ELEV_N0)/GROUND_SPACING;
 if(!(fx>=0&&fy>=0&&fx<GROUND_W-1&&fy<GROUND_H-1))return 0;
 int x=(int)fx,y=(int)fy;float tx=fx-x,ty=fy-y;
 const unsigned char *a=GROUND_MAP+(y*GROUND_W+x)*3,*b=a+3,*c=a+GROUND_W*3,*d=c+3;
 for(int i=0;i<3;i++)rgb[i]=((a[i]*(1-tx)+b[i]*tx)*(1-ty)+(c[i]*(1-tx)+d[i]*tx)*ty)/255.f;
 return 1;
}
int sceneryGroundColor(float east,float north,float rgb[3]){return groundMapColor(east,north,rgb);}
static WorldVertex *output;static int used,limit;static Vec viewer;
static int surfaceMaterial=-1;
static signed char vertexMaterial[SCENERY_PART_CAPACITY];
float sceneryHaze(float distance){
 /* Clear nearby terrain; progressively soften distant ridges without a
    hard far-distance cutoff. Division avoids per-vertex exponentials. */
 /* Arizona's dry air keeps distant rims crisp; haze begins at 12 km. */
 float d=fmaxf(distance-12000,0);return fminf(d/(d+90000),.95f);
}
void scenerySkyColor(float elevationSine,float rgb[3]){
 float t=clampf(elevationSine*1.7f,0,1);
 rgb[0]=(125-66*t)/255;rgb[1]=(177-60*t)/255;rgb[2]=(207-31*t)/255;
}
int sceneryVertexMaterial(int index){return index>=0&&index<SCENERY_PART_CAPACITY?vertexMaterial[index]:-1;}
static void triangle(Vec a,Vec b,Vec c,float r,float g,float blue){
 if(used+3>limit)return;
 Vec p[3]={a,b,c};for(int i=0;i<3;i++){
  float d=hypotf(p[i].x-viewer.x,p[i].y-viewer.y),fog=sceneryHaze(d);
  if(used<SCENERY_PART_CAPACITY)vertexMaterial[used]=surfaceMaterial;
  output[used++]=(WorldVertex){p[i],{(r*(1-fog)+125*fog)/255,(g*(1-fog)+177*fog)/255,(blue*(1-fog)+207*fog)/255}};
 }
}
static void quad(Vec a,Vec b,Vec c,Vec d,float r,float g,float blue){triangle(a,b,c,r,g,blue);triangle(a,c,d,r,g,blue);}
static Vec point(const Airport *ap,float along,float across,float z){
 float angle=ap->heading*PI/180,e=ap->east+along*sinf(angle)+across*cosf(angle),n=ap->north+along*cosf(angle)-across*sinf(angle);
 /* Secondary runways follow the flattened surface: where they cross the
    primary's core, terrain() is the primary's profile, so paint, pavement
    and wheel contact agree (v31). Primaries keep their exact profile. */
 return (Vec){e,n,(ap->primary?runwayElevation(ap,along):terrain(e,n))+z};
}
static int coarsePavement; /* v1.1p20: far runways, 400 m strips */
static void pavement(const Airport *ap,float a,float b,float left,float right,float r,float g,float blue,float bias){
 int asphalt=r<135&&fabsf(r-g)<12; /* v35.06: asphalt greys now reach 124 */surfaceMaterial=asphalt?-2:-1;
 /* Bounded, broad age variation complements the repeating aggregate grain. */
 int strips=asphalt?(int)ceilf((b-a)/(coarsePavement?400:40)):1;
 for(int i=0;i<strips;i++){
  float start=a+(b-a)*i/strips,end=a+(b-a)*(i+1)/strips;
  float shade=asphalt?1+.035f*sinf(start*.071f+left*.13f):1;
  quad(point(ap,start,left,bias),point(ap,end,left,bias),point(ap,end,right,bias),point(ap,start,right,bias),r*shade,g*shade,blue*shade);
 }
 surfaceMaterial=-1;
}
static void building(const Airport *ap,float along,float across,float length,float width,float height){
 Vec a=point(ap,along-length/2,across-width/2,1),b=point(ap,along+length/2,across-width/2,1),c=point(ap,along+length/2,across+width/2,1),d=point(ap,along-length/2,across+width/2,1);
 Vec aa=a,bb=b,cc=c,dd=d;aa.z+=height;bb.z+=height;cc.z+=height;dd.z+=height;
 quad(a,b,bb,aa,173,164,145);quad(b,c,cc,bb,155,145,129);quad(c,d,dd,cc,184,177,158);quad(d,a,aa,dd,148,143,132);quad(aa,bb,cc,dd,111,113,109);
 /* A dark hangar front, facing the runway side, and an inset roof panel. */
 float front=across+(across<0?1:-1)*(width/2+.1f);
 Vec p=point(ap,along-length*.36f,front,1),q=point(ap,along+length*.36f,front,1),r=p,t=q;r.z+=height*.75f;t.z+=height*.75f;quad(p,q,t,r,63,70,73);
}
static void digit(const Airport *ap,int value,float along,float across,int reverse){
 const unsigned rows[10][5]={{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
                             {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};
 int idx=value>=0&&value<=9?value:0;
 for(int y=0;y<5;y++)for(int x=0;x<3;x++)if(rows[idx][y]&(1<<(2-x))){
  float aa=along+(reverse?-1:1)*(4-y)*3.1f,bb=aa+(reverse?-1:1)*2.5f,side=across+(reverse?-1:1)*x*2.5f;
  pavement(ap,fminf(aa,bb),fmaxf(aa,bb),side,side+2.0f,236,236,220,.075f);
 }
}
static void airport(const Airport *ap,int detailed){
 float half=ap->length/2,w=ap->width/2;
 /* v1.1p20: far runways (15 km+, detailed < 0) are only the asphalt, in 400 m strips. */
 if(detailed<0){coarsePavement=1;pavement(ap,-half,half,-w,w,112,114,116,.025f);coarsePavement=0;return;}
 /* Shoulder bands must not bridge underneath the sloped runway. A single
    end-to-end quad interpolated clamped endpoint heights over a longer span,
    lifting its low half through the asphalt. */
 pavement(ap,-half,half,-w-5,-w,150,133,105,.01f);
 pavement(ap,-half,half,w,w+5,150,133,105,.01f);
 pavement(ap,-half-8,-half,-w-5,w+5,150,133,105,.01f);
 pavement(ap,half,half+8,-w-5,w+5,150,133,105,.01f);
 pavement(ap,-half,half,-w,w,112,114,116,.025f); /* v35.06: aged asphalt, about RGB 100 on screen (was 43, near black) */
 pavement(ap,-half+35,half-35,-w+1,-w+1.35f,225,225,212,.045f);pavement(ap,-half+35,half-35,w-1.35f,w-1,225,225,212,.045f);
 for(float t=-half+150;t<half-120;t+=52)pavement(ap,t,t+25,-.45f,.45f,235,235,222,.06f);
 for(int end=-1;end<=1;end+=2){
  float start=end<0?-half+15:half-37;
  for(int stripe=0;stripe<8;stripe++){float x=-w+3+stripe*(ap->width-6)/8;pavement(ap,start,start+22,x,x+1.6f,235,235,220,.06f);}
 }
 /* Runway numbers from the record (L/R/C suffixes are not painted). */
 digit(ap,ap->le[0]-'0',-half+70,-8,0);digit(ap,ap->le[1]-'0',-half+70,2,0);
 digit(ap,ap->he[0]-'0',half-70,8,1);digit(ap,ap->he[1]-'0',half-70,-2,1);
 /* Faded rubber streaks in generic touchdown zones, not surveyed markings. */
 for(int end=-1;end<=1;end+=2)for(int stripe=0;stripe<6;stripe++){
  float center=end*(half-260),length=65+stripe*14,left=(stripe-2.5f)*1.65f;
  pavement(ap,center-length*.5f,center+length*.5f,left,left+.42f,82,84,86,.035f);
 }
 /* Paired aiming-point bars, subdued paint with room for threshold numbers. */
 for(int end=-1;end<=1;end+=2)for(int side=-1;side<=1;side+=2){
  float center=end*(half-300),left=side<0?-w*.62f:w*.35f;
  pavement(ap,center-18,center+18,left,left+w*.27f,223,223,209,.065f);
 }
 /* Simple edge-light fixtures: generic positions, not surveyed lighting. */
 for(float t=-half+30;t<half;t+=90){
  pavement(ap,t,t+1.8f,-w-2.5f,-w-.7f,231,230,200,.075f);
  pavement(ap,t,t+1.8f,w+.7f,w+2.5f,231,230,200,.075f);
 }
 if(!detailed||!ap->primary||!ap->apronSide)return;
 /* Apron, parallel taxiway and buildings sit on apronSide; m mirrors the
    v25 left-side layout (m = 1) to the right (m = -1). */
 float m=(float)-ap->apronSide;
 pavement(ap,-650,250,-245*m,-50*m,124,125,119,.02f);
 pavement(ap,-half+65,half-65,-64*m,-47*m,106,109,106,.03f);
 pavement(ap,-half+65,half-65,-55.4f*m,-55*m,225,184,62,.055f);
 for(int i=0;i<3;i++){float t=-570+i*350;pavement(ap,t,t+18,-56*m,(-w+1)*m,108,109,106,.04f);}
 for(int i=0;i<5;i++)building(ap,-490+i*125,-227*m,74,34,10);
 building(ap,175,-175*m,52,36,8);building(ap,220,-214*m,35,28,7);
 for(int i=0;i<6;i++)pavement(ap,-520+i*100,-519+i*100,-187*m,-140*m,209,182,76,.06f);
 for(int i=0;i<6;i++)pavement(ap,-535+i*100,-505+i*100,-187*m,-186*m,209,182,76,.06f);
 /* Simple apron fixtures; generic placements, not a surveyed building replica. */
 building(ap,-560,-88*m,13,10,4);
 if(ap!=&airports[0]){
  building(ap,110,-205*m,10,10,ap==&airports[1]?15:19);
  building(ap,110,-205*m,16,16,3);
  for(int i=0;i<3;i++)building(ap,-350+i*170,-175*m,45,30,8);
 }
}
/* Rendered ground only. Inside the flattened airport core the terrain mesh
   was exactly coplanar with the runway plane, only 1-2.5 cm below the
   pavement layers. Per-frame PICA vertex rounding then flipped visibility
   between asphalt and the photo runway underneath (v28/v29 hardware
   flicker). Sinking the hidden ground gives pavement a clear depth margin.
   Physics, wheel contact and camera clearance still use terrain(). */
#define AIRPORT_GROUND_SINK .12f
float sceneryGroundHeight(float e,float n){
 float flat,h=terrainWithFlattening(e,n,&flat);
 return flat>0?h-AIRPORT_GROUND_SINK*flat:h;
}
static Vec groundVertex(float cx,float cy,int x,int y,float cell){
 float e=cx+x*cell,n=cy+y*cell,h=sceneryGroundHeight(e,n);
 /* Stitch fine outer edges to the next coarser ring, avoiding LOD cracks. */
 if(abs(x)==16&&(abs(y)%2))h=(sceneryGroundHeight(e,n-cell)+sceneryGroundHeight(e,n+cell))/2;
 if(abs(y)==16&&(abs(x)%2))h=(sceneryGroundHeight(e-cell,n)+sceneryGroundHeight(e+cell,n))/2;
 return (Vec){e,n,h};
}
typedef struct {int x,y;unsigned generation;float rgb[3];} GroundColor;
static GroundColor colorCache[4096];static unsigned colorGeneration;
static unsigned hashCell(int x,int y);
static float smooth01(float x){x=clampf(x,0,1);return x*x*(3-2*x);}
static float boxMask(float e,float n,float west,float south,float east,float north,float feather){
 return smooth01(fminf(fminf(e-west,east-e),fminf(n-south,north-n))/feather);
}
static void blendColor(float out[3],const float target[3],float t){for(int i=0;i<3;i++)out[i]+=(target[i]-out[i])*t;}
/* Illustrative Grand Canyon stratigraphy, rim to river, near Grand Canyon
   Village. Layers rise roughly 2.2 m per km northward toward the North Rim.
   These are artistic approximations of the formations, not a geologic map. */
static void canyonStrata(float layer,float out[3]){
 static const struct {float top;float rgb[3];} bands[]={
  {9999,{204,192,165}},  /* Kaibab limestone: pale grey-cream rim */
  {2010,{191,163,124}},  /* Toroweap: buff ledges */
  {1935,{226,210,174}},  /* Coconino sandstone: bright cream cliff band */
  {1840,{158,82,56}},    /* Hermit shale: deep red slope */
  {1745,{178,96,64}},    /* Supai group: red ledges */
  {1470,{168,112,92}},   /* Redwall limestone: grey stained red cliff */
  {1290,{164,140,106}},  /* Muav / Bright Angel: grey-green Tonto Platform */
  {1060,{122,88,64}},    /* Tapeats sandstone: dark brown */
  {990,{118,100,92}}     /* Vishnu schist: dark inner gorge */
 };
 int i=0;while(i<8&&layer<bands[i+1].top)i++;
 /* Soften band edges and add fine ledge striping within each band. */
 float fine=1+.06f*sinf(layer*.31f)+.03f*sinf(layer*.083f);
 for(int c=0;c<3;c++)out[c]=clampf(bands[i].rgb[c]*fine/255,0,1);
}
/* Fixed southwest sun, 30 degrees above the horizon: low enough for long
   relief shading. Unit vector toward the sun is (-.6124,-.6124,.5). */
#define SUN_TAN .57735f
/* Baked terrain shadow: march toward the sun through the elevation grid and
   report how strongly ridges or canyon walls block it (0 lit, 1 shadowed).
   Computed per unique vertex during incremental scenery builds only. */
static float terrainShadow(Vec p){
 static const float steps[]={50,130,260,450,750,1200,1900,3000};
 float shadow=0;
 for(int i=0;i<8;i++){
  float t=steps[i],ground=rawElevation(p.x-.7071f*t,p.y-.7071f*t),ray=p.z+t*SUN_TAN;
  /* Soft edge: full shadow once the blocker clears the ray by 6% of range. */
  shadow=fmaxf(shadow,clampf((ground-ray)/(t*.06f+4),0,1));
  if(shadow>=1)break;
 }
 return shadow;
}
/* The march is a pure function of vertex position: the sun is fixed and the
   elevation grids are static. LOD rings are world-snapped, so each rebuild
   (one every 256 m of travel) revisits nearly the same vertices. Remember
   results across builds instead of re-marching them; only an exact x/y/z
   match is reused, so shading is bit-identical. A rebuild touches ~10k
   vertices; 4-way sets avoid the eviction churn a direct-mapped table of
   this size suffers. 8192 sets x 4 ways x 16 bytes = 512 KB. */
#define SHADOW_CACHE_SETS 8192
#define SHADOW_CACHE_WAYS 4
typedef struct {float x,y,z,stored;} ShadowEntry; /* stored = shadow + 1; 0 = empty */
static ShadowEntry shadowCache[SHADOW_CACHE_SETS][SHADOW_CACHE_WAYS];
static float cachedTerrainShadow(Vec p){
 ShadowEntry *set=shadowCache[hashCell((int)floorf(p.x),(int)floorf(p.y))&(SHADOW_CACHE_SETS-1)];
 for(int i=0;i<SHADOW_CACHE_WAYS;i++){
  ShadowEntry *e=&set[i];
  if(e->stored>0&&e->x==p.x&&e->y==p.y&&e->z==p.z){
   ShadowEntry hit=*e;for(int j=i;j>0;j--)set[j]=set[j-1];set[0]=hit; /* move to front */
   return hit.stored-1;
  }
 }
 float shadow=terrainShadow(p);
 for(int j=SHADOW_CACHE_WAYS-1;j>0;j--)set[j]=set[j-1]; /* evict least recent */
 set[0]=(ShadowEntry){p.x,p.y,p.z,shadow+1};
 return shadow;
}
static void surfaceColor(float e,float n,float height,float dx,float dy,int regional,float sunlit,float rgb[3]){
 float slope=hypotf(dx,dy),norm=sqrtf(1+dx*dx+dy*dy);
 /* Ambient light keeps opposing canyon walls and shadows readable. This is
    baked when a scenery part is built, not recalculated every frame. */
 float diffuse=fmaxf(0,(.6124f*dx+.6124f*dy+.5f)/norm)*sunlit;
 float shade=.36f+.72f*diffuse;
 float grass=clampf((height-1650)/500,0,1),forest=clampf((height-2050)/350,0,1);
 float rock=clampf((slope-.25f)/.65f,0,1);
 float band=1+(.025f+.045f*(1-grass))*sinf(height*.14f)+.02f*sinf(height*.037f);
 float r=(177*(1-grass)+142*grass)*(1-forest)+94*forest;
 float g=(116*(1-grass)+137*grass)*(1-forest)+119*forest;
 float b=(77*(1-grass)+103*grass)*(1-forest)+86*forest;
 /* Illustrative land cover: warm sandstone low down, pale exposed rock at
    higher elevations. The aerial imagery remains the dominant detail. */
 r=r*(1-rock)+(194-5*grass)*rock;
 g=g*(1-rock)+(120+42*grass)*rock;
 b=b*(1-rock)+(81+47*grass)*rock;
 float base[3]={r/255,g/255,b/255};
 if(regional){
  /* Exposed rock reads on slopes; flat benches keep some ground cover. */
  float exposure=smooth01((slope-.10f)/.45f);
  /* v31 statewide palettes. Illustrative, from elevation, slope and broad
     region masks, not land-cover data; aerial imagery dominates nearby. */
  {
   unsigned hash=hashCell((int)floorf(e/90),(int)floorf(n/90));float speck=(hash&255)/255.f;
   /* Sonoran / Mojave basins: pale desert floor, mottled creosote scrub,
      grey-brown desert ranges; fades out between 700 and 1150 m. */
   float low=smooth01((1150-height)/450)*(1-forest);
   if(low>0){
    float floorColor[3]={188/255.f,166/255.f,132/255.f},scrub[3]={118/255.f,116/255.f,82/255.f},range[3]={126/255.f,106/255.f,92/255.f};
    float t[3]={floorColor[0],floorColor[1],floorColor[2]};blendColor(t,scrub,.30f*speck*(1-exposure));blendColor(t,range,exposure);
    blendColor(base,t,low);band=1+(band-1)*(1-low*.7f);
   }
   /* South-east semi-desert grassland (Sonoita, Sulphur Springs, San Pedro). */
   float grassland=smooth01((-120000-n)/40000)*smooth01((e-20000)/40000)*smooth01((height-1000)/250)*(1-smooth01((height-1800)/200))*(1-exposure);
   if(grassland>0){float gold[3]={178/255.f,160/255.f,110/255.f};blendColor(gold,(const float[3]){150/255.f,140/255.f,96/255.f},.4f*speck);blendColor(base,gold,grassland*.8f);}
   /* North-east Colorado Plateau: buff-to-orange sandstone; painted banding
      only on exposed slopes so flats do not show contour stripes. */
   float plateau=smooth01((n-50000)/40000)*smooth01((e+10000)/30000)*smooth01((height-1200)/200)*(1-forest);
   if(plateau>0){
    float sand[3]={192/255.f,140/255.f,100/255.f},painted[3]={152/255.f,128/255.f,138/255.f};
    blendColor(sand,(const float[3]){170/255.f,150/255.f,118/255.f},.45f*speck);
    blendColor(sand,painted,exposure*(.5f+.5f*sinf(height*.045f)));blendColor(base,sand,plateau*(.45f+.3f*exposure));
   }
   /* Central transition zone (chaparral, juniper, desert grassland): the old
      warm-sandstone base reads as red rock far from Sedona. Sedona and the
      canyon re-apply their own palettes below. */
   float mid=(1-low)*(1-grass)*(1-plateau)*(1-forest);
   if(mid>0){
    float brush[3]={156/255.f,140/255.f,104/255.f},chaparral[3]={112/255.f,114/255.f,84/255.f},rockMid[3]={146/255.f,122/255.f,100/255.f};
    blendColor(brush,chaparral,.45f*speck*(1-exposure));blendColor(brush,rockMid,exposure);
    blendColor(base,brush,mid*.75f);
   }
   /* Phoenix and Tucson metro areas read as built-up beyond the photo window;
      ragged edges from coarse hash noise, not a surveyed boundary. */
   float ragged=(hashCell((int)floorf(e/3000),(int)floorf(n/3000))&255)/255.f*7000;
   float urban=fmaxf(smooth01((30000-ragged-hypotf(e+25000,n+152000))/9000),smooth01((15000-ragged*.5f-hypotf(e-80000,n+292000))/6000))*smooth01((1300-height)/200)*(1-exposure);
   if(urban>0){float city[3]={160/255.f,154/255.f,146/255.f};blendColor(city,(const float[3]){120/255.f,124/255.f,104/255.f},.4f*speck);blendColor(base,city,urban*.45f);}
   /* Strata banding belongs on exposed rock, not flat basins and mesas. */
   band=1+(band-1)*(.25f+.75f*exposure);
   /* v35.13: the photo-derived map replaces the palette above wherever the grid reaches. It is
      divided by the flat-ground shade (.36 + .72 * .5) so level ground comes out at the photo colour
      and slopes keep their relief shading. */
   float mapped[3];
   if(groundMapColor(e,n,mapped)){
    float target[3]={clampf(mapped[0]/.72f,0,1),clampf(mapped[1]/.72f,0,1),clampf(mapped[2]/.72f,0,1)};
    blendColor(base,target,.92f);band=1+(band-1)*.2f;
   }
  }
  float canyon=boxMask(e,n,-95000,117000,5000,165000,4000);
  if(canyon>0){
   float strata[3];canyonStrata(height-.0022f*clampf(n-135000,-20000,25000),strata);
   /* Forested rim plateaus stay vegetated; walls and slopes show strata. */
   float weight=canyon*(.12f+.88f*exposure)*(1-forest*(1-exposure));
   blendColor(base,strata,weight);band=1+(band-1)*(1-weight);
  }
  /* Sedona red rocks: Schnebly Hill red-orange below the Coconino buff caps,
     fading out toward the Mogollon Rim forest. */
  /* v31: wider, ragged fade so the red country does not end in a visible
     circle against the new central-Arizona palette. */
  float sedonaEdge=(hashCell((int)floorf(e/2500)+911,(int)floorf(n/2500))&255)/255.f*6000;
  float sedona=smooth01((22000-sedonaEdge-hypotf(e,n+2000))/10000)*(1-forest);
  if(sedona>0){
   float red[3]={198/255.f,86/255.f,50/255.f},soil[3]={172/255.f,98/255.f,66/255.f},buff[3]={216/255.f,184/255.f,140/255.f};
   float cap=smooth01((height-1680)/90)*(1-smooth01((height-1900)/120));
   float rockColor[3]={red[0],red[1],red[2]};blendColor(rockColor,buff,cap);
   float target[3]={soil[0],soil[1],soil[2]};blendColor(target,rockColor,exposure);
   blendColor(base,target,sedona*.85f);
  }
 }
 float snow=clampf((height-3350)/350,0,.8f)*(1-clampf(slope-.6f,0,1));
 rgb[0]=clampf((base[0]*(1-snow)+222/255.f*snow)*shade*band,0,1);
 rgb[1]=clampf((base[1]*(1-snow)+225/255.f*snow)*shade*band,0,1);
 rgb[2]=clampf((base[2]*(1-snow)+224/255.f*snow)*shade*band,0,1);
}
void scenerySurfaceColor(float height,float dx,float dy,float rgb[3]){surfaceColor(0,0,height,dx,dy,0,1,rgb);}
/* Unfogged regional ground colour with baked shadow, for previews/tests. */
void sceneryGroundColorAt(float e,float n,float rgb[3]){
 Vec p={e,n,terrain(e,n)};
 float dx=(terrain(e+24,n)-terrain(e-24,n))/48,dy=(terrain(e,n+24)-terrain(e,n-24))/48;
 surfaceColor(e,n,p.z,dx,dy,1,1-.85f*terrainShadow(p),rgb);
}
static void groundColor(Vec p,float rgb[3]){
 int x=(int)p.x,y=(int)p.y;GroundColor *cached=&colorCache[hashCell(x/16,y/16)&4095];
 if(cached->generation==colorGeneration&&cached->x==x&&cached->y==y){for(int i=0;i<3;i++)rgb[i]=cached->rgb[i];return;}
 float dx=(terrain(p.x+24,p.y)-terrain(p.x-24,p.y))/48,dy=(terrain(p.x,p.y+24)-terrain(p.x,p.y-24))/48;
 surfaceColor(p.x,p.y,p.z,dx,dy,1,1-.85f*cachedTerrainShadow(p),rgb);
 float fog=sceneryHaze(hypotf(p.x-viewer.x,p.y-viewer.y));
 rgb[0]=rgb[0]*(1-fog)+125*fog/255;rgb[1]=rgb[1]*(1-fog)+177*fog/255;rgb[2]=rgb[2]*(1-fog)+207*fog/255;
 cached->x=x;cached->y=y;cached->generation=colorGeneration;for(int i=0;i<3;i++)cached->rgb[i]=rgb[i];
}
static void groundTriangle(Vec a,Vec b,Vec c){
 if(used+3>limit)return;
 Vec points[3]={a,b,c};for(int i=0;i<3;i++){WorldVertex v={.p=points[i]};groundColor(points[i],v.color);if(used<SCENERY_PART_CAPACITY)vertexMaterial[used]=-1;output[used++]=v;}
}
/* v31.5: fan from the measured centre to the four corners. The former
   8-triangle fan added edge midpoints that were exact corner averages (no
   extra height detail) but formed T-junctions with every unrefined
   neighbour; with the PICA200's float24 transform those opened hairline
   cracks that showed the sky in distant terrain on hardware. */
static void reliefCell(Vec a,Vec b,Vec c,Vec d){
 Vec middle={(a.x+c.x)/2,(a.y+c.y)/2,0};middle.z=sceneryGroundHeight(middle.x,middle.y);
 groundTriangle(a,b,middle);groundTriangle(b,c,middle);groundTriangle(c,d,middle);groundTriangle(d,a,middle);
}
/* v31.5: vertical skirt below a ring's outer edge. Each ring's odd outer
   vertices sit mid-edge on the next coarser ring (T-junctions); on hardware
   float24 rounding opens hairline gaps there. Looking out from inside the
   ring, a gap now shows this terrain-coloured skirt instead of the sky. */
/* v31.6: which edge of coarse cell (x,y) borders the finer ring's hole
   (0 bottom a-b, 1 right b-c, 2 top c-d, 3 left d-a), or -1. */
static int holeEdge(int x,int y,int innerX,int innerY){
 int inRows=y>=innerY&&y<innerY+16,inCols=x>=innerX&&x<innerX+16;
 if(x==innerX-1&&inRows)return 1;
 if(x==innerX+16&&inRows)return 3;
 if(y==innerY-1&&inCols)return 2;
 if(y==innerY+16&&inCols)return 0;
 return -1;
}
/* v31.6: the finer ring's odd outer vertices lie exactly halfway along these
   coarse edges (groundVertex stitches them to the corner average). Fan the
   coarse cell from that same midpoint, computed from the same corner heights
   in the same order (lower/left first), so both rings share bit-identical
   vertices: no T-junction is left for float24 rounding to open on hardware. */
static void holeBorderCell(const Vec q[4],int edge){
 Vec p0=q[edge],p1=q[(edge+1)%4];int lowFirst=edge<2;   /* a-b and b-c run low->high; c-d and d-a run high->low */
 Vec lo=lowFirst?p0:p1,hi=lowFirst?p1:p0;
 Vec m={(lo.x+hi.x)/2,(lo.y+hi.y)/2,(lo.z+hi.z)/2};
 groundTriangle(m,q[(edge+1)%4],q[(edge+2)%4]);
 groundTriangle(m,q[(edge+2)%4],q[(edge+3)%4]);
 groundTriangle(m,q[(edge+3)%4],q[edge]);
}
static void skirtEdge(Vec p,Vec q,float depth){
 Vec p2=p,q2=q;p2.z-=depth;q2.z-=depth;
 groundTriangle(p,q,q2);groundTriangle(p,q2,p2);
}
static unsigned hashCell(int x,int y){unsigned h=(unsigned)x*374761393u+(unsigned)y*668265263u;h=(h^(h>>13))*1274126177u;return h^(h>>16);}
static void scatter(const Flight *s,int quadrant){
 int cx=(int)floorf(s->pos.x/128),cy=(int)floorf(s->pos.y/128);
 int x0=(quadrant&1)?0:-12,y0=(quadrant&2)?0:-12;
 for(int y=y0;y<y0+12;y++)for(int x=x0;x<x0+12;x++){
  int gx=cx+x,gy=cy+y;unsigned h=hashCell(gx,gy);
  if((h&255)>112)continue;
  float e=(gx+.15f+((h>>8)&255)/360.f)*128,n=(gy+.15f+((h>>16)&255)/360.f)*128;
  int blocked=0;const uint16_t *list;int listed=airportCellRunways(e,n,&list); /* v1.1p20: this cell's runways */
  for(int k=0;k<listed&&!blocked;k++){
   const Airport *ap=&airports[list[k]];float dx=e-ap->east,dy=n-ap->north;
   if(fabsf(dx)>ap->length+400||fabsf(dy)>ap->length+400)continue;
   float angle=ap->heading*PI/180;
   if(fabsf(dx*sinf(angle)+dy*cosf(angle))<ap->length*.6f&&fabsf(dx*cosf(angle)-dy*sinf(angle))<310)blocked=1;
  }
  if(blocked)continue;
  float z=terrain(e,n),slope=hypotf(terrain(e+10,n)-z,terrain(e,n+10)-z)/10;
  if(slope>.55f)continue;
  float size=2+(h&7)*.35f;
  if(z>1850){
   float height=7+(h&15)*.5f;Vec tip={e,n,z+height};
   Vec corners[4]={{e-size,n-size,z+height*.25f},{e+size,n-size,z+height*.25f},{e+size,n+size,z+height*.25f},{e-size,n+size,z+height*.25f}};
   for(int j=0;j<4;j++)triangle(corners[j],corners[(j+1)%4],tip,43+j*4,72+j*3,43+j*2);
   quad((Vec){e-.22f,n,z},(Vec){e+.22f,n,z},(Vec){e+.22f,n,z+height*.3f},(Vec){e-.22f,n,z+height*.3f},93,72,49);
  }else if((h&3)==0){
   Vec tip={e,n,z+size*.65f};Vec corners[4]={{e-size,n-size,z},{e+size,n-size,z},{e+size,n+size,z},{e-size,n+size,z}};
   for(int j=0;j<4;j++)triangle(corners[j],corners[(j+1)%4],tip,155+j*6,105+j*3,72+j*2);
  }else{
   Vec tip={e,n,z+size};triangle((Vec){e-size,n,z},(Vec){e+size,n,z},tip,100,111,70);triangle((Vec){e,n-size,z},(Vec){e,n+size,z},tip,111,121,77);
  }
 }
}
void sceneryLodLayout(Vec position,int level,float *east,float *north,int *innerX,int *innerY){
 float cell=32*(1<<level),snap=2*cell;
 *east=floorf(position.x/snap)*snap;*north=floorf(position.y/snap)*snap;
 /* The finer ring's centre is snapped at this level's cell spacing. Its
    bounds therefore land on this grid, including across negative origins. */
 *innerX=(int)((floorf(position.x/cell)*cell-*east)/cell)-8;
 *innerY=(int)((floorf(position.y/cell)*cell-*north)/cell)-8;
}
int buildSceneryPart(const Flight *s,WorldVertex *vertices,int capacity,int part){
 /* Newly published elevation tiles can change shadow rays that cross them. */
 {static unsigned seen;unsigned g=terrainStreamGeneration();if(g!=seen){memset(shadowCache,0,sizeof(shadowCache));seen=g;}}
 output=vertices;used=0;limit=capacity;viewer=s->pos;surfaceMaterial=-1;colorGeneration++;
 if(!colorGeneration){memset(colorCache,0,sizeof(colorCache));colorGeneration=1;}
 if(part<0||part>=SCENERY_PARTS)return 0;
 if(part<32){
  int level=part/4,start=-16+(part%4)*8;float cell=32*(1<<level);
  float cx,cy;int innerX,innerY;sceneryLodLayout(s->pos,level,&cx,&cy,&innerX,&innerY);
  /* Rank curvature rather than refining the first cells encountered. Each job
     gets at most 24 extra centers; edges retain the shared coarse elevations. */
  float error[256]={0},centerHeight[256];unsigned char refine[256]={0};
  static Vec corners[256][4];
  /* v34.04: each grid point once (was four times, once per touching cell),
     and only the runways that can flatten this strip. */
  terrainLimitAirports(cx-17*cell-50,cy+(start-1)*cell-50,cx+17*cell+50,cy+(start+9)*cell+50);
  static Vec grid[9][33];static unsigned char gridSet[9][33];memset(gridSet,0,sizeof(gridSet));
  #define GRID(gx,gy) (gridSet[(gy)-start][(gx)+16]?grid[(gy)-start][(gx)+16]:(gridSet[(gy)-start][(gx)+16]=1,grid[(gy)-start][(gx)+16]=groundVertex(cx,cy,gx,gy,cell)))
  {
   for(int y=start;y<start+8;y++)for(int x=-16;x<16;x++){
    if(level&&x>=innerX&&x<innerX+16&&y>=innerY&&y<innerY+16)continue;
    Vec a=GRID(x,y),b=GRID(x+1,y),c=GRID(x+1,y+1),d=GRID(x,y+1);
    float actual=sceneryGroundHeight((a.x+c.x)/2,(a.y+c.y)/2);
    int index=(y-start)*32+x+16;
    corners[index][0]=a;corners[index][1]=b;corners[index][2]=c;corners[index][3]=d;centerHeight[index]=actual;
    if(level>=1&&level<=6&&holeEdge(x,y,innerX,innerY)<0)error[index]=fminf(fabsf(actual-(a.z+c.z)/2),fabsf(actual-(b.z+d.z)/2));
   }
   for(int job=0;job<24;job++){
    int best=-1;float threshold=2.5f+cell*.006f;
    for(int i=0;i<256;i++)if(!refine[i]&&error[i]>threshold){threshold=error[i];best=i;}
    if(best<0)break;
    refine[best]=1;
   }
  }
  for(int y=start;y<start+8;y++)for(int x=-16;x<16;x++){
   if(level&&x>=innerX&&x<innerX+16&&y>=innerY&&y<innerY+16)continue;
   int index=(y-start)*32+x+16;
   Vec a=corners[index][0],b=corners[index][1],c=corners[index][2],d=corners[index][3];
   int border=level?holeEdge(x,y,innerX,innerY):-1;
   if(border>=0)holeBorderCell(corners[index],border);
   else if(refine[(y-start)*32+x+16])reliefCell(a,b,c,d);
   else{
    float actual=centerHeight[index];
    if(fabsf(actual-(a.z+c.z)/2)<=fabsf(actual-(b.z+d.z)/2)){groundTriangle(a,b,c);groundTriangle(a,c,d);}
    else{groundTriangle(a,b,d);groundTriangle(b,c,d);}
   }
   {float skirt=cell*.25f+4;
    if(x==-16)skirtEdge(a,d,skirt);
    if(x==15)skirtEdge(b,c,skirt);
    if(y==-16)skirtEdge(a,b,skirt);
    if(y==15)skirtEdge(d,c,skirt);}
  }
  #undef GRID
  terrainLimitAirportsOff();
 }else{
  /* Spread runway records over the four object parts so multi-runway
     airports (KPHX + KDVT within 45 km) stay inside one part's capacity.
     v1.1p20: thousands of runways: those within 45 km nearest first, the
     ones 15 km+ away coarse pavement only, and a runway that would leave a part
     less than 1800 vertices for the ground scatter is left out. */
  static int near[SCENERY_NEAR_RUNWAYS];int nearCount=airportsNear(s->pos.x,s->pos.y,45000,near,SCENERY_NEAR_RUNWAYS);
  sceneryRunwayStats[part-32][0]=sceneryRunwayStats[part-32][1]=0;
  for(int k=part-32;k<nearCount;k+=4){
   const Airport *ap=&airports[near[k]];int before=used;
   airport(ap,hypotf(s->pos.x-ap->east,s->pos.y-ap->north)<15000?1:-1);
   if(used>limit-1800){used=before;sceneryRunwayStats[part-32][1]++;continue;}   /* a smaller one may still fit */
   sceneryRunwayStats[part-32][0]++;
  }
  scatter(s,part-32);
 }
 return used;
}
/* v34.04 terrain part cache. A whole-scene rebuild starts whenever the
   snapped anchor moves (every second or so for a climbing airliner or a fast
   jet), and each of the 32 terrain parts cost 20-35 ms on hardware even when
   its ring had not moved: the coarse rings snap at 2-8 km and rarely change.
   A part is reused when its ring layout and the published elevation are
   unchanged; only the baked haze depends on the viewer, so it is taken out
   with the old viewer and put back with the new one. */
typedef struct {WorldVertex *v;signed char *material;int count,capacity,valid;float cx,cy;int ix,iy;unsigned generation;Vec viewer;} PartCache;
static PartCache partCache[32];static int partCacheHits,partCacheMisses;
static const float hazeColour[3]={125/255.f,177/255.f,207/255.f};
int buildSceneryPartCached(const Flight *s,WorldVertex *vertices,int capacity,int part){
 if(part<0||part>=32)return buildSceneryPart(s,vertices,capacity,part);
 float cx,cy;int ix,iy;sceneryLodLayout(s->pos,part/4,&cx,&cy,&ix,&iy);
 /* Heights under the ring (plus stitching), and the shadow march up to
    2.1 km toward the south-west sun. */
 float cell=32.f*(1<<(part/4)),half=17*cell;
 unsigned generation=terrainStreamRegionVersion(cx-half-2200,cy-half-2200,cx+half+100,cy+half+100);PartCache *c=&partCache[part];
 if(c->valid&&c->cx==cx&&c->cy==cy&&c->ix==ix&&c->iy==iy&&c->generation==generation&&c->count<=capacity){
  for(int i=0;i<c->count;i++){
   WorldVertex w=c->v[i];
   float f0=sceneryHaze(hypotf(w.p.x-c->viewer.x,w.p.y-c->viewer.y)),f1=sceneryHaze(hypotf(w.p.x-s->pos.x,w.p.y-s->pos.y));
   if(f0!=f1)for(int k=0;k<3;k++){float clear=(w.color[k]-hazeColour[k]*f0)/(1-f0);w.color[k]=clear*(1-f1)+hazeColour[k]*f1;}
   vertices[i]=w;
  }
  memcpy(vertexMaterial,c->material,c->count<SCENERY_PART_CAPACITY?c->count:SCENERY_PART_CAPACITY);
  partCacheHits++;return c->count;
 }
 int n=buildSceneryPart(s,vertices,capacity,part);partCacheMisses++;
 if(n>c->capacity){
  WorldVertex *v=realloc(c->v,n*sizeof(WorldVertex));signed char *m=realloc(c->material,n);
  if(v)c->v=v;
  if(m)c->material=m;
  if(!v||!m){c->valid=0;return n;}
  c->capacity=n;
 }
 memcpy(c->v,vertices,n*sizeof(WorldVertex));memcpy(c->material,vertexMaterial,n<SCENERY_PART_CAPACITY?n:SCENERY_PART_CAPACITY);
 c->count=n;c->valid=1;c->cx=cx;c->cy=cy;c->ix=ix;c->iy=iy;c->generation=generation;c->viewer=s->pos;
 return n;
}
void sceneryPartCacheStats(int *hits,int *misses){*hits=partCacheHits;*misses=partCacheMisses;partCacheHits=partCacheMisses=0;}
void freeSceneryPartCache(void){for(int i=0;i<32;i++){free(partCache[i].v);free(partCache[i].material);partCache[i]=(PartCache){0};}}
int buildScenery(const Flight *s,WorldVertex *vertices,int capacity){
 int total=0;for(int part=0;part<SCENERY_PARTS;part++)total+=buildSceneryPart(s,vertices+total,capacity-total,part);
 return total;
}
