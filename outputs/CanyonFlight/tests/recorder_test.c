#include "flight_recorder.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
int checks,failures,firstFailure,outputLength;char outputCsv[300000];
static char buffer[300000];static int length,failOpen,failWrite,failRename,removals;
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
FILE *fopen(const char *p,const char *m){length=0;return failOpen?NULL:(FILE*)buffer;}
int fputs(const char *s,FILE *f){if(failWrite)return EOF;int n=strlen(s);if(length+n>=(int)sizeof(buffer))return EOF;memcpy(buffer+length,s,n);length+=n;return n;}
int fprintf(FILE *f,const char *fmt,...){if(failWrite)return -1;va_list args;va_start(args,fmt);int n=vsnprintf(buffer+length,sizeof(buffer)-length,fmt,args);va_end(args);if(n<0||n>=(int)sizeof(buffer)-length)return -1;length+=n;return n;}
int fclose(FILE *f){return 0;}int rename(const char *a,const char *b){return failRename?-1:0;}int remove(const char *p){removals++;return 0;}
int main(void){
 Flight s={0};s.aircraft=1;s.startPhase=PHASE_APPROACH;s.pos=(Vec){1234,-55,3000};s.vel=(Vec){4,60,-2};s.wind=(Vec){2,3,1};s.ias=123.4f;s.pitch=.1f;s.roll=-.2f;s.yaw=1;s.mass=2517.4377f;s.brakeLeft=.25f;s.brakeRight=.75f;s.onGround=1;
 s.isaOffset=20;s.thrust=1234.5f;s.p=.12f;s.q=-.23f;s.r=.34f;s.elevator=.9f;s.trim=.3f;
 flightRecorderBegin(&s);CHECK(flightRecorderActive()&&flightRecorderCount()==1);
 for(int i=0;i<1200;i++)flightRecorderStep(&s,1/120.f);
 CHECK(flightRecorderCount()==51);flightRecorderStop();
 for(int i=0;i<1200;i++)flightRecorderStep(&s,1/120.f);CHECK(flightRecorderCount()==51);
 failOpen=1;CHECK(!flightRecorderSave("test.csv")&&flightRecorderCount()==51);failOpen=0;
 failWrite=1;CHECK(!flightRecorderSave("test.csv")&&flightRecorderCount()==51);CHECK(removals==1);failWrite=0;
 failRename=1;CHECK(!flightRecorderSave("test.csv")&&flightRecorderCount()==51);CHECK(removals==2);failRename=0;
 CHECK(flightRecorderSave("test.csv")&&flightRecorderCount()==0);outputLength=length;memcpy(outputCsv,buffer,length);
 flightRecorderBegin(&s);for(int i=0;i<200000;i++)flightRecorderStep(&s,1/120.f);
 CHECK(!flightRecorderActive()&&flightRecorderCount()==FLIGHT_RECORD_CAPACITY);
 flightRecorderBegin(&s);CHECK(flightRecorderActive()&&flightRecorderCount()==1);
 return failures;
}
