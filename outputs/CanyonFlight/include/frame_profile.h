#ifndef FRAME_PROFILE_H
#define FRAME_PROFILE_H
#define FRAME_PROFILE_CAPACITY 300
typedef struct {float frameMs,cpuMs,gpuMs;} FrameSample;
typedef struct {float fps,p95Ms,maxMs;int samples;} FrameStats;
typedef struct {FrameSample data[FRAME_PROFILE_CAPACITY];int next,count,aircraft,exterior,paused,stereo;double sum;} FrameProfile;
void recordFrameProfile(FrameProfile *p,float seconds,float cpuMs,float gpuMs,int aircraft,int exterior,int paused,int stereo);
FrameStats frameProfileStats(const FrameProfile *p);
int saveFrameProfile(const FrameProfile *p,const char *path);
#endif
