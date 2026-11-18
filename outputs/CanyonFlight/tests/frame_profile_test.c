#include "frame_profile.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
int checks,failures,firstFailure,outputLength;char outputCsv[40000];
static FrameProfile profile;static char buffer[40000];static int length,failOpen,failWrite,failRename;
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
FILE *fopen(const char *p,const char *m){length=0;return failOpen?NULL:(FILE*)buffer;}
int fputs(const char *s,FILE *f){if(failWrite)return EOF;int n=strlen(s);if(length+n>=sizeof(buffer))return EOF;memcpy(buffer+length,s,n);length+=n;return n;}
int fprintf(FILE *f,const char *fmt,...){if(failWrite)return -1;va_list args;va_start(args,fmt);int n=vsnprintf(buffer+length,sizeof(buffer)-length,fmt,args);va_end(args);if(n<0||n>=sizeof(buffer)-length)return -1;length+=n;return n;}
int fclose(FILE *f){return 0;}int rename(const char *a,const char *b){return failRename?-1:0;}int remove(const char *p){return 0;}
int main(void){
 CHECK(frameProfileStats(&profile).samples==0);
 for(int i=0;i<600;i++)recordFrameProfile(&profile,1/60.f,4,6,0,0,0,0);
 FrameStats s=frameProfileStats(&profile);CHECK(s.samples==300&&fabsf(s.fps-60)<.001f&&fabsf(s.p95Ms-16.66667f)<.001f);
 recordFrameProfile(&profile,.25f,4,6,0,0,0,0);s=frameProfileStats(&profile);
 CHECK(fabsf(s.maxMs-250)<.001f&&fabsf(s.fps-57.32484f)<.001f);
 recordFrameProfile(&profile,NAN,0,0,0,0,0,0);recordFrameProfile(&profile,-1,0,0,0,0,0,0);CHECK(profile.count==300);
 recordFrameProfile(&profile,.02f,0,0,1,0,0,0);CHECK(profile.count==1);
 recordFrameProfile(&profile,.02f,0,0,1,1,0,0);CHECK(profile.count==1);
 recordFrameProfile(&profile,.02f,0,0,1,1,1,0);CHECK(profile.count==1);
 recordFrameProfile(&profile,.02f,0,0,1,1,1,1);CHECK(profile.count==1);
 memset(&profile,0,sizeof(profile));for(int i=1;i<=350;i++)recordFrameProfile(&profile,i/1000.f,2,3,0,0,0,0);
 CHECK(profile.count==300);CHECK(saveFrameProfile(&profile,"frame.csv"));outputLength=length;memcpy(outputCsv,buffer,length);
 failOpen=1;CHECK(!saveFrameProfile(&profile,"frame.csv"));failOpen=0;
 failWrite=1;CHECK(!saveFrameProfile(&profile,"frame.csv"));failWrite=0;
 failRename=1;CHECK(!saveFrameProfile(&profile,"frame.csv"));failRename=0;
 CHECK(profile.count==300);
 return failures;
}
