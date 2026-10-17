#ifndef BATTLE_MAP_LIGHT_H
#define BATTLE_MAP_LIGHT_H

/* Battle map lighting: the light matrices and their transitions, the background gradient, ambient and darkness colors
 * and their fades, and the GTE back color. */

#include "fft/battle_map.h"
#include "fft/gfx.h"
#include "fft/render.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* light matrices */
/* Commands of battle_map_light_matrix_command. A START queues its STEP in
 * g_battle_map_light_transition_command, which the map draw loop issues every frame. */
typedef enum battle_map_light_matrix_command {
    BATTLE_MAP_LIGHT_MATRIX_COMMAND_SET = 0x5e,
    BATTLE_MAP_LIGHT_MATRIX_COMMAND_SAVE = 0x5f,
    BATTLE_MAP_LIGHT_MATRIX_COMMAND_RESTORE = 0x60,
    BATTLE_MAP_LIGHT_MATRIX_COMMAND_GET_COLOR = 0x61,
    BATTLE_MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT = 0x62,
    BATTLE_MAP_LIGHT_MATRIX_COMMAND_START_BLEND = 0x66, /* towards the given matrices over the duration */
    BATTLE_MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND = 0x67,
    BATTLE_MAP_LIGHT_MATRIX_COMMAND_START_DELTA = 0x68, /* add a frame-scaled delta */
    BATTLE_MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA = 0x69
} battle_map_light_matrix_command_e;

extern MATRIX g_battle_map_light_color_matrix;
extern MATRIX g_battle_map_light_matrix;
extern SVECTOR g_battle_map_light_rotation;
extern MATRIX g_battle_map_light_rotation_matrix;
extern MATRIX g_battle_map_local_light_matrix;
extern MATRIX g_battle_map_saved_light_color_matrix;            /* saved color matrix (command 0x5f/0x60) */
extern MATRIX g_battle_map_saved_light_matrix;                  /* saved light matrix (command 0x5f/0x60) */
extern u16 g_battle_map_light_transition_command;               /* pending per-frame light command, 0 when idle */
extern u16 g_battle_map_light_transition_duration;              /* transition duration in frames */
extern u16 g_battle_map_light_transition_frame;                 /* transition frame counter */
extern MATRIX g_battle_map_light_transition_start_color_matrix; /* transition start color matrix */
extern MATRIX g_battle_map_light_transition_start_light_matrix; /* transition start light matrix */

/* The transition's target (command 0x66) or delta (0x68) color and light matrices. */
extern MATRIX g_battle_map_light_transition_target_color_matrix;
extern MATRIX g_battle_map_light_transition_target_light_matrix;

MATRIX* battle_map_light_matrix_command(
    battle_map_light_matrix_command_e command, MATRIX* color, MATRIX* light, s32 duration);

/* map colors */
/* Commands for the map's background gradient, ambient and darkness colors. */
typedef enum battle_map_light_command {
    BATTLE_MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT = 0x56,
    BATTLE_MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT = 0x57,
    BATTLE_MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT = 0x58,
    BATTLE_MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT = 0x59,
    BATTLE_MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR = 0x5a,
    BATTLE_MAP_LIGHT_COMMAND_SAVE_AMBIENT_COLOR = 0x5b,
    BATTLE_MAP_LIGHT_COMMAND_RESTORE_AMBIENT_COLOR = 0x5c,
    BATTLE_MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR = 0x5d,
    BATTLE_MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR = 0x63,
    BATTLE_MAP_LIGHT_COMMAND_RESET_DARKNESS_COLOR = 0x64,
    BATTLE_MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR = 0x65
} battle_map_light_command_e;

/* The applied background gradient pair. The copies in battle_map_light_state_command
 * move each color as an independent unaligned 32-bit value (lwl/lwr + swl/swr). */
extern battle_map_background_gradient_colors_t g_battle_map_background_gradient_colors;
extern CVECTOR g_battle_map_saved_background_gradient_first;
extern CVECTOR g_battle_map_saved_background_gradient_second;
extern CVECTOR g_battle_map_ambient_base_color;
extern rgb24_t g_battle_map_ambient_polygon_color;
extern render_buffer_t* g_battle_map_ambient_restored_buffer;
extern CVECTOR g_battle_map_darkness_base_color;
extern CVECTOR g_battle_map_current_darkness_color;

void* battle_map_light_state_command(battle_map_light_command_e command, void* data);
void battle_map_background_init(void);
void battle_map_ambient_init(void);
void battle_map_darkness_init(void);
void battle_map_background_set(s32 frame_duration, const battle_map_background_gradient_colors_t* colors);
void battle_map_background_modify(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_background_transition_and_store(
    s32 frame_duration, const battle_map_background_gradient_colors_t* colors);
void battle_map_ambient_start_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_darkness_start_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_darkness_set(s32 frame_duration, const CVECTOR* color);

/* back color */
/* Commands of battle_map_light_control_back_color, which stores RGB and controls the GTE
 * background color. */
typedef enum battle_map_light_back_color_command {
    BATTLE_MAP_LIGHT_BACK_COLOR_SET = 9,
    BATTLE_MAP_LIGHT_BACK_COLOR_GET = 10,
    BATTLE_MAP_LIGHT_BACK_COLOR_DISABLE = 11,
    BATTLE_MAP_LIGHT_BACK_COLOR_RESTORE = 12
} battle_map_light_back_color_command_e;
extern u8 g_battle_map_back_color_disabled;
extern CVECTOR g_battle_map_cached_back_color;
extern rgb24_t g_battle_map_light_back_color_backup;

void battle_map_light_copy_back_color(void);
void battle_map_light_restore_back_color(void);
rgb24_t* battle_map_light_get_back_color(void);
void battle_map_light_set_back_color(s32 red, s32 green, s32 blue);
void battle_map_light_adjust_back_color(s16 red_delta, s16 green_delta, s16 blue_delta);

/* Only SET/GET define a result (stored RGB pointer, or null for SET while
 * disabled). Other command paths leave the target return register unspecified. */
s32* battle_map_light_control_back_color(s32 command, s32 red, s32 green, s32 blue);

#endif
