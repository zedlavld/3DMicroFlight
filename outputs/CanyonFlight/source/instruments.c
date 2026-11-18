#include "instruments.h"
#define PI 3.14159265359f
static float degrees(float radians){float value=fmodf(radians*180/PI+360,360);return value<0?value+360:value;}
PfdData readPfd(const Flight *s){
 Vec air={s->vel.x-s->wind.x,s->vel.y-s->wind.y,s->vel.z-s->wind.z};
 const Airport *ap=&airports[s->airport];float east=ap->east-s->pos.x,north=ap->north-s->pos.y;
 float course=ap->heading*PI/180,cross=(s->pos.x-ap->east)*cosf(course)-(s->pos.y-ap->north)*sinf(course);
 float along=(s->pos.x-ap->east)*sinf(course)+(s->pos.y-ap->north)*cosf(course);
 PfdData d={0};d.tas=sqrtf(air.x*air.x+air.y*air.y+air.z*air.z)/.514444444f;
 d.groundSpeed=hypotf(s->vel.x,s->vel.y)/.514444444f;
 d.heading=degrees(s->yaw);d.track=d.groundSpeed>.5f?degrees(atan2f(s->vel.x,s->vel.y)):d.heading;
 d.altitude=s->pos.z*3.2808399f;d.verticalSpeed=s->vel.z*196.850394f;
 d.windSpeed=hypotf(s->wind.x,s->wind.y)/.514444444f;
 d.windFrom=d.windSpeed>.1f?degrees(atan2f(-s->wind.x,-s->wind.y)):0;
 /* Specific lateral acceleration drives the slip/skid indicator; no gravity
    term is added because a real inclinometer senses apparent lateral load. */
 float speed2=air.x*air.x+air.y*air.y+air.z*air.z;
 float lateral=.5f*atmosphere(s->pos.z,s->isaOffset).density*speed2*aircraft[s->aircraft].area*(aircraft[s->aircraft].cyb*s->beta+aircraft[s->aircraft].cydr*s->rudder);
 d.slip=clampf(-lateral/(s->mass*9.80665f)*4,-1,1);
 d.turnRate=(s->q*sinf(s->roll)+s->r*cosf(s->roll))/fmaxf(cosf(s->pitch),.1f)*180/PI;
 d.bearing=degrees(atan2f(east,north));d.distance=hypotf(east,north)/1852;
 d.course=ap->heading;d.cdi=clampf(-cross/1852,-1,1);d.to=along<=0;return d;
}

float pfdAirspeedLimit(const Flight *s){
 const Aircraft *a=&aircraft[s->aircraft];Atmosphere atm=atmosphere(s->pos.z,s->isaOffset);
 float limit=a->mmo>0?fminf(a->vmo,calibratedAirspeed(atm.sound*a->mmo,atm)):a->vmo;
 if(a->gearLimit>0&&s->gearActual>.05f)limit=fminf(limit,a->gearLimit);
 if(s->flapActual>.01f)limit=fminf(limit,s->flapActual>.51f?a->flapLimit[1]:a->flapLimit[0]);
 return limit;
}
