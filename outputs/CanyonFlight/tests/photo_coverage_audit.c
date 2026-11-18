/* Host audit: the photo clipper must partition every terrain triangle.
   For many fine/coarse window placements (fine on and off), the clipped
   pieces of each triangle over all materials must add up to its own area.
   usage: audit [start index] [phase] */
#include "scenery.h"
#include "imagery.h"
#include <stdio.h>
#include <stdlib.h>
static WorldVertex v[SCENERY_PART_CAPACITY];static ImageryVertex out[64];
static double area2(Vec a,Vec b,Vec c){return ((double)b.x-a.x)*((double)c.y-a.y)-((double)b.y-a.y)*((double)c.x-a.x);}
int main(int argc,char **argv){
 int k=argc>1?atoi(argv[1]):3,phase=argc>2?atoi(argv[2]):PHASE_CRUISE;
 Flight s;resetFlightPhase(&s,1,airportStarts[k],phase);
 long tris=0,bad=0,under=0;double worst=0,worstUnder=0;
 for(int config=0;config<64;config++){
  /* Windows snap to 2048 m (fine) and 8192 m (coarse) around the aircraft. */
  float fe=floorf(s.pos.x/2048)*2048+((config%4)-1)*2048,fn=floorf(s.pos.y/2048)*2048+((config/4%4)-1)*2048;
  float ce=floorf(s.pos.x/8192)*8192+((config/16%2))*8192,cn=floorf(s.pos.y/8192)*8192+((config/32%2))*8192;
  setImageryWindows((Vec){fe,fn,0},(Vec){ce,cn,0});
  for(int fine=0;fine<2;fine++){setImageryFineEnabled(fine);
   for(int part=0;part<32;part++){
    int n=buildSceneryPart(&s,v,SCENERY_PART_CAPACITY,part);
    for(int i=0;i+2<n;i+=3){
     double whole=area2(v[i].p,v[i+1].p,v[i+2].p);if(whole==0)continue;  /* skirts */
     double sum=0;
     for(int material=-1;material<IMAGERY_TILES;material++){int first,last;imageryMaterialRegions(material,&first,&last);
      for(int region=first;region<last;region++){
       int m=clipImageryTriangle(v+i,region,out,64,s.pos);
       for(int j=0;j+2<m;j+=3)sum+=area2(out[j].p,out[j+1].p,out[j+2].p);}}
     tris++;double err=fabs(sum-whole)/fabs(whole);
     if(sum<whole*(1-1e-3)){under++;if(1-sum/whole>worstUnder){worstUnder=1-sum/whole;printf("UNDER config %d fine %d part %d tri %d: %.4f covered (%.0f,%.0f)(%.0f,%.0f)(%.0f,%.0f)\n",config,fine,part,i/3,sum/whole,v[i].p.x,v[i].p.y,v[i+1].p.x,v[i+1].p.y,v[i+2].p.x,v[i+2].p.y);}}
     if(err>1e-3){bad++;if(err>worst){worst=err;
      printf("config %d fine %d part %d tri %d: pieces cover %.4f of the triangle (%.0f,%.0f)(%.0f,%.0f)(%.0f,%.0f) fineWin %.0f,%.0f coarseWin %.0f,%.0f\n",
       config,fine,part,i/3,sum/whole,v[i].p.x,v[i].p.y,v[i+1].p.x,v[i+1].p.y,v[i+2].p.x,v[i+2].p.y,fe,fn,ce,cn);}}
    }
   }
  }
 }
 printf("%ld triangle checks, %ld not exactly covered (worst relative error %.4f), %ld under-covered\n",tris,bad,worst,under);
 return bad!=0;
}
