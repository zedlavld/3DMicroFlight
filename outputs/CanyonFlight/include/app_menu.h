#ifndef APP_MENU_H
#define APP_MENU_H
enum {MENU_TITLE,MENU_SETUP,MENU_FLIGHT,MENU_PAUSE,MENU_EXIT,MENU_SAVE_SLOTS,MENU_LOAD_SLOTS,MENU_AUDIO};
enum {MENU_NONE,MENU_START_FLIGHT,MENU_RESUME,MENU_SAVE,MENU_LOAD,MENU_QUIT,MENU_CALIBRATE,MENU_SWITCH_AIRCRAFT,MENU_AUTOTEST};
/* v31.17 pause menu: resume, switch aircraft, save, load, new flight, title,
   audio & controls, exit. */
/* v31.1: MENU_AUDIO is the "Audio & controls" screen: volume, sound,
   calibrate stick, stick deadzone (percent, 0-15), back. */
#define MENU_DEADZONE_MAX 15
/* v35.15 autotestCases: cases in autotest.txt; the title menu then offers "Run autotest" (row 4). */
typedef struct {int screen,row,returnScreen,aircraft,airport,phase,weather,hasSave,slot,volume,muted,deadzone,timeOfDay,autotestCases;} AppMenu;
/* v34: setup rows are aircraft, airport, stage, weather, time of day
   (TIME_* in daylight.h), start flight, back. */
int menuRowHeight(const AppMenu *m);
int menuRows(const AppMenu *m);
void menuMove(AppMenu *m,int direction);
void menuAdjust(AppMenu *m,int direction);
int menuAccept(AppMenu *m);
void menuBack(AppMenu *m);
#endif
