#ifndef BATTLE_CAMERA_H
#define BATTLE_CAMERA_H

/* BATTLE camera state, player input and scripted motion. WORLD declares its
 * separate camera state and entry points in world.h. */

#include "fft/battle_unit.h"
#include "fft/camera.h"
#include "fft/gfx.h"
#include "fft/script.h"

/* state and projection */
enum {
    CAMERA_PITCH_STEEP_MASK_THRESHOLD = 0x178,
};

extern MATRIX g_camera_matrix;
/* Base of the large render-state records; low offsets overlap camera/GPU fields.
 * .vx is the camera pitch and .vy its yaw. */
extern SVECTOR g_camera_render_state;

void camera_convert_unit_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords);
void camera_convert_screen_coords_modify_by_1(const SVECTOR* screen, SVECTOR* destination);
void camera_convert_screen_coords_modify_by_5(const SVECTOR* screen, SVECTOR* destination);
void camera_init_defaults(void);
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

/* g_battle_cursor_input_mode: how the directional buttons map to map directions. The first two are
 * the Cursor Movement option's settings; FREE_CURSOR sends the four buttons along the screen's axes. */
typedef enum battle_cursor_input_mode {
    BATTLE_CURSOR_INPUT_OPTION_A = 0,
    BATTLE_CURSOR_INPUT_OPTION_B = 1,
    BATTLE_CURSOR_INPUT_FREE_CURSOR = 2,
} battle_cursor_input_mode_e;

extern s32 g_battle_cursor_input_mode;
extern s32 g_battle_cursor_input_mode_saved;
/* Held directions pan the map instead of following the cursor tile. */
extern s32 g_camera_map_pan_enabled;

void camera_disable_map_pan(void);
void camera_enable_map_pan(void);
s32 camera_get_input_direction(s32 source);
void camera_handle_rotation_input(void);
void camera_update_map_pan(void);
void camera_update_tilt_input(void);
void camera_update_zoom_input(void);

/* position */
extern VECTOR g_camera_current_real_coords;
extern SVECTOR g_camera_integer_coords;
extern u16 g_camera_real_coord_countdown;
extern VECTOR g_camera_real_coord_delta;
extern VECTOR g_camera_saved_real_coords;
extern VECTOR g_camera_step_vector;

void camera_calculate_acting_unit_vector_normal(void);
s32 camera_calculate_cursor_tile_vector_normal(void);
VECTOR* camera_get_current_real_coords(void);
s32 camera_is_active(void);
void camera_restore_current_real_coords(void);
void camera_save_current_real_coords(void);
s32 camera_scale_cursor_tile_vector(void);
void camera_set_current_real_coords(const VECTOR* coords);
void camera_shift_acting_unit_vector(void);
void camera_step_focus_toward_cursor_tile(s16 height);
/* Some routines address these components as separate linker symbols. */
void camera_step_real_coords_toward_target(const battle_unit_t* target);
void camera_update_cursor_tile_vector(void);
void camera_update_real_coord_animation(void);
void camera_update_vector_from_game_state(void);

/* screen offset */
extern VECTOR g_camera_current_screen_offset;
extern VECTOR g_camera_saved_screen_offset;
extern u16 g_camera_screen_offset_countdown;
extern VECTOR g_camera_screen_offset_fixed;

VECTOR* camera_get_screen_offset(void);
void camera_restore_screen_offset(void);
void camera_save_screen_offset(void);
void camera_set_screen_offset(const VECTOR* coords);
void camera_start_screen_offset_move_relative(const VECTOR* values, s32 frame_count);
void camera_start_screen_offset_move_uniform(const s32* destination_value, s32 frame_count);
void camera_update_screen_offset_animation(void);

/* rotation */
/* g_camera_rotation_action. */
typedef enum camera_rotation_action {
    CAMERA_ROTATION_NONE = 0,
    CAMERA_ROTATION_LEFT = 1,  /* the yaw falls to its destination */
    CAMERA_ROTATION_RIGHT = 2, /* the yaw rises to its destination */
} camera_rotation_action_e;

extern s32 g_battle_previous_camera_yaw;
/* Maps a camera quadrant index to its map-tile camera-block direction. */
extern u8 g_camera_quadrant_block_directions[4];
extern camera_rotation_action_e g_camera_rotation_action;
extern SVECTOR g_camera_rotation_backup;
extern u16 g_camera_rotation_countdown;
extern SVECTOR g_camera_rotation_delta;
extern s32 g_camera_rotation_increment;
extern s32 g_camera_yaw_band;
extern s32 g_camera_yaw_target;

void camera_choose_unblocked_rotation_for_tile(const SVECTOR* tile_coords, SVECTOR* rotation_out);
void camera_convert_screen_coords_by_rotation(const SVECTOR* input, SVECTOR* output, s32 offset);
SVECTOR* camera_get_rotation(void);
void camera_request_rotation(s32 angle);
void camera_restore_rotation(void);
void camera_rotate_left(s32 target_angle);
void camera_rotate_right(s32 target_angle);
void camera_rotate_when_unit_tile_not_visible(const struct battle_unit* unit);
void camera_save_rotation(void);
void camera_set_rotation(const SVECTOR* rotation);
void camera_start_rotation_left(s32 target_angle);
void camera_start_rotation_right(s32 target_angle);
void camera_update_rotation(void);
void camera_update_rotation_animation(void);

/* tilt */
/* g_camera_tilt_action and g_camera_zoom_action: the move in progress towards
 * g_map_tilt_target or g_map_zoom_target. */
typedef enum camera_step_action {
    CAMERA_STEP_NONE = 0,
    CAMERA_STEP_INCREASE = 2, /* tilt up; zoom in */
    CAMERA_STEP_DECREASE = 4, /* tilt down; zoom out */
} camera_step_action_e;

/* g_map_tilt_target values the tilt code tests; the target is a tilt band plus one, and
 * bands 3 and 4 have no pitch here. */
enum {
    MAP_TILT_LOW = 1,  /* pitch 0x12e */
    MAP_TILT_HIGH = 2, /* pitch 0x1c0 */
};

extern camera_step_action_e g_camera_tilt_action;
extern s32 g_camera_tilt_increment;

void camera_toggle_tilt(void);
void camera_update_tilt(void);

/* zoom */
/* g_map_zoom_target. */
enum {
    MAP_ZOOM_IN = 1,  /* scale ONE */
    MAP_ZOOM_OUT = 4, /* scale 0xc00 */
};

extern VECTOR g_camera_zoom;
extern camera_step_action_e g_camera_zoom_action;
extern VECTOR g_camera_zoom_backup;
extern u16 g_camera_zoom_countdown;
extern VECTOR g_camera_zoom_delta;
extern s32 g_camera_zoom_increment;

VECTOR* camera_get_zoom(void);
void camera_restore_zoom(void);
void camera_save_zoom(void);
void camera_set_zoom(const VECTOR* zoom);
void camera_toggle_zoom(void);
void camera_update_zoom(void);
void camera_update_zoom_animation(void);

/* scripted motion */
extern VECTOR* g_camera_published_coords_ptr;
extern SVECTOR* g_camera_published_rotation_ptr;
extern SVECTOR g_camera_script_rotation;
extern u16 g_camera_script_variable_indices[EVENT_CAMERA_COMPONENT_COUNT];
extern VECTOR g_camera_script_vector;

void camera_execute_angle_command(s32 command, s32 target, s32 duration, const SVECTOR* angles);
void camera_execute_position_command(s32 command, s32 target, s32 duration, const VECTOR* offset);
void camera_execute_zoom_command(s32 command, s32 unused, s32 duration, const VECTOR* zoom);
void camera_load_state_from_script_variables(void);
void camera_store_state_to_script_variables(void);

/* effect tracks */
void camera_advance_effect_tracks(s16 frame);
void camera_calculate_relative_offset(const SVECTOR* input, SVECTOR* output, s32 offset);
void camera_calculate_relative_offset_by_1(const SVECTOR* input, SVECTOR* output);
void camera_calculate_relative_offset_by_5(const SVECTOR* input, SVECTOR* output);
s32 camera_find_active_keyframe(s16 frame, s16 mask, s32* out_index, s32* out_frame);
void camera_start_effect_keyframe_moves(s16 frame);
void camera_update_effect_rotation_and_zoom(void);

#endif
