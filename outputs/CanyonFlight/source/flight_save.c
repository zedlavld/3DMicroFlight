#include "flight_save.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "airport_index.h"
typedef struct {Flight flight;View view;} SavedState;
typedef struct {char magic[8];uint32_t version,size,checksum,reserved;} SaveHeader;
static uint32_t checksum(const void *data,size_t n){
 const unsigned char *p=data;uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h;
}
static int valid(const SavedState *p){
 const Flight *s=&p->flight;const View *v=&p->view;
 /* Float members are checked explicitly; do not alias integer fields as floats. */
 const float values[]={s->pos.x,s->pos.y,s->pos.z,s->vel.x,s->vel.y,s->vel.z,s->wind.x,s->wind.y,s->wind.z,
 s->roll,s->pitch,s->yaw,s->p,s->q,s->r,s->throttle,s->engine,s->trim,s->flap,s->qw,s->qx,s->qy,s->qz,s->mass,s->isaOffset,
 s->flapActual,s->gearActual,s->elevator,s->aileron,s->rudder,s->aileronTrim,s->rudderTrim,s->ias,s->alpha,s->beta,
 s->loadFactor,s->stallWarning,s->thrust,s->weatherTime,s->brakeLeft,s->brakeRight,s->groundLoad,s->touchdownSink,
 v->insideYaw,v->insidePitch,v->orbitYaw,v->orbitPitch,v->distance};
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++)if(!isfinite(values[i]))return 0;
 float q=s->qw*s->qw+s->qx*s->qx+s->qy*s->qy+s->qz*s->qz;
 return s->aircraft>=0&&s->aircraft<AIRCRAFT_COUNT&&s->airport>=0&&s->airport<airportCount&&airports[s->airport].primary&&s->startPhase>=0&&s->startPhase<PHASE_COUNT&&
 s->weather>=0&&s->weather<WEATHER_VARIED+WEATHER_SEEDS&&s->gear>=0&&s->gear<=1&&s->crashed>=0&&s->crashed<=1&&
 s->engineCutoff>=0&&s->engineCutoff<=1&&s->onGround>=0&&s->onGround<=1&&
 s->mass>100&&s->mass<10000&&q>.99f&&q<1.01f&&fabsf(s->pos.x)<20000000&&fabsf(s->pos.y)<20000000&&fabsf(s->pos.z)<100000&&
 s->throttle>=0&&s->throttle<=1&&s->engine>=0&&s->engine<=1&&s->flap>=0&&s->flap<=1&&s->flapActual>=0&&s->flapActual<=1&&
 s->gearActual>=0&&s->gearActual<=1&&v->exterior>=0&&v->exterior<=1&&v->distance>=5&&v->distance<=200;
}
/* v33.01 appended Flight.speedbrake/spoilerActual. A v32 save (shorter
   Flight, then View) still loads; the new fields start at zero. */
#define FLIGHT_V32_BYTES offsetof(Flight,speedbrake)
#define STATE_V32_BYTES (FLIGHT_V32_BYTES+sizeof(View))
/* v1.1p20: the start airport is a runway-record index, and the card's records (index 49 on)
   can move when scenery/airports_fly.bin is rebuilt or be missing. The header's reserved word
   (0 in older saves) holds the airport id's hash; a record that no longer matches it is found
   again by the hash, else the nearest airport to the saved position is used. */
static void resolveAirport(SavedState *p,uint32_t hash){
 Flight *s=&p->flight;
 int ok=s->airport>=0&&s->airport<airportCount&&airports[s->airport].primary;
 if(ok&&(!hash||airportIdHash(airports[s->airport].id)==hash))return;
 if(!hash&&s->airport<AIRPORT_COUNT)return;   /* old save: valid() decides */
 int found=hash?airportFindHash(hash):-1;
 if(found<0&&isfinite(s->pos.x)&&isfinite(s->pos.y))found=airportNearest(s->pos.x,s->pos.y,0);
 if(found>=0)s->airport=found;
}
static int readState(const char *path,SavedState *p){
 FILE *f=fopen(path,"rb");if(!f)return 0;SaveHeader h;
 int ok=fread(&h,1,sizeof(h),f)==sizeof(h)&&!memcmp(h.magic,"CFLTSAVE",8)&&h.version==1&&(h.size==sizeof(*p)||h.size==STATE_V32_BYTES);
 if(ok&&h.size==sizeof(*p)){ok=fread(p,1,sizeof(*p),f)==sizeof(*p)&&fgetc(f)==EOF&&h.checksum==checksum(p,sizeof(*p));if(ok)resolveAirport(p,h.reserved);ok=ok&&valid(p);}
 else if(ok){
  static unsigned char old[STATE_V32_BYTES];
  ok=fread(old,1,sizeof(old),f)==sizeof(old)&&fgetc(f)==EOF&&h.checksum==checksum(old,sizeof(old));
  if(ok){memset(p,0,sizeof(*p));memcpy(&p->flight,old,FLIGHT_V32_BYTES);memcpy(&p->view,old+FLIGHT_V32_BYTES,sizeof(View));resolveAirport(p,h.reserved);ok=valid(p);}
 }
 if(fclose(f)!=0)ok=0;
 return ok;
}
int loadFlightState(const char *path,Flight *s,View *v){
 char backup[256];if(snprintf(backup,sizeof(backup),"%s.bak",path)>=(int)sizeof(backup))return 0;
 SavedState p;if(!readState(path,&p)&&!readState(backup,&p))return 0;
 *s=p.flight;*v=p.view;return 1;
}
int saveFlightState(const char *path,const Flight *s,const View *v){
 char temp[256],backup[256];if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)||snprintf(backup,sizeof(backup),"%s.bak",path)>=(int)sizeof(backup))return 0;
 SavedState p;memset(&p,0,sizeof(p));p.flight=*s;p.view=*v;if(!valid(&p))return 0;
 SaveHeader h={{'C','F','L','T','S','A','V','E'},1,sizeof(p),checksum(&p,sizeof(p)),airportIdHash(airports[s->airport].id)};
 FILE *f=fopen(temp,"wb");if(!f)return 0;
 int ok=fwrite(&h,1,sizeof(h),f)==sizeof(h)&&fwrite(&p,1,sizeof(p),f)==sizeof(p);
 if(fclose(f)!=0)ok=0;
 SavedState verify;if(ok)ok=readState(temp,&verify);
 if(!ok){remove(temp);return 0;}
 /* Only replace the backup with a verified primary. A corrupt primary must
    not destroy the last recoverable save. */
 SavedState previous;int hadPrimary=readState(path,&previous);
 if(hadPrimary){remove(backup);if(rename(path,backup)!=0){remove(temp);return 0;}}
 else remove(path);
 if(rename(temp,path)!=0){if(hadPrimary)rename(backup,path);remove(temp);return 0;}
 return 1;
}
