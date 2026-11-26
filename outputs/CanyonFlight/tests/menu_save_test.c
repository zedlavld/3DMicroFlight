#include "app_menu.h"
#include "daylight.h"
#include "flight_save.h"
#include <stdio.h>
#include <string.h>
int checks,failures,firstFailure;
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
/* In-memory filesystem models short writes, rename failures and bad files. */
typedef struct {unsigned char data[2048];size_t size,pos;int exists;} MemFile;
static MemFile files[9];static int shortWrite,failCommit,failOpen,failClose;
static int indexOf(const char *p){int slot=strstr(p,"flight2")?1:strstr(p,"flight3")?2:0;return slot*3+(strstr(p,".tmp")?1:strstr(p,".bak")?2:0);}
FILE *fopen(const char *p,const char *mode){
 if(failOpen)return NULL;
 MemFile *f=&files[indexOf(p)];
 if(mode[0]=='w'){f->exists=1;f->size=0;}else if(!f->exists)return NULL;
 f->pos=0;return (FILE*)f;
}
size_t fwrite(const void *p,size_t n,size_t count,FILE *file){
 MemFile *f=(MemFile*)file;size_t total=n*count;if(shortWrite&&total)total--;
 if(f->pos+total>sizeof(f->data))return 0;
 memcpy(f->data+f->pos,p,total);f->pos+=total;f->size=f->pos;return total/n;
}
size_t fread(void *p,size_t n,size_t count,FILE *file){
 MemFile *f=(MemFile*)file;size_t total=n*count;if(total>f->size-f->pos)total=f->size-f->pos;
 memcpy(p,f->data+f->pos,total);f->pos+=total;return total/n;
}
int fgetc(FILE *file){MemFile *f=(MemFile*)file;return f->pos<f->size?f->data[f->pos++]:EOF;}
int fclose(FILE *file){return failClose?-1:0;}
int remove(const char *p){MemFile *f=&files[indexOf(p)];if(!f->exists)return -1;f->exists=0;return 0;}
int rename(const char *a,const char *b){
 int i=indexOf(a),j=indexOf(b);if(failCommit&&i%3==1&&j%3==0)return -1;
 if(!files[i].exists||files[j].exists)return -1;
 files[j]=files[i];files[i].exists=0;return 0;
}
int main(void){
 AppMenu m={.screen=MENU_TITLE};CHECK(menuRows(&m)==4);
 menuMove(&m,-1);CHECK(m.row==3);menuAccept(&m);CHECK(m.screen==MENU_EXIT&&m.row==0);
 CHECK(menuAccept(&m)==MENU_NONE&&m.screen==MENU_TITLE);m.row=1;CHECK(menuAccept(&m)==MENU_NONE&&m.screen==MENU_TITLE);
 m.hasSave=1;CHECK(menuAccept(&m)==MENU_NONE&&m.screen==MENU_LOAD_SLOTS);CHECK(menuAccept(&m)==MENU_LOAD);menuBack(&m);m.row=0;menuAccept(&m);CHECK(m.screen==MENU_SETUP);
 menuAdjust(&m,-1);CHECK(m.aircraft==AIRCRAFT_COUNT-1);m.row=1;menuAdjust(&m,-1);CHECK(m.airport==airportStarts[AIRPORT_START_COUNT-1]);
 /* v31: forward steps visit every start runway once, skipping secondaries. */
 for(int k=0;k<AIRPORT_START_COUNT;k++){menuAdjust(&m,1);CHECK(m.airport==airportStarts[k]&&airports[m.airport].primary);}
 m.airport=airportStarts[AIRPORT_START_COUNT-1];
 m.row=2;menuAdjust(&m,-1);CHECK(m.phase==PHASE_COUNT-1);m.row=3;menuAdjust(&m,-1);CHECK(m.weather==3);
 m.row=4;menuAdjust(&m,-1);CHECK(m.timeOfDay==TIME_SETTING_COUNT-1);menuAdjust(&m,1);CHECK(m.timeOfDay==0);
 m.row=5;CHECK(menuAccept(&m)==MENU_START_FLIGHT);menuBack(&m);CHECK(m.screen==MENU_TITLE);
 m.screen=MENU_FLIGHT;menuBack(&m);CHECK(m.screen==MENU_PAUSE);m.row=1;CHECK(menuAccept(&m)==MENU_SWITCH_AIRCRAFT&&m.screen==MENU_PAUSE);m.row=2;CHECK(menuAccept(&m)==MENU_NONE&&m.screen==MENU_SAVE_SLOTS);CHECK(menuAccept(&m)==MENU_SAVE);menuBack(&m);
 m.row=3;CHECK(menuAccept(&m)==MENU_NONE&&m.screen==MENU_LOAD_SLOTS);CHECK(menuAccept(&m)==MENU_LOAD);menuBack(&m);m.row=4;menuAccept(&m);m.row=6;menuAccept(&m);CHECK(m.screen==MENU_PAUSE);
 m.row=7;menuAccept(&m);m.row=1;CHECK(menuAccept(&m)==MENU_QUIT);menuBack(&m);CHECK(m.screen==MENU_PAUSE);
 m.row=0;CHECK(menuAccept(&m)==MENU_RESUME);
 CHECK(menuRows(&m)==8&&menuRowHeight(&m)==19);
 m.row=6;menuAccept(&m);CHECK(m.screen==MENU_AUDIO);m.row=0;menuAdjust(&m,-1);CHECK(m.volume==0);
 menuAdjust(&m,1);CHECK(m.volume==1);m.row=1;menuAccept(&m);CHECK(m.muted);menuBack(&m);CHECK(m.screen==MENU_PAUSE);
 m.screen=MENU_TITLE;m.row=2;menuAccept(&m);CHECK(m.screen==MENU_AUDIO);
 /* v31.1 Audio & controls: row 2 asks main to calibrate the stick, row 3 adjusts the deadzone
    within 0..MENU_DEADZONE_MAX; v1.1p18 row 4 lower screen, v1.1p21 row 5 easy mode, row 6 Back. */
 CHECK(menuRows(&m)==7&&menuRowHeight(&m)==19);m.row=2;CHECK(menuAccept(&m)==MENU_CALIBRATE&&m.screen==MENU_AUDIO);
 m.row=3;m.deadzone=0;menuAdjust(&m,-1);CHECK(m.deadzone==0);menuAdjust(&m,1);CHECK(m.deadzone==1);
 m.deadzone=MENU_DEADZONE_MAX;menuAdjust(&m,1);CHECK(m.deadzone==MENU_DEADZONE_MAX);
 menuAccept(&m);CHECK(m.deadzone==0&&m.screen==MENU_AUDIO);
 m.row=4;m.lowerMap=0;menuAccept(&m);CHECK(m.lowerMap==1&&m.screen==MENU_AUDIO);
 m.row=5;m.level=0;menuAdjust(&m,-1);CHECK(m.level==0);menuAdjust(&m,1);CHECK(m.level==1);m.level=FLIGHT_LEVELS-1;menuAdjust(&m,1);CHECK(m.level==FLIGHT_LEVELS-1);
 menuAccept(&m);CHECK(m.level==0&&m.screen==MENU_AUDIO);
 m.row=6;menuAccept(&m);CHECK(m.screen==MENU_TITLE);

 for(int type=0;type<2;type++)for(int airport=0;airport<3;airport++)for(int phase=0;phase<PHASE_COUNT;phase++){
  Flight s={0},loaded={0};View v={.exterior=1,.distance=45,.orbitYaw=.2f},restored={0};
  s.aircraft=type;s.airport=airport;s.startPhase=phase;s.mass=type?2721.554f:1542.214f;s.qw=1;s.pos=(Vec){123,456,3000};s.vel.y=60;s.throttle=.7f;s.engine=.65f;s.flap=.5f;s.flapActual=.3f;s.weather=2;s.weatherTime=15;
  CHECK(saveFlightState("flight.sav",&s,&v));CHECK(loadFlightState("flight.sav",&loaded,&restored));
  CHECK(!memcmp(&s,&loaded,sizeof(s))&&!memcmp(&v,&restored,sizeof(v)));
 }
 Flight s={0},loaded={0};View v={.distance=32},restored={0};s.mass=1542;s.qw=1;
 CHECK(saveFlightState("flight.sav",&s,&v));s.pos.x=100;
 CHECK(saveFlightState("flight.sav",&s,&v));files[0].data[30]^=128;
 CHECK(loadFlightState("flight.sav",&loaded,&restored)&&loaded.pos.x==0); /* backup */
 failOpen=1;CHECK(!saveFlightState("flight.sav",&s,&v));failOpen=0;
 shortWrite=1;CHECK(!saveFlightState("flight.sav",&s,&v));shortWrite=0;
 CHECK(loadFlightState("flight.sav",&loaded,&restored)&&loaded.pos.x==0);
 CHECK(saveFlightState("flight.sav",&s,&v));s.pos.x=200;failCommit=1;
 CHECK(!saveFlightState("flight.sav",&s,&v));failCommit=0;
 CHECK(loadFlightState("flight.sav",&loaded,&restored)&&loaded.pos.x==100);
 failClose=1;CHECK(!saveFlightState("flight.sav",&s,&v));failClose=0;
 CHECK(loadFlightState("flight.sav",&loaded,&restored)&&loaded.pos.x==100);
 files[0].size=3;files[2].size=3;loaded.pos.x=777;
 CHECK(!loadFlightState("flight.sav",&loaded,&restored)&&loaded.pos.x==777);
 /* Three independent slots, including backups and cancelled selection. */
 s.vel.x=0;s.pos.x=11;CHECK(saveFlightState("flight.sav",&s,&v));
 s.pos.x=22;CHECK(saveFlightState("flight2.sav",&s,&v));
 s.pos.x=33;CHECK(saveFlightState("flight3.sav",&s,&v));
 CHECK(loadFlightState("flight.sav",&loaded,&restored)&&loaded.pos.x==11);
 CHECK(loadFlightState("flight2.sav",&loaded,&restored)&&loaded.pos.x==22);
 CHECK(loadFlightState("flight3.sav",&loaded,&restored)&&loaded.pos.x==33);
 s.pos.x=44;CHECK(saveFlightState("flight2.sav",&s,&v));files[3].data[30]^=128;
 CHECK(loadFlightState("flight2.sav",&loaded,&restored)&&loaded.pos.x==22);
 CHECK(loadFlightState("flight3.sav",&loaded,&restored)&&loaded.pos.x==33);
 for(int slot=0;slot<3;slot++){
  m=(AppMenu){.screen=MENU_PAUSE,.row=2,.hasSave=1<<slot};menuAccept(&m);m.row=slot;
  CHECK(menuAccept(&m)==MENU_SAVE&&m.slot==slot);menuBack(&m);CHECK(m.screen==MENU_PAUSE);
  m.row=3;menuAccept(&m);CHECK(menuRows(&m)==4);m.row=slot;
  CHECK(menuAccept(&m)==MENU_LOAD&&m.slot==slot);m.row=(slot+1)%3;CHECK(menuAccept(&m)==MENU_NONE);
  menuBack(&m);CHECK(m.screen==MENU_PAUSE);
 }
 m=(AppMenu){.screen=MENU_TITLE,.row=1,.hasSave=4};menuAccept(&m);menuBack(&m);CHECK(m.screen==MENU_TITLE);
 s.qw=0;CHECK(!saveFlightState("flight.sav",&s,&v));s.qw=1;s.vel.x=NAN;CHECK(!saveFlightState("flight.sav",&s,&v));
 return failures;
}
