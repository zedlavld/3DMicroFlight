#include "audio_synth.h"
#include <string.h>
#define LENGTH (AUDIO_RATE*2)
int checks,failures,firstFailure;int16_t previews[6][LENGTH];
static AudioSynth a,b;static int16_t scratch[LENGTH],tiny[16];
#define CHECK(x) do{checks++;if(!(x)){failures++;if(!firstFailure)firstFailure=__LINE__;}}while(0)
int main(void){
 for(int mode=0;mode<6;mode++){
  AudioParams p={.running=1,.exterior=1,.engine=(mode&1)?1:.05f,.ias=mode<4?100:180,.volume=.6f};
  p.aircraft=mode>=2&&mode<4;p.cutoff=mode>=4;p.stall=mode==5?1:0;
  audioSynthInit(&a);audioSynthRender(&a,&p,previews[mode],LENGTH);
  int peak=0;long long energy=0;
  for(int i=0;i<LENGTH;i++){int n=previews[mode][i];if(n<0)n=-n;if(n>peak)peak=n;energy+=(long long)n*n;}
  CHECK(peak>100&&peak<31130&&energy>100000);
  /* Queue blocks must reproduce one continuous call, without phase resets. */
  audioSynthInit(&b);for(int i=0;i<LENGTH;i+=512){int n=LENGTH-i;if(n>512)n=512;audioSynthRender(&b,&p,scratch+i,n);}
  CHECK(!memcmp(scratch,previews[mode],sizeof(scratch)));
  p.volume=0;for(int i=0;i<20;i++)audioSynthRender(&a,&p,scratch,512);
  for(int i=0;i<512;i++)CHECK(scratch[i]==0);
  p.volume=.6f;p.running=0;audioSynthRender(&a,&p,scratch,512);
  for(int i=0;i<512;i++)CHECK(scratch[i]==0);
 }
 for(int type=0;type<2;type++){
  AudioParams loud={.aircraft=type,.running=1,.exterior=1,.engine=1,.ias=300,.stall=1,.volume=1,.thunder=1,.rain=1};
  audioSynthInit(&a);audioSynthRender(&a,&loud,scratch,LENGTH);int clipped=0;
  for(int i=0;i<LENGTH;i++)if(scratch[i]>=31130||scratch[i]<=-31130)clipped++;
  CHECK(clipped==0);
 }
 AudioParams weather={.running=1,.cutoff=1,.volume=.8f,.thunder=1,.rain=1};
 audioSynthInit(&a);audioSynthRender(&a,&weather,scratch,LENGTH);
 long long weatherEnergy=0;for(int i=0;i<LENGTH;i++)weatherEnergy+=(long long)scratch[i]*scratch[i];CHECK(weatherEnergy>100000);
 weather.volume=0;for(int i=0;i<20;i++)audioSynthRender(&a,&weather,scratch,512);
 for(int i=0;i<512;i++)CHECK(scratch[i]==0);
 audioSynthInit(&a);AudioParams p={.running=1,.volume=0};memset(tiny,0x7f,sizeof(tiny));
 audioSynthRender(&a,&p,tiny,7);for(int i=0;i<7;i++)CHECK(tiny[i]==0);CHECK(tiny[7]==0x7f7f);
 audioSynthRender(&a,&p,tiny,0);CHECK(tiny[7]==0x7f7f);
 return failures;
}
