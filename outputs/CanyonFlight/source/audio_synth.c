#include "audio_synth.h"
#include <math.h>
#include <string.h>
static float bound(float x,float lo,float hi){return fmaxf(lo,fminf(hi,x));}
static float tone(const AudioSynth *s,uint32_t phase){return s->sine[phase>>22]/32768.f;}
void audioSynthInit(AudioSynth *s){
 memset(s,0,sizeof(*s));s->noise=0x53463233u;s->frequency=50;
 for(int i=0;i<1024;i++)s->sine[i]=(int16_t)(sinf(i*6.28318530718f/1024)*32760);
}
void audioSynthRender(AudioSynth *s,const AudioParams *p,int16_t *out,int count){
 if(!s||!p||!out||count<=0)return;
 float volume=p->running?bound(p->volume,0,1):0,engine=bound(p->engine,0,1);
 float speed=bound(p->ias/200,0,1.5f),outside=p->exterior?1:.63f;
 float eg=volume*outside*(p->cutoff?0:(.08f+.18f*engine));
 float wg=volume*(p->exterior?.09f:.035f)*speed*speed;
 float hg=volume*(p->stall>.5f?.18f:0);
 float tg=volume*.5f*bound(p->thunder,0,1),rg=volume*.035f*bound(p->rain,0,1);
 if(eg==0&&wg==0&&hg==0&&tg==0&&rg==0&&fabsf(s->engineGain)+fabsf(s->windGain)+fabsf(s->hornGain)+fabsf(s->thunderGain)+fabsf(s->rainGain)<.00001f){
  s->engineGain=s->windGain=s->hornGain=s->thunderGain=s->rainGain=0;memset(out,0,count*sizeof(*out));return;
 }
 float target=p->aircraft?280+1100*engine:50+85*sqrtf(engine);
 const float phaseScale=4294967296.f/AUDIO_RATE;
 for(int i=0;i<count;i++){
  s->engineGain+=(eg-s->engineGain)*.004f;s->windGain+=(wg-s->windGain)*.004f;s->hornGain+=(hg-s->hornGain)*.004f;
  s->thunderGain+=(tg-s->thunderGain)*.004f;s->rainGain+=(rg-s->rainGain)*.004f;
  s->frequency+=(target-s->frequency)*.0008f;
  s->phase[0]+=(uint32_t)(s->frequency*phaseScale);
  s->phase[1]+=(uint32_t)((p->aircraft?45+80*engine:s->frequency*.5f)*phaseScale);
  s->phase[2]+=(uint32_t)(850*phaseScale);
  uint32_t n=s->noise;n^=n<<13;n^=n>>17;n^=n<<5;s->noise=n;
  float noise=(int32_t)n/2147483648.f;s->rumble+=(noise-s->rumble)*.035f;s->air+=(noise-s->air)*.32f;
  float jet=.38f*tone(s,s->phase[0])+.12f*tone(s,s->phase[0]*2u)+.30f*tone(s,s->phase[1])+2*s->rumble;
  float piston=.65f*tone(s,s->phase[0])+.22f*tone(s,s->phase[0]*2u)+.13f*tone(s,s->phase[0]*3u)+.12f*tone(s,s->phase[1]);
  s->jetBlend+=((p->aircraft?1.f:0)-s->jetBlend)*.004f;
  float motor=piston+(jet-piston)*s->jetBlend;
  float sample=s->engineGain*motor+s->windGain*s->air+s->hornGain*tone(s,s->phase[2])+s->thunderGain*2*s->rumble+s->rainGain*s->air;
  out[i]=(int16_t)(bound(sample,-.95f,.95f)*32767);
 }
}
