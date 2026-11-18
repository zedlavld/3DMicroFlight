/* Scripted test runs; see include/autotest.h. No 3DS dependencies, so the
   parser also builds on the host. */
#include "autotest.h"
#include "flight.h"
#include "maneuver.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
static int startIndex(const char *icao){
 for(int k=0;k<AIRPORT_START_COUNT;k++)if(!strcasecmp(airports[airportStarts[k]].id,icao))return airportStarts[k];
 return -1;
}
static int airportIndex(const char *icao){
 for(int i=0;i<AIRPORT_COUNT;i++)if(!strcasecmp(airports[i].id,icao)&&airports[i].primary)return i;
 return -1;
}
static int phaseIndex(const char *name){
 /* v33: underscores stand for spaces (SHORT_FINAL, SLOW_FLIGHT, ENGINE-OUT_GLIDE). */
 char spaced[24];snprintf(spaced,sizeof(spaced),"%s",name);for(char *c=spaced;*c;c++)if(*c=='_')*c=' ';
 for(int p=0;p<PHASE_COUNT;p++)if(!strcasecmp(flightPhaseName(p),spaced))return p;
 return -1;
}
int autotestLoad(const char *path,AutotestCase *cases,int max,int *rejected){
 FILE *f=fopen(path,"r");if(!f)return 0;
 char line[256];int n=0,bad=0;
 while(fgets(line,sizeof(line),f)){
  char *hash=strchr(line,'#');if(hash)*hash=0;
  char name[32],craft[8],icao[8],stage[24],view[24],dest[16]="";int render,shots;float warmup,interval;
  int got=sscanf(line,"%31s %7s %7s %23s %23s %d %f %f %d %15s",name,craft,icao,stage,view,&render,&warmup,&interval,&shots,dest);
  if(got<=0)continue;                      /* blank or comment */
  if(n>=max){bad++;continue;}              /* past the case limit: reported, not silently dropped */
  AutotestCase c;memset(&c,0,sizeof(c));
  snprintf(c.name,sizeof(c.name),"%s",name);
  c.aircraft=-1;for(int k=0;k<AIRCRAFT_COUNT;k++)if(!strcasecmp(craft,aircraft[k].name))c.aircraft=k;
  c.airport=startIndex(icao);c.phase=phaseIndex(stage);
  char *suffix=strchr(view,':');c.weather=0;c.hours=12;int timeOk=1;
  if(suffix){*suffix++=0;c.weather=!strcasecmp(suffix,"clear")?0:!strcasecmp(suffix,"cloud")?1:!strcasecmp(suffix,"storm")?2:-1;
   /* v31.22: varied (seed 1) or variedN (seed N) */
   if(!strncasecmp(suffix,"varied",6)){int seed=suffix[6]?atoi(suffix+6):1;c.weather=seed>=0&&seed<WEATHER_SEEDS?WEATHER_VARIED+seed:-1;}}
  char *clock=strchr(view,'+');if(clock){*clock++=0;char *end;c.hours=strtof(clock,&end);timeOk=end!=clock&&!*end&&c.hours>=0&&c.hours<=24;}
  char *look=strchr(view,'@');float lookYaw=0,lookPitch=0;int lookOk=1;
  if(look){*look++=0;lookOk=sscanf(look,"%f/%f",&lookYaw,&lookPitch)==2;}
  c.lookYaw=lookYaw*3.14159265f/180;c.lookPitch=lookPitch*3.14159265f/180;
  c.exterior=!strcasecmp(view,"ext")||!strcasecmp(view,"exterior");
  int viewOk=c.exterior||!strcasecmp(view,"cockpit");
  c.renderTest=render;c.warmup=warmup;c.interval=interval;c.shots=shots;
  if(got==10&&!strncasecmp(dest,"AT",2)){float lat,lon;
   /* the Sedona-centred frame of tools/build_west_v35.py */
   if(sscanf(dest+2,"%f,%f",&lat,&lon)==2){const double m=6371000*3.14159265358979/180;
    c.atNorth=(float)((lat-34.848598480225)*m);c.atEast=(float)((lon+111.78800201416)*m*cos(34.848598480225*3.14159265358979/180));c.destination=AUTOTEST_AT_POSITION;}
   else c.destination=-1;}
  else c.destination=got==10?(!strcasecmp(dest,"HOLD")?AUTOTEST_PLAYER_HOLD:!strcasecmp(dest,"PANEL")?AUTOTEST_PLAYER_PANEL:!strcasecmp(dest,"CLIMB")?AUTOTEST_PLAYER_CLIMB:maneuverIndex(dest)?AUTOTEST_MANEUVER_BASE-maneuverIndex(dest):airportIndex(dest)):-1;
  if((got!=9&&got!=10)||(got==10&&c.destination>-2&&(c.destination<0||c.destination==c.airport))||!viewOk||!lookOk||!timeOk||c.weather<0||c.aircraft<0||c.airport<0||c.phase<0||render<0||render>15||warmup<0||interval<0||shots<1||shots>60){bad++;continue;}
  cases[n++]=c;
 }
 fclose(f);
 if(rejected)*rejected=bad;
 return n;
}
