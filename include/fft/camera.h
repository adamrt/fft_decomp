#ifndef CAMERA_H
#define CAMERA_H

/* The camera: the BATTLE map camera's projection, player controls and animated position, screen offset, rotation, tilt
 * and zoom, and the event library's script camera for BATTLE and WORLD with its fusion tracks and fixed-point math. */

#include "fft/script_instruction.h"
#include "psx/libgte.h"
#include "psx/types.h"

struct map_unit;

/* state and projection */
enum { CAMERA_PITCH_STEEP_MASK_THRESHOLD = 0x178 };

extern MATRIX g_camera_matrix;

/* Base of the large render-state records; low offsets overlap camera/GPU fields.
 * .vx is the camera pitch and .vy its yaw. */
extern SVECTOR g_camera_render_state;

void camera_update_matrices(MATRIX* camera_matrix, SVECTOR* rotation, VECTOR* screen_offset, VECTOR* camera_zoom);
s32 camera_is_active(void);
void camera_project_world_coords_to_screen(SVECTOR* world_coords, SVECTOR* screen_coords);
void camera_set_position_and_cursor(const SVECTOR* coords, const SVECTOR* rotation);
void camera_map_unit_get_depth_point(u32 map_unit_id, SVECTOR* coords);
void camera_get_unbiased_tile_depth_point(const SVECTOR* world, SVECTOR* destination);
void camera_get_biased_tile_depth_point(const SVECTOR* world, SVECTOR* destination);
void camera_get_unbiased_step_depth_point(const SVECTOR* input, SVECTOR* output);
void camera_get_biased_step_depth_point(const SVECTOR* input, SVECTOR* output);

/* input */
/* cursor_get_input_direction sources: which button state it reads. */
typedef enum camera_input_source {
    CAMERA_INPUT_HELD = 0,
    CAMERA_INPUT_PRESSED = 1, /* newly pressed, repeating at the cursor repeat speed while held */
    CAMERA_INPUT_RELEASED = 2
} camera_input_source_e;

/* Held directions pan the map instead of following the cursor tile. */
extern s32 g_camera_pan_enabled;

void camera_disable_pan(void);
void camera_enable_pan(void);
s32 cursor_get_input_direction(s32 source);
void camera_update_pan(void);
void camera_handle_rotation_input(void);
void camera_handle_tilt_input(void);
void camera_handle_zoom_input(void);

/* position */
extern VECTOR g_camera_real_coords;
extern SVECTOR g_camera_integer_coords;
extern VECTOR g_camera_step_vector;

VECTOR* camera_get_real_coords(void);
void camera_set_real_coords(const VECTOR* coords);
void camera_save_real_coords(void);
void camera_restore_real_coords(void);
void camera_update_real_coords_move(void);
void camera_update_cursor_focus(void);
void camera_update_scene_focus(void);

/* screen offset */
extern VECTOR g_camera_screen_offset;
extern u16 g_camera_screen_offset_countdown;

VECTOR* camera_get_screen_offset(void);
void camera_set_screen_offset(const VECTOR* coords);
void camera_save_screen_offset(void);
void camera_restore_screen_offset(void);
void camera_start_relative_screen_offset_move(const VECTOR* values, s32 frame_count);
void camera_start_uniform_screen_offset_move(const s32* destination_value, s32 frame_count);
void camera_update_screen_offset_move(void);

/* rotation */
/* Values of g_camera_rotation_action. */
typedef enum camera_rotation_action {
    CAMERA_ROTATION_NONE = 0,
    CAMERA_ROTATION_LEFT = 1, /* the yaw falls to its destination */
    CAMERA_ROTATION_RIGHT = 2 /* the yaw rises to its destination */
} camera_rotation_action_e;

extern camera_rotation_action_e g_camera_rotation_action;
extern u16 g_camera_rotation_countdown;

SVECTOR* camera_get_rotation(void);
void camera_set_rotation(const SVECTOR* rotation);
void camera_save_rotation(void);
void camera_restore_rotation(void);
void camera_update_rotation(void);
void camera_update_rotation_move(void);
void camera_rotate_for_unit_visibility(const struct map_unit* unit);

/* tilt */
/* g_camera_tilt_action and g_camera_zoom_action: the move in progress towards
 * g_camera_tilt_target or g_camera_zoom_target. INCREASE equals MAP_TILT_HIGH and DECREASE
 * equals MAP_ZOOM_OUT, and retail compares a target with the action loaded for that value. */
typedef enum camera_step_action {
    CAMERA_STEP_NONE = 0,
    CAMERA_STEP_INCREASE = 2, /* tilt up; zoom in */
    CAMERA_STEP_DECREASE = 4  /* tilt down; zoom out */
} camera_step_action_e;

extern s32 g_camera_tilt_target;
extern camera_step_action_e g_camera_tilt_action;

void camera_update_tilt(void);

/* zoom */
extern s32 g_camera_zoom_target;
extern VECTOR g_camera_zoom;
extern camera_step_action_e g_camera_zoom_action;
extern u16 g_camera_zoom_countdown;

VECTOR* camera_get_zoom(void);
void camera_set_zoom(const VECTOR* zoom);
void camera_save_zoom(void);
void camera_restore_zoom(void);
void camera_update_zoom(void);
void camera_update_zoom_move(void);

/* script camera */
extern SVECTOR g_camera_script_rotation;
extern u16 g_camera_script_variable_indices[EVENT_CAMERA_COMPONENT_COUNT];
extern u16 g_camera_shake_z_offset;
extern u16 g_camera_position_locked;
extern s32 g_camera_wrap_yaw_pending;

/* The CameraSpeedCurve operand, unpacked with the EVENT_CAMERA_SPEED_CURVE_* masks. */
extern s32 g_camera_speed_curve;

void camera_thread(void);
void camera_wait_idle(void);
void camera_settle_tilt_zoom(void);
void camera_clear_zoom_flag(void);
void camera_normalize_yaw(void);
void camera_store_yaw_band(s32 value);
s32 camera_wrap_yaw(void);
void camera_store_state_to_script_variables(void);

/* fusion tracks */
/* A camera operand or key value that keeps the component's current value. */
enum { CAMERA_FUSION_HOLD_SENTINEL = 0x2710 };

void camera_fusion_thread(void);

/* fixed-point math */
/* No C source: hand-written MIPS in the original, kept as original bytes through the `regions:` rows of
 * target/battle.yaml and target/world.yaml (a 64-round signed divide and a fixed-point multiply with overflow check); a
 * port supplies both bodies. QUIRKS.md records their rounding and carry bugs. */
s32 mul_div_s64(s32 a, s32 b, s32 c);
s32 fixed_cross_product_q12(s32 a, s32 b, s32 c, s32 d);

/* tilt and zoom bands */
/* g_camera_tilt_target values the tilt code tests; the target is a tilt band plus one, and
 * bands 3 and 4 have no pitch here. */
enum {
    MAP_TILT_LOW = 1, /* pitch 0x12e */
    MAP_TILT_HIGH = 2 /* pitch 0x1c0 */
};

/* g_camera_zoom_target values. */
enum {
    MAP_ZOOM_IN = 1, /* scale ONE */
    MAP_ZOOM_OUT = 4 /* scale 0xc00 */
};

#endif
