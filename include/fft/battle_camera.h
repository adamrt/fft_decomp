#ifndef BATTLE_CAMERA_H
#define BATTLE_CAMERA_H

/* The BATTLE camera: its position, rotation, tilt and zoom, each animated toward a destination,
 * the player's camera controls, and the motion event scripts and effects give it. The camera code
 * the event systems of both programs share is in camera.h. */

#include "fft/battle_unit.h"
#include "fft/script_instruction.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* state and projection */
enum {
    CAMERA_PITCH_STEEP_MASK_THRESHOLD = 0x178,
};

extern MATRIX g_camera_matrix;
/* Base of the large render-state records; low offsets overlap camera/GPU fields.
 * .vx is the camera pitch and .vy its yaw. */
extern SVECTOR g_camera_render_state;

void camera_get_unit_depth_point_by_runtime_id(u32 runtime_id, SVECTOR* coords);
void camera_get_tile_depth_point_unbiased(const SVECTOR* screen, SVECTOR* destination);
void camera_get_tile_depth_point_biased(const SVECTOR* screen, SVECTOR* destination);
void camera_project_world_coords_to_screen(SVECTOR* world_coords, SVECTOR* screen_coords);
void camera_set_focus_and_cursor_tile(const SVECTOR* coords, const SVECTOR* rotation);
void camera_update_matrices(MATRIX* camera_matrix, SVECTOR* rotation, VECTOR* screen_offset, VECTOR* camera_zoom);

/* input */
/* camera_get_input_direction sources: which button state it reads. */
typedef enum camera_input_source {
    CAMERA_INPUT_HELD = 0,
    CAMERA_INPUT_PRESSED = 1, /* newly pressed, repeating at the cursor repeat speed while held */
    CAMERA_INPUT_RELEASED = 2,
} camera_input_source_e;

/* g_cursor_input_mode: how the directional buttons map to map directions. The first two are
 * the Cursor Movement option's settings; FREE_CURSOR sends the four buttons along the screen's axes. */
typedef enum cursor_input_mode {
    CURSOR_INPUT_OPTION_A = 0,
    CURSOR_INPUT_OPTION_B = 1,
    CURSOR_INPUT_FREE_CURSOR = 2,
} cursor_input_mode_e;

extern s32 g_cursor_input_mode;
extern s32 g_cursor_input_mode_saved;
/* Held directions pan the map instead of following the cursor tile. */
extern s32 g_camera_pan_enabled;

void camera_pan_disable(void);
void camera_pan_enable(void);
s32 camera_get_input_direction(s32 source);
void camera_rotation_handle_input(void);
void camera_pan_update(void);
void camera_tilt_handle_input(void);
void camera_zoom_handle_input(void);

/* position */
extern VECTOR g_camera_real_coords;
extern SVECTOR g_camera_integer_coords;
extern u16 g_camera_real_coords_countdown;
extern VECTOR g_camera_real_coords_delta;
extern VECTOR g_camera_step_vector;

VECTOR* camera_real_coords_get(void);
s32 camera_is_active(void);
void camera_real_coords_restore(void);
void camera_real_coords_save(void);
void camera_real_coords_set(const VECTOR* coords);
void camera_update_cursor_tile_vector(void);
void camera_real_coords_update_animation(void);
void camera_update_vector_from_game_state(void);

/* screen offset */
extern VECTOR g_camera_screen_offset;
extern VECTOR g_camera_screen_offset_saved;
extern u16 g_camera_screen_offset_countdown;
extern VECTOR g_camera_screen_offset_fixed;

VECTOR* camera_screen_offset_get(void);
void camera_screen_offset_restore(void);
void camera_screen_offset_save(void);
void camera_screen_offset_set(const VECTOR* coords);
void camera_screen_offset_start_move_relative(const VECTOR* values, s32 frame_count);
void camera_screen_offset_start_move_uniform(const s32* destination_value, s32 frame_count);
void camera_screen_offset_update_animation(void);

/* rotation */
/* g_camera_rotation_action. */
typedef enum camera_rotation_action {
    CAMERA_ROTATION_NONE = 0,
    CAMERA_ROTATION_LEFT = 1,  /* the yaw falls to its destination */
    CAMERA_ROTATION_RIGHT = 2, /* the yaw rises to its destination */
} camera_rotation_action_e;

/* Camera yaw rounded down to 0x200 (yaw & 0xfe00), stored as each turn's destination is set;
 * nothing reads it. */
extern s32 g_camera_previous_yaw;
/* Maps a camera quadrant index to its map-tile camera-block direction. */
extern u8 g_camera_quadrant_block_directions[4];
extern camera_rotation_action_e g_camera_rotation_action;
extern SVECTOR g_camera_rotation_saved;
extern u16 g_camera_rotation_countdown;
extern s32 g_camera_rotation_increment;
extern s32 g_camera_yaw_band;
extern s32 g_camera_yaw_target;

SVECTOR* camera_rotation_get(void);
void camera_rotation_restore(void);
void camera_rotate_when_unit_tile_not_visible(const struct unit* unit);
void camera_rotation_save(void);
void camera_rotation_set(const SVECTOR* rotation);
void camera_rotation_update(void);
void camera_rotation_update_animation(void);

/* tilt */
/* g_camera_tilt_action and g_camera_zoom_action: the move in progress towards
 * g_camera_tilt_target or g_camera_zoom_target. INCREASE equals BATTLE_MAP_TILT_HIGH and DECREASE
 * equals BATTLE_MAP_ZOOM_OUT, and retail compares a target with the action loaded for that value. */
typedef enum camera_step_action {
    CAMERA_STEP_NONE = 0,
    CAMERA_STEP_INCREASE = 2, /* tilt up; zoom in */
    CAMERA_STEP_DECREASE = 4, /* tilt down; zoom out */
} camera_step_action_e;

/* g_camera_tilt_target values the tilt code tests; the target is a tilt band plus one, and
 * bands 3 and 4 have no pitch here. */
enum {
    BATTLE_MAP_TILT_LOW = 1,  /* pitch 0x12e */
    BATTLE_MAP_TILT_HIGH = 2, /* pitch 0x1c0 */
};

extern camera_step_action_e g_camera_tilt_action;
extern s32 g_camera_tilt_increment;

void camera_tilt_update(void);

/* zoom */
/* g_camera_zoom_target. */
enum {
    BATTLE_MAP_ZOOM_IN = 1,  /* scale ONE */
    BATTLE_MAP_ZOOM_OUT = 4, /* scale 0xc00 */
};

extern VECTOR g_camera_zoom;
extern camera_step_action_e g_camera_zoom_action;
extern VECTOR g_camera_zoom_saved;
extern u16 g_camera_zoom_countdown;
extern s32 g_camera_zoom_increment;

VECTOR* camera_zoom_get(void);
void camera_zoom_restore(void);
void camera_zoom_save(void);
void camera_zoom_set(const VECTOR* zoom);
void camera_zoom_update(void);
void camera_zoom_update_animation(void);

/* scripted motion */
extern VECTOR* g_camera_published_coords_ptr;
extern SVECTOR* g_camera_published_rotation_ptr;
extern SVECTOR g_camera_script_rotation;
extern u16 g_camera_script_variable_indices[EVENT_CAMERA_COMPONENT_COUNT];
extern VECTOR g_camera_script_vector;

/* effect tracks */
void effect_camera_start_parent_keyframe_moves(s16 frame);
void camera_get_step_depth_point_unbiased(const SVECTOR* input, SVECTOR* output);
void camera_get_step_depth_point_biased(const SVECTOR* input, SVECTOR* output);
void effect_camera_start_child_keyframe_moves(s16 frame);
void effect_camera_update_tracks(void);

#endif
