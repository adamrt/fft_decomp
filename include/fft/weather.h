#ifndef WEATHER_H
#define WEATHER_H

/* Battle map weather: the weather values and commands, rain and snow particles, lightning, E458's timed particle
 * transform and the weather sounds. */

#include "fft/battle_map.h"
#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* weather */
/* Script variable 0x23, set by event instruction {3C} Weather. The same strength scale covers rain and snow; the
 * map's 32-bit palette-mod flags say which one is falling. weather_get_effective folds that flag in by adding 3, so
 * callers that compare against it see the snow values. */
typedef enum weather {
    WEATHER_NONE = 0,
    WEATHER_NONE_ALT = 1,
    WEATHER_RAIN = 2,
    WEATHER_STORM = 3,
    WEATHER_STRONG_STORM = 4,
    WEATHER_SNOW = 5,
    WEATHER_SNOWSTORM = 6,
    WEATHER_STRONG_SNOWSTORM = 7
} weather_e;

/* Added to a rain value to give its snow counterpart. */
enum { WEATHER_SNOW_OFFSET = 3 };

/* Bits of g_weather_flags ("weather palette mods"). */
typedef enum weather_flags {
    BATTLE_MAP_WEATHER_FLAG_SNOW = 0x01,
    BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER = 0x02
} weather_flags_e;

/* Commands of weather_run_command and weather_init_particles. */
typedef enum weather_command {
    BATTLE_MAP_WEATHER_COMMAND_SNOW = 0x53,
    BATTLE_MAP_WEATHER_COMMAND_CLEAR = 0x54,
    BATTLE_MAP_WEATHER_COMMAND_RAIN = 0x55,
    BATTLE_MAP_WEATHER_COMMAND_LIGHT_RAIN = 0x7c,
    BATTLE_MAP_WEATHER_COMMAND_HEAVY_RAIN = 0x7d,
    BATTLE_MAP_WEATHER_COMMAND_LIGHT_SNOW = 0x7e,
    BATTLE_MAP_WEATHER_COMMAND_HEAVY_SNOW = 0x7f,
    BATTLE_MAP_WEATHER_COMMAND_THUNDER = 0x89,              /* one lightning flash with its sound */
    BATTLE_MAP_WEATHER_COMMAND_LIGHTNING = 0x8a,            /* random lightning from now on */
    BATTLE_MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER = 0x8b, /* issue the commands for the weather script variable */
    BATTLE_MAP_WEATHER_COMMAND_RESTORE_LIGHTING = 0x8c,
    BATTLE_MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES = 0x8f, /* weather_init_particles */
    BATTLE_MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES = 0x90
} weather_command_e;

/* Stored renderer selector at g_weather_effect_mode. The ordinary
 * modes reuse command IDs; 0x96 runs E458's timed particle transform. */
typedef enum weather_effect_mode {
    BATTLE_MAP_WEATHER_EFFECT_SNOW = BATTLE_MAP_WEATHER_COMMAND_SNOW,
    BATTLE_MAP_WEATHER_EFFECT_CLEAR = BATTLE_MAP_WEATHER_COMMAND_CLEAR,
    BATTLE_MAP_WEATHER_EFFECT_RAIN = BATTLE_MAP_WEATHER_COMMAND_RAIN,
    BATTLE_MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM = 0x96
} weather_effect_mode_e;

/* weather_flags_e bits, with the signed palette modifier above BATTLE_MAP_WEATHER_MODIFIER_SHIFT. */
extern s32 g_weather_flags;
extern s16 g_weather_effect_mode; /* weather_effect_mode_e */

s32 weather_get_effective(void);

/* A byte-valued weather_command_e, also read from the event command table. */
void weather_run_command(u8 command);

/* timed particle transform */
/* Frame, duration and rotation of E458's timed particle transform
 * (BATTLE_MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM). */
extern s32 g_weather_timed_transform_frame;
extern s32 g_weather_timed_transform_duration;

void weather_set_timed_transform_duration(s32 id, s32 value);

/* sound */
/* Sound IDs remembered for weather_stop_sfx; zero means no ID is stored.
 * The rain ID is also retained while weather sounds are disabled. */
extern s32 g_weather_rain_sfx_id;
extern s32 g_weather_thunder_a_sfx_id;
extern s32 g_weather_thunder_b_sfx_id;
extern u8 g_weather_sfx_enabled;

void weather_start_sfx(void);
s32 weather_stop_sfx(void); /* the result is indeterminate; no caller reads it */

/* palette modifier encoding */
/* The signed palette modifier shares the 32-bit value above its seven low flag bits. */
enum { BATTLE_MAP_WEATHER_MODIFIER_SHIFT = 7, BATTLE_MAP_WEATHER_LOW_FLAGS_MASK = 0x7f };

#endif
