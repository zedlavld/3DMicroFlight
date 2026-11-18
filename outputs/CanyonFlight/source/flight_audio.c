#include <3ds.h>
#include <string.h>
#include "flight_audio.h"
#include "audio_synth.h"
#include "weather_visual.h"
static ndspWaveBuf wave[3];static int16_t *pcm;static Thread worker;
static LightLock lock;static int ready,quitting;static AudioParams params;static AudioSynth synth;
static void audioWorker(void *unused){
 (void)unused;
 while(1){
  AudioParams p;LightLock_Lock(&lock);p=params;int stop=quitting;LightLock_Unlock(&lock);if(stop)break;
  for(int i=0;i<3;i++)if(wave[i].status==NDSP_WBUF_FREE||wave[i].status==NDSP_WBUF_DONE){
   audioSynthRender(&synth,&p,wave[i].data_pcm16,AUDIO_BLOCK);
   DSP_FlushDataCache(wave[i].data_pcm16,AUDIO_BLOCK*sizeof(int16_t));ndspChnWaveBufAdd(0,&wave[i]);
  }
  svcSleepThread(4000000);
 }
}
int initFlightAudio(void){
 if(R_FAILED(ndspInit()))return 0;
 pcm=linearAlloc(3*AUDIO_BLOCK*sizeof(int16_t));if(!pcm){ndspExit();return 0;}
 memset(wave,0,sizeof(wave));memset(pcm,0,3*AUDIO_BLOCK*sizeof(int16_t));memset(&params,0,sizeof(params));
 audioSynthInit(&synth);LightLock_Init(&lock);quitting=0;
 ndspSetOutputMode(NDSP_OUTPUT_STEREO);ndspChnReset(0);ndspChnSetInterp(0,NDSP_INTERP_LINEAR);
 ndspChnSetRate(0,AUDIO_RATE);ndspChnSetFormat(0,NDSP_FORMAT_MONO_PCM16);
 float mix[12]={1,1};ndspChnSetMix(0,mix);
 for(int i=0;i<3;i++){wave[i].data_pcm16=pcm+i*AUDIO_BLOCK;wave[i].nsamples=AUDIO_BLOCK;}
 worker=threadCreate(audioWorker,NULL,16384,0x28,-2,false);
 if(!worker){ndspChnWaveBufClear(0);ndspExit();linearFree(pcm);pcm=NULL;return 0;}
 ready=1;return 1;
}
void setFlightAudio(const Flight *s,int playing,int exterior,float volume,int muted){
 if(!ready)return;
 AudioParams p={aircraft[s->aircraft].engine==ENGINE_TURBOFAN,playing&&!s->crashed,s->engineCutoff,exterior,s->engine,s->ias,s->stallWarning,muted?0:volume,weatherThunder(s),weatherRain(s)};
 LightLock_Lock(&lock);params=p;LightLock_Unlock(&lock);
}
void freeFlightAudio(void){
 if(!ready)return;
 LightLock_Lock(&lock);quitting=1;LightLock_Unlock(&lock);threadJoin(worker,U64_MAX);threadFree(worker);worker=NULL;
 ndspChnWaveBufClear(0);ndspChnReset(0);ndspExit();linearFree(pcm);pcm=NULL;ready=0;
}
