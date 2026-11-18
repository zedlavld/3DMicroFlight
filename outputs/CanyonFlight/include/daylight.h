#ifndef DAYLIGHT_H
#define DAYLIGHT_H
/* v34: time of day. One scene-wide light colour (sun height from the local
   solar time at the Arizona/Nevada latitude in mid-November) tints terrain,
   aircraft, clouds and sky; night brings runway edge lights and aircraft
   navigation lights. The clock runs with flight time. */
enum {TIME_DAWN,TIME_MORNING,TIME_NOON,TIME_DUSK,TIME_NIGHT,TIME_CLOCK,TIME_SETTING_COUNT};
extern const char *const timeSettingNames[TIME_SETTING_COUNT];
/* Local hours for a menu setting; TIME_CLOCK reads the 3DS clock. */
float timeSettingHours(int setting);
void daylightSet(float hours);
float daylightHours(void);
void daylightAdvance(float seconds);
/* Sun elevation in degrees (negative below the horizon). */
float daylightSunElevation(void);
/* Scene light colour, 0..1 per channel (1,1,1 at full day). */
void daylightTint(float rgb[3]);
/* 0 in daylight .. 1 once lights are needed (sun below about -2 degrees). */
float daylightNight(void);
/* v34.03: lamp brightness for a night factor: 0 until night passes .03 (sun about half a
   degree up),
   then at least .7, so lamps switched on at dusk are never darker than the
   ground around them. */
float daylightLampLevel(float night);
/* Sky colour for a view direction with the given elevation sine, already
   tinted (blue by day, warm horizon glow at twilight, dark blue at night). */
void daylightSky(float elevationSine,float rgb[3]);
#endif
