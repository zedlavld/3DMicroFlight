#include "weather_visual.h"
#include "terrain.h"
static unsigned cellHash(int x,int y){unsigned h=(unsigned)x*374761393u+(unsigned)y*668265263u;h=(h^(h>>13))*1274126177u;return h^(h>>16);}
/* v31.22: effects follow the local level, so VARIED weather storms only
   where it is stormy; the presets give exactly the v31 values. */
float weatherStorm(const Flight *s){return weatherVaried(s->weather)?clampf(weatherLevel(s)-1,0,1):s->weather==2?1:0;}
float weatherFlash(const Flight *s){
 float k=weatherStorm(s);if(k<.5f)return 0;
 float t=fmodf(s->weatherTime+10,23);return (t<.10f?1:(t>.24f&&t<.32f?.65f:0))*fminf(1,(k-.5f)*2);
}
float weatherThunder(const Flight *s){
 float k=weatherStorm(s);if(k<.5f)return 0;
 float t=fmodf(s->weatherTime+10,23)-1.8f;
 return (t>=0&&t<3?(.25f+.75f*fminf(t*5,1))*(1-t/3):0)*fminf(1,(k-.5f)*2);
}
float weatherRain(const Flight *s){float k=weatherStorm(s);return k>0?clampf((3300-s->pos.z)/400,0,1)*k:0;}
int weatherPuffs(const Flight *s,WeatherPuff *out,int capacity){
 if(s->weather==0||!out||capacity<=0)return 0;
 if(weatherVaried(s->weather)){
  /* Each 2.5 km cell takes the level where it is: coverage rises from
     nothing (clear) through half (scattered) to all (storm); cells above
     1.5 are tall dark storm cells. */
  float drift=s->weatherTime*3,cx=floorf((s->pos.x-drift)/2500),cy=floorf(s->pos.y/2500);int count=0;
  for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
   int gx=(int)cx+x,gy=(int)cy+y;unsigned hash=cellHash(gx,gy);
   float east=(gx+.5f)*2500+drift,north=(gy+.5f)*2500;
   float level=weatherLevelAt(s->weather,(Vec){east,north,0},s->weatherTime);
   float cover=level<1?.5f*level:fminf(1,.5f+.5f*(level-1));
   if(level<.25f||((hash>>8)&1023)/1024.f>=cover)continue;
   float distance=hypotf(east-s->pos.x,north-s->pos.y),fade=clampf((3500-distance)/1200,0,1);if(fade==0)continue;
   int storm=level>1.5f;float base=fmaxf(3200,terrain(east,north)+700);
   for(int i=0;i<(storm?3:2)&&count<capacity;i++){
    float offset=i==1?280:i==2?-180:0;
    out[count++]=(WeatherPuff){{east+offset,north+offset*.4f,base+(storm?i*430:i*100)},storm?1200+i*180:650+i*180,storm?750:340,
     fade*(storm?.88f:.80f),storm?.40f+i*.13f:.94f};
   }
  }
  return count;
 }
 float drift=s->weatherTime*3,cx=floorf((s->pos.x-drift)/2500),cy=floorf(s->pos.y/2500);
 int count=0;
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  int gx=(int)cx+x,gy=(int)cy+y;unsigned hash=cellHash(gx,gy);
  if(s->weather==1&&(hash&1))continue;
  float east=(gx+.5f)*2500+drift,north=(gy+.5f)*2500;
  float distance=hypotf(east-s->pos.x,north-s->pos.y);
  /* Fade completely before a cell can leave the surrounding 3x3 window. */
  float fade=clampf((3500-distance)/1200,0,1);if(fade==0)continue;
  float base=fmaxf(3200,terrain(east,north)+700);
  int layers=s->weather==2?3:2;
  for(int i=0;i<layers&&count<capacity;i++){
   float offset=i==1?280:i==2?-180:0;
   out[count++]=(WeatherPuff){{east+offset,north+offset*.4f,base+(s->weather==2?i*430:i*100)},
    s->weather==2?1200+i*180:650+i*180,s->weather==2?750:340,
    fade*(s->weather==2?.88f:.80f),s->weather==2?.40f+i*.13f:.94f};
  }
 }
 return count;
}
