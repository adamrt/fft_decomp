#ifndef MAP_LIGHT_H
#define MAP_LIGHT_H

/* Battle map lighting: the light matrices and their transitions, the background gradient, ambient and darkness colors
 * and their fades, and the GTE back color. */

#include "psx/libgte.h"
#include "psx/types.h"

/* light matrices */
/* Commands of map_light_dispatch_matrix_command. A START queues its STEP in
 * g_map_light_transition_command, which the map draw loop issues every frame. */
typedef enum map_light_matrix_command {
    MAP_LIGHT_MATRIX_COMMAND_SET = 0x5e,
    MAP_LIGHT_MATRIX_COMMAND_SAVE = 0x5f,
    MAP_LIGHT_MATRIX_COMMAND_RESTORE = 0x60,
    MAP_LIGHT_MATRIX_COMMAND_GET_COLOR = 0x61,
    MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT = 0x62,
    MAP_LIGHT_MATRIX_COMMAND_START_BLEND = 0x66, /* towards the given matrices over the duration */
    MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND = 0x67,
    MAP_LIGHT_MATRIX_COMMAND_START_DELTA = 0x68, /* add a frame-scaled delta */
    MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA = 0x69
} map_light_matrix_command_e;

MATRIX* map_light_dispatch_matrix_command(
    map_light_matrix_command_e command, MATRIX* color, MATRIX* light, s32 duration);

/* map colors */
/* Map backgrounds use two four-byte runtime color slots. The fourth byte
 * is copied with each CVECTOR but is not an alpha channel; mesh-file RGB
 * triples have a separate three-byte layout. */
typedef enum map_background_gradient_color {
    MAP_BACKGROUND_GRADIENT_TOP = 0,
    MAP_BACKGROUND_GRADIENT_BOTTOM = 1,
    MAP_BACKGROUND_GRADIENT_COLOR_COUNT = 2
} map_background_gradient_color_e;

typedef struct map_background_gradient_colors {
    CVECTOR colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT];
} map_background_gradient_colors_t;
typedef char map_background_gradient_colors_size_must_be_8[(sizeof(map_background_gradient_colors_t) == 8) ? 1 : -1];

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
    MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR = 0x65
} map_light_command_e;

void* map_light_dispatch_command(map_light_command_e command, void* data);
void map_light_init_background(void);
void map_light_init_ambient(void);
void map_light_init_darkness(void);
void map_light_set_background(s32 frame_duration, const map_background_gradient_colors_t* colors);
void map_light_modify_background(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void map_light_transition_and_store_background(s32 frame_duration, const map_background_gradient_colors_t* colors);
void map_light_start_ambient_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void map_light_start_darkness_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void map_light_set_darkness(s32 frame_duration, const CVECTOR* color);

/* back color */
/* Commands of map_light_dispatch_back_color_command, which stores RGB and controls the GTE
 * background color. */
typedef enum map_light_back_color_command {
    MAP_LIGHT_BACK_COLOR_SET = 9,
    MAP_LIGHT_BACK_COLOR_GET = 10,
    MAP_LIGHT_BACK_COLOR_DISABLE = 11,
    MAP_LIGHT_BACK_COLOR_RESTORE = 12
} map_light_back_color_command_e;

/* Only SET/GET define a result (stored RGB pointer, or null for SET while
 * disabled). Other command paths leave the target return register unspecified. */
s32* map_light_dispatch_back_color_command(s32 command, s32 r, s32 g, s32 b);

/* script lighting */
void map_light_reset_script_light(void);

#endif
