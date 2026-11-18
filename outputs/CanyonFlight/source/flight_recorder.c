/* Bounded RAM recorder. Sampling never touches the SD card; save explicitly
   while paused, or once when leaving the application. */
#include "flight_recorder.h"
#include <stdio.h>
#include <string.h>
typedef struct {float time;Vec pos,vel,wind;float ias,roll,pitch,yaw,alpha,beta,load,warning,throttle,engine,flap,gear,mass,elevator,aileron,rudder,brakeLeft,brakeRight,isaOffset,thrust,p,q,r,effectiveElevator;int aircraft,profile,ground,crashed,cutoff;} FlightSample;
static FlightSample samples[FLIGHT_RECORD_CAPACITY];static int active,count;static double clockTime;static float untilSample;
static void sample(const Flight *s){
 if(count>=FLIGHT_RECORD_CAPACITY){active=0;return;}
 samples[count++]=(FlightSample){clockTime,s->pos,s->vel,s->wind,s->ias,s->roll,s->pitch,s->yaw,s->alpha,s->beta,s->loadFactor,s->stallWarning,s->throttle,s->engine,s->flapActual,s->gearActual,s->mass,s->elevator,s->aileron,s->rudder,s->brakeLeft,s->brakeRight,s->isaOffset,s->thrust,s->p,s->q,s->r,fmaxf(-1,fminf(1,s->elevator+s->trim)),s->aircraft,s->startPhase,s->onGround,s->crashed,s->engineCutoff};
 if(count==FLIGHT_RECORD_CAPACITY)active=0;
}
void flightRecorderBegin(const Flight *s){count=0;clockTime=0;untilSample=.2f;active=1;sample(s);}
void flightRecorderStep(const Flight *s,float dt){if(!active||dt<=0)return;clockTime+=dt;untilSample-=dt;if(untilSample<=.00001f){sample(s);untilSample+=.2f;}}
void flightRecorderStop(void){active=0;}
int flightRecorderActive(void){return active;}
int flightRecorderCount(void){return count;}
int flightRecorderSave(const char *path){
 flightRecorderStop();if(!count)return 1;
 char temporary[256];if(snprintf(temporary,sizeof(temporary),"%s.tmp",path)>=(int)sizeof(temporary))return 0;
 FILE *file=fopen(temporary,"w");if(!file)return 0;
 int valid=fputs("model_version,time_s,aircraft,start_profile,east_m,north_m,altitude_m,ve_mps,vn_mps,vu_mps,wind_e_mps,wind_n_mps,wind_u_mps,ias_approx_kcas,roll_deg,pitch_deg,heading_true_deg,alpha_deg,beta_deg,air_load_g,stall_warning,throttle,engine,flaps,gear,mass_kg,elevator,aileron,rudder,brake_left,brake_right,on_ground,crashed,engine_cutoff,isa_offset_c,thrust_n,p_rad_s,q_rad_s,r_rad_s,effective_elevator\n",file)>=0;
 for(int i=0;i<count&&valid;i++){
  const FlightSample *s=&samples[i];
  valid=fprintf(file,"29,%.3f,%d,%d,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.3f,%d,%d,%d,%.3f,%.3f,%.5f,%.5f,%.5f,%.4f\n",s->time,s->aircraft,s->profile,s->pos.x,s->pos.y,s->pos.z,s->vel.x,s->vel.y,s->vel.z,s->wind.x,s->wind.y,s->wind.z,s->ias,s->roll*57.29578f,s->pitch*57.29578f,s->yaw*57.29578f,s->alpha*57.29578f,s->beta*57.29578f,s->load,s->warning,s->throttle,s->engine,s->flap,s->gear,s->mass,s->elevator,s->aileron,s->rudder,s->brakeLeft,s->brakeRight,s->ground,s->crashed,s->cutoff,s->isaOffset,s->thrust,s->p,s->q,s->r,s->effectiveElevator)>0;
 }
 if(fclose(file)!=0)valid=0;
 if(valid&&rename(temporary,path)==0){count=0;return 1;}
 remove(temporary);return 0;
}
