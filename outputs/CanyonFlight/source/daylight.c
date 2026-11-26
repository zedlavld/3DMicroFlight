#include <math.h>
#include <time.h>
#include "daylight.h"
#include "scenery.h"
const char *const timeSettingNames[TIME_SETTING_COUNT]={"DAWN","MORNING","NOON","DUSK","NIGHT","3DS CLOCK"};
/* v1.1p17: double. A 1/120 s step is 2.3e-6 h, under a float's step near 16-40 h (the +24
   wrap), so in float the clock stalled: a 55 min flight left at 16:30 still landed at 16:30. */
static double hours=12;
float timeSettingHours(int setting){
 static const float fixed[TIME_CLOCK]={6.35f,9.5f,12,17.25f,21.5f};
 if(setting>=0&&setting<TIME_CLOCK)return fixed[setting];
 time_t now=time(NULL);struct tm *t=gmtime(&now); /* the 3DS clock holds local time */
 return t?t->tm_hour+t->tm_min/60.f:12;
}
void daylightSet(float h){double x=fmod((double)h,24);if(x<0)x+=24;hours=x;}
float daylightHours(void){return (float)hours;}
void daylightAdvance(float seconds){if(seconds>0&&seconds<10){hours+=seconds/3600.0;if(hours>=24)hours-=24;}}
float daylightSunElevation(void){
 /* 34.5 N, declination -18 (mid November): sunrise ~6:20, sunset ~17:10 solar. */
 const float lat=34.5f*3.14159265f/180,dec=-18*3.14159265f/180;
 float hourAngle=(hours-12)*15*3.14159265f/180;
 float s=sinf(lat)*sinf(dec)+cosf(lat)*cosf(dec)*cosf(hourAngle);
 return asinf(s)*180/3.14159265f;
}
static float smooth(float a,float b,float x){float t=(x-a)/(b-a);t=t<0?0:t>1?1:t;return t*t*(3-2*t);}
static void mix(float out[3],const float a[3],const float b[3],float t){for(int i=0;i<3;i++)out[i]=a[i]+(b[i]-a[i])*t;}
void daylightTint(float rgb[3]){
 static const float day[3]={1,1,1},golden[3]={1,.80f,.60f},twilight[3]={.42f,.38f,.52f},night[3]={.20f,.23f,.36f};
 float e=daylightSunElevation();
 if(e>=18){mix(rgb,day,day,0);return;}
 if(e>=2){mix(rgb,golden,day,smooth(2,18,e));return;}
 if(e>=-4){mix(rgb,twilight,golden,smooth(-4,2,e));return;}
 mix(rgb,night,twilight,smooth(-12,-4,e));
}
float daylightNight(void){return 1-smooth(-6,1,daylightSunElevation());}
float daylightLampLevel(float night){return night<.03f?0:fminf(1,.7f+.4f*night);}
void daylightSky(float elevationSine,float rgb[3]){
 float tint[3];daylightTint(tint);scenerySkyColor(elevationSine,rgb);
 for(int i=0;i<3;i++)rgb[i]*=tint[i];
 /* Warm band low in the sky around sunset/sunrise, strongest at the horizon. */
 float e=daylightSunElevation(),glow=smooth(-9,-1,e)*(1-smooth(4,12,e));
 float low=1-smooth(0,.35f,elevationSine<0?0:elevationSine);
 static const float warm[3]={.55f,.24f,.08f};
 for(int i=0;i<3;i++){rgb[i]+=warm[i]*glow*low;if(rgb[i]>1)rgb[i]=1;}
}
