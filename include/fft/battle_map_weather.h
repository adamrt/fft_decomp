#ifndef BATTLE_MAP_WEATHER_H
#define BATTLE_MAP_WEATHER_H

/* BATTLE map weather: rain and snow particles, lightning, and E458's timed particle transform. */

#include "fft/gfx.h"
#include "fft/map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* weather */
/*
 * Script variable 0x23, set by event instruction {3C} Weather.  The same
 * strength scale covers rain and snow; the map's 32-bit palette-mod flags says
 * which one is falling.  battle_map_get_effective_weather folds that flag in by
 * adding 3, so callers that compare against it see the snow values.
 */
typedef enum map_weather {
    BATTLE_WEATHER_NONE = 0,
    BATTLE_WEATHER_NONE_ALT = 1,
    BATTLE_WEATHER_RAIN = 2,
    BATTLE_WEATHER_STORM = 3,
    BATTLE_WEATHER_STRONG_STORM = 4,
    BATTLE_WEATHER_SNOW = 5,
    BATTLE_WEATHER_SNOWSTORM = 6,
    BATTLE_WEATHER_STRONG_SNOWSTORM = 7,
} map_weather_e;

/* Added to a rain value to give its snow counterpart. */
enum {
    BATTLE_WEATHER_SNOW_OFFSET = 3,
};

/* Bits of g_battle_map_weather_flags ("weather palette mods"). */
typedef enum map_weather_flags {
    MAP_WEATHER_FLAG_SNOW = 0x01,
    MAP_WEATHER_FLAG_IGNORE_WEATHER = 0x02,
} map_weather_flags_e;

/* The signed palette modifier shares the 32-bit value above its seven low flag bits. */
enum {
    MAP_WEATHER_MODIFIER_SHIFT = 7,
    MAP_WEATHER_LOW_FLAGS_MASK = 0x7f,
};

/* Commands of battle_map_run_weather_command and battle_map_init_weather_particles. */
typedef enum map_weather_command {
    MAP_WEATHER_COMMAND_SNOW = 0x53,
    MAP_WEATHER_COMMAND_CLEAR = 0x54,
    MAP_WEATHER_COMMAND_RAIN = 0x55,
    MAP_WEATHER_COMMAND_LIGHT_RAIN = 0x7c,
    MAP_WEATHER_COMMAND_HEAVY_RAIN = 0x7d,
    MAP_WEATHER_COMMAND_LIGHT_SNOW = 0x7e,
    MAP_WEATHER_COMMAND_HEAVY_SNOW = 0x7f,
    MAP_WEATHER_COMMAND_THUNDER = 0x89,              /* one lightning flash with its sound */
    MAP_WEATHER_COMMAND_LIGHTNING = 0x8a,            /* random lightning from now on */
    MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER = 0x8b, /* issue the commands for the weather script variable */
    MAP_WEATHER_COMMAND_RESTORE_LIGHTING = 0x8c,
    MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES = 0x8f, /* battle_map_init_weather_particles */
    MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES = 0x90,
} map_weather_command_e;

/* Stored renderer selector at g_battle_map_weather_effect_mode. The ordinary
 * modes reuse command IDs; 0x96 runs E458's timed particle transform. */
typedef enum map_weather_effect_mode {
    MAP_WEATHER_EFFECT_SNOW = MAP_WEATHER_COMMAND_SNOW,
    MAP_WEATHER_EFFECT_CLEAR = MAP_WEATHER_COMMAND_CLEAR,
    MAP_WEATHER_EFFECT_RAIN = MAP_WEATHER_COMMAND_RAIN,
    MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM = 0x96,
} map_weather_effect_mode_e;

/* Weather particles of g_battle_map_weather_particles: snow
 * (MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES) uses all 64, rain
 * (MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES) fills two sets of 32. */
typedef struct battle_weather_particle {
    s16 x;
    s16 y;
    s16 z;
    /* g_battle_map_weather_fall_speed, plus the sprite index for 0x8f; rain
     * reuses it as splash state (0/1/2) and the splash set as a countdown. */
    s16 fall_speed;
} battle_weather_particle_t;

/* map_weather_flags_e bits, with the signed palette modifier above MAP_WEATHER_MODIFIER_SHIFT. */
extern s32 g_battle_map_weather_flags;
extern s16 g_battle_map_weather_effect_mode; /* map_weather_effect_mode_e */
extern palette_t g_battle_map_weather_particle_palette;
extern u8 g_battle_map_weather_primary_speed;
extern u8 g_battle_map_weather_fall_speed;
extern u8 g_battle_map_rain_secondary_fall_speed;
extern battle_weather_particle_t g_battle_map_weather_particles[64];
extern battle_weather_particle_t g_battle_map_rain_splash_particles[32];
extern RECT g_battle_map_weather_drop_uv_rects[3];
extern RECT g_battle_map_weather_splash_uv_rects[4];
extern MATRIX g_battle_map_weather_matrix;
extern SVECTOR g_battle_map_weather_rotation;
extern VECTOR g_battle_map_weather_scale;
extern s32 g_battle_map_weather_sprite_scale;
extern VECTOR g_battle_map_weather_translation;

s32 battle_map_get_effective_weather(void);
void battle_map_run_weather_command(map_weather_command_e command);
s32 battle_map_stop_clut_cycles_and_weather_sfx(void);

/* lightning */
/* g_battle_map_lightning_mode. */
typedef enum map_lightning_mode {
    MAP_LIGHTNING_OFF = 0,
    MAP_LIGHTNING_RANDOM = 1,
    MAP_LIGHTNING_ONCE = 2, /* back to OFF when the flash ends */
} map_lightning_mode_e;

/* Lightning flash state (g_battle_map_lightning_state).
 * battle_map_set_light_matrices fills color_matrices[0] and [2]; the lightning weather commands
 * clear the rotation part of color_matrices[1] and start the 60-frame flash timer. */
typedef struct battle_lightning_state {
    u32 timer;                                                /* 0x00 */
    MATRIX color_matrices[3];                                 /* 0x04 */
    u8 _unused_64[0x20];                                      /* 0x64 */
    CVECTOR base_colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT]; /* 0x84 */
    CVECTOR colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT];      /* 0x8c */
} battle_lightning_state_t;
typedef char battle_lightning_state_size_must_be_148[(sizeof(battle_lightning_state_t) == 148) ? 1 : -1];

extern s32 g_battle_map_lightning_mode;
extern battle_lightning_state_t g_battle_map_lightning_state;

/* timed particle transform */
/* Frame, duration and rotation of E458's timed particle transform
 * (MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM). */
extern s32 g_battle_map_command_0x96_frame;
extern s32 g_battle_map_command_0x96_duration;
extern SVECTOR g_battle_map_command_0x96_rotation;

void battle_map_set_command_0x96_duration(s32 id, s32 value);

#endif
