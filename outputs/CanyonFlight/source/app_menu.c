#include "app_menu.h"
#include "flight.h"
#include "daylight.h"
int menuRows(const AppMenu *m){return m->screen==MENU_TITLE?4+(m->autotestCases>0):m->screen==MENU_SETUP?7:m->screen==MENU_PAUSE?8:m->screen==MENU_EXIT?2:m->screen==MENU_AUDIO?5:(m->screen==MENU_SAVE_SLOTS||m->screen==MENU_LOAD_SLOTS)?4:0;}
int menuRowHeight(const AppMenu *m){return m->screen==MENU_PAUSE?19:25;}
void menuMove(AppMenu *m,int d){int n=menuRows(m);if(n)m->row=(m->row+d+n)%n;}
void menuAdjust(AppMenu *m,int d){
 if(m->screen==MENU_AUDIO){
  if(m->row==0){m->volume+=d;if(m->volume<0)m->volume=0;if(m->volume>5)m->volume=5;}
  else if(m->row==1)m->muted=!m->muted;
  else if(m->row==3){m->deadzone+=d;if(m->deadzone<0)m->deadzone=0;if(m->deadzone>MENU_DEADZONE_MAX)m->deadzone=MENU_DEADZONE_MAX;}
  return;
 }
 if(m->screen!=MENU_SETUP)return;
 int *value=0,n=0;
 /* Airport values are runway-record indices; step between start runways. */
 if(m->row==1){m->airport=nextAirportStart(m->airport,d);return;}
 if(m->row==0){value=&m->aircraft;n=AIRCRAFT_COUNT;}
 else if(m->row==2){value=&m->phase;n=PHASE_COUNT;}else if(m->row==3){value=&m->weather;n=4;}else if(m->row==4){value=&m->timeOfDay;n=TIME_SETTING_COUNT;}
 if(value)*value=(*value+d+n)%n;
}
static void confirmExit(AppMenu *m){m->returnScreen=m->screen;m->screen=MENU_EXIT;m->row=0;}
int menuAccept(AppMenu *m){
 if(m->screen==MENU_TITLE){
  if(m->row==0){m->returnScreen=MENU_TITLE;m->screen=MENU_SETUP;m->row=0;}
  else if(m->row==1&&m->hasSave){m->returnScreen=MENU_TITLE;m->screen=MENU_LOAD_SLOTS;m->row=0;}
  else if(m->row==2){m->returnScreen=MENU_TITLE;m->screen=MENU_AUDIO;m->row=0;}
  else if(m->row==3)confirmExit(m);
  else if(m->row==4&&m->autotestCases>0)return MENU_AUTOTEST; /* v35.15 */
 }else if(m->screen==MENU_SETUP){
  if(m->row<5)menuAdjust(m,1);
  else if(m->row==5)return MENU_START_FLIGHT;
  else {menuBack(m);}
 }else if(m->screen==MENU_PAUSE){
  if(m->row==0)return MENU_RESUME;
  if(m->row==1)return MENU_SWITCH_AIRCRAFT;
  if(m->row==2){m->returnScreen=MENU_PAUSE;m->screen=MENU_SAVE_SLOTS;m->row=0;return MENU_NONE;}
  if(m->row==3&&m->hasSave){m->returnScreen=MENU_PAUSE;m->screen=MENU_LOAD_SLOTS;m->row=0;return MENU_NONE;}
  if(m->row==4){m->returnScreen=MENU_PAUSE;m->screen=MENU_SETUP;m->row=0;}
  if(m->screen==MENU_PAUSE&&m->row==5){m->screen=MENU_TITLE;m->row=0;}
  if(m->screen==MENU_PAUSE&&m->row==6){m->returnScreen=MENU_PAUSE;m->screen=MENU_AUDIO;m->row=0;}
  if(m->screen==MENU_PAUSE&&m->row==7)confirmExit(m);
 }else if(m->screen==MENU_AUDIO){
  if(m->row==0)m->volume=(m->volume+1)%6;else if(m->row==1)m->muted=!m->muted;
  else if(m->row==2)return MENU_CALIBRATE;else if(m->row==3)m->deadzone=(m->deadzone+1)%(MENU_DEADZONE_MAX+1);else menuBack(m);
 }else if(m->screen==MENU_SAVE_SLOTS||m->screen==MENU_LOAD_SLOTS){
  if(m->row==3){menuBack(m);return MENU_NONE;}
  m->slot=m->row;
  if(m->screen==MENU_SAVE_SLOTS)return MENU_SAVE;
  if(m->hasSave&(1<<m->slot))return MENU_LOAD;
 }else if(m->screen==MENU_EXIT){
  if(m->row==1)return MENU_QUIT;
  m->screen=m->returnScreen;m->row=0;
 }
 return MENU_NONE;
}
void menuBack(AppMenu *m){
 if(m->screen==MENU_FLIGHT){m->screen=MENU_PAUSE;m->row=0;}
 else if(m->screen==MENU_PAUSE)m->screen=MENU_FLIGHT;
 else if(m->screen==MENU_SETUP){m->screen=m->returnScreen==MENU_PAUSE?MENU_PAUSE:MENU_TITLE;m->row=0;}
 else if(m->screen==MENU_AUDIO||m->screen==MENU_EXIT||m->screen==MENU_SAVE_SLOTS||m->screen==MENU_LOAD_SLOTS){m->screen=m->returnScreen;m->row=0;}
}
