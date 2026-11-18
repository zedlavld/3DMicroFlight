#ifndef AUDIO_SYNTH_H
#define AUDIO_SYNTH_H
#include <stdint.h>
#define AUDIO_RATE 22050
#define AUDIO_BLOCK 512
/* aircraft: 1 = turbofan sound, 0 = piston (v33: engine type, not index). */
typedef struct {int aircraft,running,cutoff,exterior;float engine,ias,stall,volume,thunder,rain;} AudioParams;
typedef struct {int16_t sine[1024];uint32_t phase[3],noise;float engineGain,windGain,hornGain,frequency,rumble,air,jetBlend,thunderGain,rainGain;} AudioSynth;
void audioSynthInit(AudioSynth *s);
void audioSynthRender(AudioSynth *s,const AudioParams *p,int16_t *samples,int count);
#endif
