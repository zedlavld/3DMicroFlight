#include "imagery.h"
#include <string.h>
static const Vec photoOrigins[3]={{0,0,0},{10624,32256,0},{-32768,128000,0}};
static const char *photoNames[3]={"sedona","flagstaff","canyon"};
Vec imageryRegionOrigin(int region){return photoOrigins[region>=0&&region<3?region:0];}
const char *imageryRegionName(int region){return photoNames[region>=0&&region<3?region:0];}
int chooseImageryRegion(Vec position,int current,int airport,int previousAirport){
 if(current<0||current>2)current=0;
 if(airport!=previousAirport&&airport>=0&&airport<3)return airport;
 Vec origin=photoOrigins[current];float best=hypotf(position.x-origin.x,position.y-origin.y);
 for(int i=0;i<3;i++){
  float e=position.x-photoOrigins[i].x,n=position.y-photoOrigins[i].y,d=hypotf(e,n);
  if(fabsf(e)<8192&&fabsf(n)<8192&&d+1024<best){current=i;best=d;}
 }
 return current;
}
static Vec photoOrigin,coarseOrigin;static int fineEnabled=1;
void setImageryFineEnabled(int enabled){fineEnabled=enabled;}
void setImageryOrigin(Vec origin){photoOrigin=coarseOrigin=origin;fineEnabled=1;}
void setImageryWindows(Vec fine,Vec coarse){photoOrigin=fine;coarseOrigin=coarse;}
Vec getImageryOrigin(void){return photoOrigin;}
Vec getImageryCoarseOrigin(void){return coarseOrigin;}
int imageryCoarseTile(int tile){
 float e=photoOrigin.x-2048+(tile%2)*2048,n=photoOrigin.y-2048+(tile/2)*2048;
 return 4+(e>=coarseOrigin.x?1:0)+(n>=coarseOrigin.y?2:0);
}
static WorldVertex intersection(WorldVertex a,WorldVertex b,int axis,float edge){
 float av=axis?a.p.y:a.p.x,bv=axis?b.p.y:b.p.x;
 /* Adjacent triangles traverse their shared edge in opposite directions.
    Canonical interpolation prevents tiny float differences from opening
    cracks when that edge is split at a photographic tile boundary. */
 if(av>bv){WorldVertex swap=a;a=b;b=swap;float f=av;av=bv;bv=f;}
 float t=(edge-av)/(bv-av);WorldVertex p;
 p.p=(Vec){a.p.x+t*(b.p.x-a.p.x),a.p.y+t*(b.p.y-a.p.y),a.p.z+t*(b.p.z-a.p.z)};
 if(axis)p.p.y=edge;else p.p.x=edge;
 for(int i=0;i<3;i++)p.color[i]=a.color[i]+t*(b.color[i]-a.color[i]);
 return p;
}
void imageryCoarseTransform(int tile,float uv[4]){
 float scale=(1024.f/1022)*.25f*(510.f/512);
 int coarse=imageryCoarseTile(tile)-4;
 float x=(photoOrigin.x-2048+(tile%2)*2048)-(coarseOrigin.x-8192+(coarse%2)*8192);
 float y=(photoOrigin.y-2048+(tile/2)*2048)-(coarseOrigin.y-8192+(coarse/2)*8192);
 uv[0]=uv[1]=scale;uv[2]=1/512.f+x*510/(8192*512.f)-scale/1024;uv[3]=1/512.f+y*510/(8192*512.f)-scale/1024;
}
int imageryMaterial(int region){return region<4?-1:region<IMAGERY_FINE_REGION?4+(region-4)/4:region-IMAGERY_FINE_REGION;}
void imageryMaterialRegions(int material,int *first,int *end){
 if(material<0){*first=0;*end=4;}
 else if(material<4){*first=IMAGERY_FINE_REGION+material;*end=*first+1;}
 else{*first=4+(material-4)*4;*end=*first+4;}
}
typedef struct {float left,right,bottom,top,tileLeft,tileBottom,span,size;int tile;} RegionBounds;
/* 0: the region keeps nothing. */
static int regionBounds(int region,RegionBounds *rb){
 if(!fineEnabled&&region>=IMAGERY_FINE_REGION)return 0;
 float left=-1e9f,right=1e9f,bottom=-1e9f,top=1e9f;
 int tile=imageryMaterial(region);float tileLeft=0,tileBottom=0,span=0,size=0;
 float outerLeft=coarseOrigin.x-8192,outerRight=coarseOrigin.x+8192,outerBottom=coarseOrigin.y-8192,outerTop=coarseOrigin.y+8192;
 float innerLeft=photoOrigin.x-2048,innerRight=photoOrigin.x+2048,innerBottom=photoOrigin.y-2048,innerTop=photoOrigin.y+2048;
 if(region==0)right=outerLeft;
 else if(region==1)left=outerRight;
 else if(region==2){left=outerLeft;right=outerRight;top=outerBottom;}
 else if(region==3){left=outerLeft;right=outerRight;bottom=outerTop;}
 else if(region<IMAGERY_FINE_REGION){
  int quadrant=tile-4,col=quadrant%2,row=quadrant/2,strip=(region-4)%4;
  tileLeft=outerLeft+col*8192;tileBottom=outerBottom+row*8192;span=8192;size=512;
  left=tileLeft;right=left+8192;bottom=tileBottom;top=bottom+8192;
  float l=fmaxf(left,innerLeft),r=fminf(right,innerRight),b=fmaxf(bottom,innerBottom),t=fminf(top,innerTop);
  if(!fineEnabled||l>=r||b>=t){if(strip)return 0;}
  else if(strip==0)right=l;
  else if(strip==1)left=r;
  else if(strip==2){left=l;right=r;top=b;}
  else{left=l;right=r;bottom=t;}
  if(left>=right||bottom>=top)return 0;
 }else{
  tileLeft=innerLeft+(tile%2)*2048;tileBottom=innerBottom+(tile/2)*2048;span=2048;size=1024;
  left=tileLeft;right=left+2048;bottom=tileBottom;top=bottom+2048;
 }
 *rb=(RegionBounds){left,right,bottom,top,tileLeft,tileBottom,span,size,tile};return 1;
}
int imageryRegionRect(int region,float rect[4]){
 RegionBounds rb;if(region<0||region>=IMAGERY_REGIONS||!regionBounds(region,&rb))return 0;
 rect[0]=rb.left;rect[1]=rb.right;rect[2]=rb.bottom;rect[3]=rb.top;return 1;
}
int clipImageryTriangle(const WorldVertex input[3],int region,ImageryVertex *out,int capacity,Vec viewer){
 if(region<0||region>=IMAGERY_REGIONS||capacity<0)return -1;
 RegionBounds rb;if(!regionBounds(region,&rb))return 0;
 float left=rb.left,right=rb.right,bottom=rb.bottom,top=rb.top,tileLeft=rb.tileLeft,tileBottom=rb.tileBottom,span=rb.span,size=rb.size;int tile=rb.tile;
 float minx=fminf(input[0].p.x,fminf(input[1].p.x,input[2].p.x)),maxx=fmaxf(input[0].p.x,fmaxf(input[1].p.x,input[2].p.x));
 float miny=fminf(input[0].p.y,fminf(input[1].p.y,input[2].p.y)),maxy=fmaxf(input[0].p.y,fmaxf(input[1].p.y,input[2].p.y));
 if(maxx<left||minx>right||maxy<bottom||miny>top)return 0;
 WorldVertex poly[12],next[12];int count=3;memcpy(poly,input,3*sizeof(WorldVertex));
 const float edges[4]={left,right,bottom,top};
 for(int plane=0;plane<4&&count;plane++){
  int axis=plane/2,keepGreater=(plane%2)==0,n=0;float edge=edges[plane];
  for(int i=0;i<count;i++){
   WorldVertex a=poly[i],b=poly[(i+1)%count];float av=axis?a.p.y:a.p.x,bv=axis?b.p.y:b.p.x;
   int ai=keepGreater?av>=edge:av<=edge,bi=keepGreater?bv>=edge:bv<=edge;
   if(ai)next[n++]=a;
   if(ai!=bi)next[n++]=intersection(a,b,axis,edge);
  }
  count=n;memcpy(poly,next,count*sizeof(WorldVertex));
 }
 if(count<3)return 0;
 if((count-2)*3>capacity)return -1;
 int used=0;
 for(int triangle=1;triangle<count-1;triangle++){
  WorldVertex p[3]={poly[0],poly[triangle],poly[triangle+1]};
  float area=(p[1].p.x-p[0].p.x)*(p[2].p.y-p[0].p.y)-(p[1].p.y-p[0].p.y)*(p[2].p.x-p[0].p.x);
  if(fabsf(area)<.0001f)continue;
  for(int j=0;j<3;j++){
   ImageryVertex v={.p=p[j].p,.uv={0,0},.color={p[j].color[0],p[j].color[1],p[j].color[2],1}};
   if(region>=4){
    v.uv[0]=(1+(v.p.x-tileLeft)*((size-2)/span))/size;
    v.uv[1]=(1+(v.p.y-tileBottom)*((size-2)/span))/size;
    float e=v.p.x-coarseOrigin.x,n=v.p.y-coarseOrigin.y;
    float edge=fminf(fminf(e+8192,8192-e),fminf(n+8192,8192-n));
    if(tile<4){e=v.p.x-photoOrigin.x;n=v.p.y-photoOrigin.y;edge=fminf(fminf(e+2048,2048-e),fminf(n+2048,2048-n));}
    float fade=clampf(edge/(tile<4?384:512),0,1);fade=fade*fade*(3-2*fade);
    /* Blend into the original terrain at the pack boundary and at distance.
       The procedural RGB already contains its own slope lighting and fog. */
    v.color[3]=tile<4?fade:fade*(1-sceneryHaze(hypotf(v.p.x-viewer.x,v.p.y-viewer.y)));
   }
   out[used++]=v;
  }
 }
 return used;
}
