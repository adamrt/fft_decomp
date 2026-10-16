#ifndef BATTLE_MAP_LIGHT_H
#define BATTLE_MAP_LIGHT_H

/* BATTLE map lighting: light matrices, background gradient, ambient and darkness colors, and the back color. */

#include "fft/battle_gfx.h"
#include "fft/gfx.h"
#include "fft/map.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* light matrices */
/* Commands of battle_map_light_matrix_command. A START queues its STEP in
 * g_battle_map_light_transition_command, which the map draw loop issues every frame. */
typedef enum map_light_matrix_command {
    MAP_LIGHT_MATRIX_COMMAND_SET = 0x5e,
    MAP_LIGHT_MATRIX_COMMAND_SAVE = 0x5f,
    MAP_LIGHT_MATRIX_COMMAND_RESTORE = 0x60,
    MAP_LIGHT_MATRIX_COMMAND_GET_COLOR = 0x61,
    MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT = 0x62,
    MAP_LIGHT_MATRIX_COMMAND_START_BLEND = 0x66, /* towards the given matrices over the duration */
    MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND = 0x67,
    MAP_LIGHT_MATRIX_COMMAND_START_DELTA = 0x68, /* add a frame-scaled delta */
    MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA = 0x69,
} map_light_matrix_command_e;

extern MATRIX g_battle_map_light_color_matrix;
extern MATRIX g_battle_map_light_matrix;
extern SVECTOR g_battle_map_light_rotation;
extern MATRIX g_battle_map_light_rotation_matrix;
extern MATRIX g_battle_map_local_light_matrix;
extern u16 g_battle_map_light_transition_command;               /* pending per-frame light command, 0 when idle */
extern u16 g_battle_map_light_transition_duration;              /* transition duration in frames */
extern u16 g_battle_map_light_transition_frame;                 /* transition frame counter */
extern MATRIX g_battle_map_light_transition_start_color_matrix; /* transition start color matrix */
extern MATRIX g_battle_map_light_transition_start_light_matrix; /* transition start light matrix */
extern MATRIX
    g_battle_map_light_transition_target_color_matrix; /* transition target (0x66) or delta (0x68) color matrix */
extern MATRIX
    g_battle_map_light_transition_target_light_matrix; /* transition target (0x66) or delta (0x68) light matrix */
extern MATRIX g_battle_map_saved_light_color_matrix;   /* saved color matrix (command 0x5f/0x60) */
extern MATRIX g_battle_map_saved_light_matrix;         /* saved light matrix (command 0x5f/0x60) */

MATRIX* battle_map_light_matrix_command(map_light_matrix_command_e command, MATRIX* color, MATRIX* light, s32 duration);

/* map colors */
/* Commands for the map's background gradient, ambient and darkness colors. */
typedef enum map_light_command {
    MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT = 0x56,
    MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT = 0x57,
    MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT = 0x58,
    MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT = 0x59,
    MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR = 0x5a,
    MAP_LIGHT_COMMAND_SAVE_AMBIENT_COLOR = 0x5b,
    MAP_LIGHT_COMMAND_RESTORE_AMBIENT_COLOR = 0x5c,
    MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR = 0x5d,
    MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR = 0x63,
    MAP_LIGHT_COMMAND_RESET_DARKNESS_COLOR = 0x64,
    MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR = 0x65,
} map_light_command_e;

/* Runtime color fades stepped by battle_map_update_animations:
 * g_battle_map_background_gradient_transition, g_battle_map_ambient_light_transition and
 * g_battle_map_darkness_transition. Channels are 16.16 fixed point;
 * the byte fields follow the updater's use. */
typedef struct map_color_transition_channels {
    s32 red;
    s32 green;
    s32 blue;
    s32 red_step;
    s32 green_step;
    s32 blue_step;
} map_color_transition_channels_t;
typedef char map_color_transition_channels_size_must_be_24[(sizeof(map_color_transition_channels_t) == 24) ? 1 : -1];

typedef struct map_color_transition {
    u8 active;
    u8 phase;
    u8 tick; /* 0x02; frame counter, reset when it reaches period >> 2 */
    u8 period;
    u8 repeat;            /* 0x04; map_color_repeat_e */
    rgb24_t repeat_color; /* 0x05 */
    map_color_transition_channels_t channels;
} map_color_transition_t;
typedef char map_color_transition_size_must_be_32[(sizeof(map_color_transition_t) == 32) ? 1 : -1];

/* The background gradient fade: one channel set per gradient color. colors[]
 * at 0x38 is the applied gradient pair;
 * battle_map_blend_background_gradient_color indexes it from the same base. */
typedef struct map_gradient_transition {
    u8 active;
    u8 phase;
    u8 tick; /* 0x02; frame counter, reset when it reaches period >> 2 */
    u8 period;
    u8 repeat;            /* 0x04; map_color_repeat_e */
    rgb24_t repeat_color; /* 0x05 */
    map_color_transition_channels_t channels[2];
    CVECTOR colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT];
} map_gradient_transition_t;
typedef char map_gradient_transition_size_must_be_64[(sizeof(map_gradient_transition_t) == 64) ? 1 : -1];

/* The applied background gradient pair. The copies in battle_map_light_state_command
 * move each color as an independent unaligned 32-bit value (lwl/lwr + swl/swr). */
extern map_background_gradient_colors_t g_battle_map_background_gradient_colors;
extern CVECTOR g_battle_map_ambient_light_color;
extern CVECTOR g_battle_map_darkness_color;
extern map_gradient_transition_t g_battle_map_background_gradient_transition;
extern map_color_transition_t g_battle_map_ambient_light_transition;
extern map_color_transition_t g_battle_map_darkness_transition;
extern rgb24_t g_battle_map_ambient_polygon_color;
extern battle_render_buffer_t* g_battle_map_ambient_restored_buffer;
extern CVECTOR g_battle_map_current_darkness_color;
extern CVECTOR g_battle_map_saved_background_gradient_first;
extern CVECTOR g_battle_map_saved_background_gradient_second;

void battle_map_init_ambient_light(void);
void battle_map_init_background_gradient(void);
void battle_map_init_darkness(void);
void* battle_map_light_state_command(map_light_command_e command, void* data);
void battle_map_start_darkness_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_color_field(s32 mode, s32 amount, s16 red, s16 green, s16 blue);
void battle_map_modify_background_gradient(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_set_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors);
void battle_map_start_ambient_light_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_set_darkness_color(s32 frame_duration, const CVECTOR* color);
void battle_map_transition_and_store_background_gradient(
    s32 frame_duration, const map_background_gradient_colors_t* colors);

/* back color */
extern rgb24_t g_battle_map_background_color_backup;
extern s32 g_battle_map_back_color_red;
extern s32 g_battle_map_back_color_green;
extern s32 g_battle_map_back_color_blue;
extern u8 g_battle_map_back_color_disabled;
extern CVECTOR g_battle_map_cached_back_color;
extern s32 g_battle_map_saved_back_color_blue;
extern s32 g_battle_map_saved_back_color_green;
extern s32 g_battle_map_saved_back_color_red;

/* Only SET/GET define a result (stored RGB pointer, or null for SET while
 * disabled). Other command paths leave the target return register unspecified. */
s32* battle_map_control_gte_background_color(s32 command, s32 red, s32 green, s32 blue);
void battle_map_copy_background_color(void);
void battle_map_restore_background_color(void);
rgb24_t* battle_map_get_background_color(void);
void battle_map_set_background_color(s32 red, s32 green, s32 blue);
void battle_map_adjust_background_color(s16 red_delta, s16 green_delta, s16 blue_delta);

#endif
