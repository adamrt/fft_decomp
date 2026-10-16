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
    WEATHER_STRONG_SNOWSTORM = 7,
} weather_e;

/* Added to a rain value to give its snow counterpart. */
enum {
    WEATHER_SNOW_OFFSET = 3,
};

/* Bits of g_weather_flags ("weather palette mods"). */
typedef enum weather_flags {
    BATTLE_MAP_WEATHER_FLAG_SNOW = 0x01,
    BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER = 0x02,
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
    BATTLE_MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES = 0x90,
} weather_command_e;

/* Stored renderer selector at g_weather_effect_mode. The ordinary
 * modes reuse command IDs; 0x96 runs E458's timed particle transform. */
typedef enum weather_effect_mode {
    BATTLE_MAP_WEATHER_EFFECT_SNOW = BATTLE_MAP_WEATHER_COMMAND_SNOW,
    BATTLE_MAP_WEATHER_EFFECT_CLEAR = BATTLE_MAP_WEATHER_COMMAND_CLEAR,
    BATTLE_MAP_WEATHER_EFFECT_RAIN = BATTLE_MAP_WEATHER_COMMAND_RAIN,
    BATTLE_MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM = 0x96,
} weather_effect_mode_e;

/* weather_flags_e bits, with the signed palette modifier above BATTLE_MAP_WEATHER_MODIFIER_SHIFT. */
extern s32 g_weather_flags;
extern s16 g_weather_effect_mode; /* weather_effect_mode_e */

s32 weather_get_effective(void);
void weather_run_command(weather_command_e command);

/* particles */
/* Weather particles of g_weather_particles: snow
 * (BATTLE_MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES) uses all 64, rain
 * (BATTLE_MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES) fills two sets of 32. */
typedef struct weather_particle {
    s16 x;
    s16 y;
    s16 z;
    /* g_weather_fall_speed, plus the sprite index for 0x8f; rain
     * reuses it as splash state (0/1/2) and the splash set as a countdown. */
    s16 fall_speed;
} weather_particle_t;

extern weather_particle_t g_weather_particles[64];
extern weather_particle_t g_weather_rain_splash_particles[32];
extern palette_t g_weather_particle_palette;
extern RECT g_weather_drop_uv_rects[3];
extern RECT g_weather_splash_uv_rects[4];
extern u8 g_weather_primary_speed;
extern u8 g_weather_fall_speed;
extern u8 g_weather_rain_secondary_fall_speed;
extern MATRIX g_weather_matrix;
extern SVECTOR g_weather_rotation;
extern VECTOR g_weather_scale;
extern s32 g_weather_sprite_scale;
extern VECTOR g_weather_translation;

/* lightning */
/* Values of g_weather_lightning_mode. */
typedef enum weather_lightning_mode {
    BATTLE_MAP_LIGHTNING_OFF = 0,
    BATTLE_MAP_LIGHTNING_RANDOM = 1,
    BATTLE_MAP_LIGHTNING_ONCE = 2, /* back to OFF when the flash ends */
} weather_lightning_mode_e;

/* Lightning flash state (g_weather_lightning_state).
 * battle_map_light_set_matrices fills color_matrices[0] and [2]; the lightning weather commands
 * clear the rotation part of color_matrices[1] and start the 60-frame flash timer. */
typedef struct weather_lightning_state {
    u32 timer;                                                       /* 0x00 */
    MATRIX color_matrices[3];                                        /* 0x04 */
    u8 _unused_64[0x20];                                             /* 0x64 */
    CVECTOR base_colors[BATTLE_MAP_BACKGROUND_GRADIENT_COLOR_COUNT]; /* 0x84 */
    CVECTOR colors[BATTLE_MAP_BACKGROUND_GRADIENT_COLOR_COUNT];      /* 0x8c */
} weather_lightning_state_t;
typedef char weather_lightning_state_size_must_be_148[(sizeof(weather_lightning_state_t) == 148) ? 1 : -1];

extern s32 g_weather_lightning_mode;
extern weather_lightning_state_t g_weather_lightning_state;

/* timed particle transform */
/* Frame, duration and rotation of E458's timed particle transform
 * (BATTLE_MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM). */
extern s32 g_weather_timed_transform_frame;
extern s32 g_weather_timed_transform_duration;
extern SVECTOR g_weather_timed_transform_rotation;
extern s32 g_weather_timed_transform_unread; /* weather_set_timed_transform_duration stores 0x95; nothing reads it */

void weather_set_timed_transform_duration(s32 id, s32 value);

/* sound */
/* The rain sound (primary) and the two thunder sounds, 0 when none plays. */
extern s32 g_weather_primary_sfx_id;
extern s32 g_weather_secondary_sfx_id;
extern s32 g_weather_tertiary_sfx_id;
extern u8 g_weather_sfx_enabled;

void weather_start_sfx(void);
s32 weather_stop_sfx(void); /* the result is indeterminate; no caller reads it */

/* palette modifier encoding */
/* The signed palette modifier shares the 32-bit value above its seven low flag bits. */
enum {
    BATTLE_MAP_WEATHER_MODIFIER_SHIFT = 7,
    BATTLE_MAP_WEATHER_LOW_FLAGS_MASK = 0x7f,
};

#endif
