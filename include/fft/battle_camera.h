#ifndef FFT_BATTLE_CAMERA_H
#define FFT_BATTLE_CAMERA_H

#include "psx/gte.h"

/* Script-variable word indices that mirror the live camera. Proven by
 * battle_camera_store_state_to_script_variables (0x80143a9c), which writes
 * them, and battle_camera_load_state_from_script_variables, which reads them back into a VECTOR
 * (X -> vx, Z (height) -> vy, Y (map depth) -> vz). */
enum {
    BATTLE_CAMERA_X_WORD_INDEX = 0x1a,
    BATTLE_CAMERA_Z_WORD_INDEX = 0x1b,
    BATTLE_CAMERA_Y_WORD_INDEX = 0x1c,
    BATTLE_CAMERA_TILT_WORD_INDEX = 0x1d,
    BATTLE_CAMERA_YAW_WORD_INDEX = 0x1e,
    BATTLE_CAMERA_ROLL_WORD_INDEX = 0x1f,
    BATTLE_CAMERA_ZOOM_WORD_INDEX = 0x20,
};

enum {
    BATTLE_CAMERA_PITCH_STEEP_MASK_THRESHOLD = 0x178,
    BATTLE_CAMERA_FUSION_HOLD_SENTINEL = 0x2710,
};

/* Renderer-space coordinates use signed X, Z, Y halfwords. */
typedef struct battle_screen_coords {
    s16 x;
    s16 z;
    s16 y;
} battle_screen_coords_t;

/* Camera rotations are three packed halfwords with no padding. */
typedef struct battle_camera_rotation {
    u16 x;
    u16 y;
    u16 z;
} battle_camera_rotation_t;

typedef char battle_camera_rotation_size_must_be_6[sizeof(battle_camera_rotation_t) == 6 ? 1 : -1];

/* Provisional camera interpolation key: frame time and target value. */
typedef struct battle_camera_key {
    s32 time;  /* 0x00; cumulative frame of this key */
    s32 value; /* 0x04 */
    u8 unknown_08[8];
} battle_camera_key_t;

/* Provisional per-component camera fusion track (0xa4 bytes). */
typedef struct battle_camera_track {
    battle_camera_key_t keys[8]; /* 0x00 */
    s32 key_count;               /* 0x80 */
    s32 segment;                 /* 0x84; first key of the active segment */
    s32 frame;                   /* 0x88; frames since the track started */
    s32 segment_frame;           /* 0x8c; frames into the active segment */
    s32 prev_time;               /* 0x90; Q12 */
    s32 prev_value;              /* 0x94; Q12 */
    s32 start_value;             /* 0x98; Q12 */
    s32 segment_duration;        /* 0x9c */
    s32 finished;                /* 0xa0 */
} battle_camera_track_t;

typedef char battle_camera_track_size_must_be_0xa4[sizeof(battle_camera_track_t) == 0xa4 ? 1 : -1];

/* Screen-Z helpers over Misc Unit Data (battle_unit_misc_data_t in fft/battle.h).
 * Each resolves the unit's mount first and applies the rider height offset. */
struct battle_unit_misc_data;

extern VECTOR g_battle_camera_current_real_coords;
extern SVECTOR g_battle_camera_integer_coords;
extern MATRIX g_battle_camera_matrix;
extern u16 g_battle_camera_offset_screen_coord_countdown;
extern VECTOR g_battle_camera_offset_screen_coords;
extern VECTOR g_battle_camera_offset_screen_coords_fixed;
extern u16 g_battle_camera_real_coord_countdown;
extern VECTOR g_battle_camera_real_coord_delta;
extern s32 g_battle_camera_rotation_action;
extern SVECTOR g_battle_camera_rotation_backup;
extern u16 g_battle_camera_rotation_countdown;
extern SVECTOR g_battle_camera_rotation_delta;
extern s32 g_battle_camera_rotation_increment;
extern VECTOR g_battle_camera_saved_real_coords;
extern s32 g_battle_camera_tilt_action;
extern s32 g_battle_camera_tilt_increment;
extern VECTOR g_battle_camera_zoom;
extern s32 g_battle_camera_zoom_action;
extern VECTOR g_battle_camera_zoom_backup;
extern u16 g_battle_camera_zoom_countdown;
extern VECTOR g_battle_camera_zoom_delta;
extern s32 g_battle_camera_zoom_increment;
extern battle_screen_coords_t g_battle_current_misc_screen_coords;
extern VECTOR g_battle_current_vector;

/* Some routines address these components as separate linker symbols. */

/* adjust */
void battle_camera_update_vector_from_game_state(void);

/* calculate */
s32 battle_calculate_screen_z_from_input_coords(
    struct battle_unit_misc_data* unit, const battle_screen_coords_t* coords);

/* copy */
battle_screen_coords_t* battle_copy_misc_unit_screen_coords(u32 misc_id);

/* camera */
void battle_camera_calculate_acting_unit_vector_normal(void);
s32 battle_camera_calculate_cursor_tile_vector_normal(void);
void battle_camera_call_rotate_left(s32 target_angle);
void battle_camera_call_rotate_right(s32 target_angle);
void battle_camera_call_toggle_tilt(void);
void battle_camera_call_zoom_map(void);
void battle_camera_copy_current_real_coords(void);
void battle_camera_copy_rotation(void);
void battle_camera_copy_zoom(void);
void battle_camera_execute_zoom_command(s32 flags, s32 unused, s32 duration, VECTOR* zoom);
VECTOR* battle_camera_get_current_real_coords(void);
u16* battle_camera_get_rotation(void);
VECTOR* battle_camera_get_zoom(void);
void battle_camera_handle_rotation_input(void);
void battle_camera_init_defaults(void);
void battle_camera_init_tilt_and_zoom(void);
void battle_camera_normalize_yaw_angle(void);
s32 battle_camera_is_active(void);
void battle_camera_project_world_coords_to_screen(const battle_screen_coords_t* world_coords, s16* screen_coords);
void battle_camera_request_rotation(s32 angle);
void battle_camera_reset_script_transform(void);
void battle_camera_restore_current_real_coords(void);
void battle_camera_restore_rotation(void);
void battle_camera_restore_zoom(void);
void battle_camera_rotate_left(s32 target_angle);
void battle_camera_rotate_right(s32 target_angle);
void battle_camera_rotate_when_unit_tile_not_visible(struct battle_unit_misc_data* unit);
s32 battle_camera_scale_cursor_tile_vector(void);
void battle_camera_set_current_real_coords(const VECTOR* coords);
void battle_camera_set_rotation(const battle_camera_rotation_t* rotation);
void battle_camera_set_zoom(const VECTOR* zoom);
s32 battle_camera_step_track(battle_camera_track_t* track, s32 component);
void battle_camera_store_state_to_script_variables(void);
void battle_camera_store_yaw_band(s32 value);
void battle_camera_toggle_zoom(void);
void battle_camera_update_map_pan(void);
void battle_camera_update_matrices(
    MATRIX* camera_matrix, void* render_buffers, void* offset_screen_coords, VECTOR* camera_zoom);
void battle_camera_update_offset_screen_coord_animation(void);
void battle_camera_update_real_coord_animation(void);
void battle_camera_update_rotation(void);
void battle_camera_update_rotation_animation(void);
void battle_camera_update_tilt(void);
void battle_camera_update_zoom(void);
void battle_camera_update_zoom_animation(void);
s32 battle_camera_wrap_yaw_angle(void);

/* gfx */
s32 battle_gfx_calculate_screen_z_from_misc_battle_map_data(struct battle_unit_misc_data* unit);
s32 battle_gfx_calculate_screen_z_from_misc_move_data(struct battle_unit_misc_data* unit);

/* Returns (s16), but an s16 prototype makes callers such as
 * battle_unit_update_distortion_height re-extend the result; keep s32. */
s32 battle_gfx_calculate_screen_z_from_misc_screen_data(struct battle_unit_misc_data* unit);
s32 battle_gfx_calculate_screen_z_with_caller_data(
    struct battle_unit_misc_data* unit, const battle_screen_coords_t* coords);
void battle_gfx_init_render_buffers(void);

/* map */
void battle_map_update_animations(void);
void battle_map_update_lighting(void);

/* menu */
s32 battle_menu_draw_selection_data(s32 otag, s32 controller_input);

/* target */
void battle_map_draw_mesh_and_weather(MATRIX* camera_matrix);

/* unit */
struct battle_unit_misc_data* battle_unit_get_mount_misc_data(struct battle_unit_misc_data* unit);
battle_screen_coords_t* battle_unit_get_screen_data_ptr_by_misc_id(u32 misc_id);
void battle_unit_set_real_coords_from_screen_coords(struct battle_unit_misc_data* unit);
void battle_unit_set_screen_coords_from_real_coords(struct battle_unit_misc_data* unit);

void battle_camera_advance_effect_tracks(s16 frame);
void battle_camera_calculate_relative_offset(const u16* input, s16* output, s32 offset);
void battle_camera_calculate_relative_offset_1(const u16* input, s16* output);
void battle_camera_calculate_relative_offset_5(const u16* input, s16* output);
void battle_camera_choose_unblocked_rotation_for_tile(
    const battle_screen_coords_t* tile_coords, battle_camera_rotation_t* rotation_out);
void battle_camera_convert_screen_coords_modify_by_5(const s16* screen, s16* destination);
s32 battle_camera_find_active_keyframe(s16 frame, s32 mask, s32* out_index, s32* out_frame);
void battle_camera_load_state_from_script_variables(void);
void battle_camera_set_focus_and_cursor_tile(
    const battle_screen_coords_t* coords, const battle_camera_rotation_t* rotation);
void battle_camera_start_effect_keyframe_moves(s16 frame);
void battle_camera_update_cursor_tile_vector(void);
extern s32 g_battle_camera_cross_product_overflow; /* set by battle_fixed_cross_product_q12 on Q12 overflow */
extern s16 g_battle_camera_diagonal_yaws[4];
/* Base of the large render-state records; low offsets overlap camera/GPU fields.
 * .vx is the camera pitch and .vy its yaw. */
extern SVECTOR g_battle_camera_render_state;

void battle_camera_shift_acting_unit_vector(void);

#endif
