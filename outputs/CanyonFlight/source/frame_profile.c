#include "frame_profile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void recordFrameProfile(FrameProfile *p,float sec,float cpu,float gpu,int aircraft,int exterior,int paused,int stereo){
 if(!p||!isfinite(sec)||sec<=0)return;
 if(p->count&&(p->aircraft!=aircraft||p->exterior!=exterior||p->paused!=paused||p->stereo!=stereo))memset(p,0,sizeof(*p));
 p->aircraft=aircraft;p->exterior=exterior;p->paused=paused;p->stereo=stereo;
 if(p->count==FRAME_PROFILE_CAPACITY)p->sum-=p->data[p->next].frameMs;else p->count++;
 FrameSample sample={sec*1000,isfinite(cpu)?fmaxf(cpu,0):0,isfinite(gpu)?fmaxf(gpu,0):0};
 p->data[p->next]=sample;p->sum+=sample.frameMs;p->next=(p->next+1)%FRAME_PROFILE_CAPACITY;
}
static int compareFloat(const void *a,const void *b){float x=*(const float*)a,y=*(const float*)b;return (x>y)-(x<y);}
FrameStats frameProfileStats(const FrameProfile *p){
 FrameStats s={0};if(!p||!p->count||p->sum<=0)return s;
 float times[FRAME_PROFILE_CAPACITY];for(int i=0;i<p->count;i++)times[i]=p->data[i].frameMs;
 qsort(times,p->count,sizeof(float),compareFloat);s.samples=p->count;s.fps=p->count*1000/p->sum;
 s.p95Ms=times[(95*p->count+99)/100-1];s.maxMs=times[p->count-1];return s;
}
int saveFrameProfile(const FrameProfile *p,const char *path){
 if(!p||!p->count)return 0;
 char temp[256];if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp))return 0;
 FILE *f=fopen(temp,"w");if(!f)return 0;
 int ok=fputs("frame,frame_ms,cpu_ms,gpu_ms,aircraft,exterior,paused,stereo\n",f)>=0;
 int start=p->count==FRAME_PROFILE_CAPACITY?p->next:0;
 for(int i=0;i<p->count&&ok;i++){
  FrameSample s=p->data[(start+i)%FRAME_PROFILE_CAPACITY];
  ok=fprintf(f,"%d,%.4f,%.4f,%.4f,%d,%d,%d,%d\n",i,s.frameMs,s.cpuMs,s.gpuMs,p->aircraft,p->exterior,p->paused,p->stereo)>0;
 }
 if(fclose(f)!=0)ok=0;
 if(ok&&rename(temp,path)==0)return 1;
 remove(temp);return 0;
}
