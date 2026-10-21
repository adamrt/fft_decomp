#ifndef CAMERA_H
#define CAMERA_H

/* The camera: the BATTLE map camera's projection, player controls and animated position, screen offset, rotation, tilt
 * and zoom, and the event library's script camera for BATTLE and WORLD with its fusion tracks and fixed-point math. */

#include "fft/script_instruction.h"
#include "psx/libgte.h"
#include "psx/types.h"

struct unit_runtime;

/* state and projection */
enum { CAMERA_PITCH_STEEP_MASK_THRESHOLD = 0x178 };

extern MATRIX g_camera_matrix;

/* Base of the large render-state records; low offsets overlap camera/GPU fields.
 * .vx is the camera pitch and .vy its yaw. */
extern SVECTOR g_camera_render_state;

void camera_matrices_update(MATRIX* camera_matrix, SVECTOR* rotation, VECTOR* screen_offset, VECTOR* camera_zoom);
s32 camera_is_active(void);
void camera_project_world_coords_to_screen(SVECTOR* world_coords, SVECTOR* screen_coords);
void camera_cursor_focus_set(const SVECTOR* coords, const SVECTOR* rotation);
void camera_get_unit_depth_point_by_runtime_id(u32 runtime_id, SVECTOR* coords);
void camera_get_tile_depth_point_unbiased(const SVECTOR* world, SVECTOR* destination);
void camera_get_tile_depth_point_biased(const SVECTOR* world, SVECTOR* destination);
void camera_get_step_depth_point_unbiased(const SVECTOR* input, SVECTOR* output);
void camera_get_step_depth_point_biased(const SVECTOR* input, SVECTOR* output);

/* input */
/* cursor_input_direction_get sources: which button state it reads. */
typedef enum camera_input_source {
    CAMERA_INPUT_HELD = 0,
    CAMERA_INPUT_PRESSED = 1, /* newly pressed, repeating at the cursor repeat speed while held */
    CAMERA_INPUT_RELEASED = 2
} camera_input_source_e;

/* Held directions pan the map instead of following the cursor tile. */
extern s32 g_camera_pan_enabled;

void camera_pan_disable(void);
void camera_pan_enable(void);
s32 cursor_input_direction_get(s32 source);
void camera_pan_update(void);
void camera_rotation_handle_input(void);
void camera_tilt_handle_input(void);
void camera_zoom_handle_input(void);

/* position */
extern VECTOR g_camera_real_coords;
extern SVECTOR g_camera_integer_coords;
extern VECTOR g_camera_step_vector;

VECTOR* camera_real_coords_get(void);
void camera_real_coords_set(const VECTOR* coords);
void camera_real_coords_save(void);
void camera_real_coords_restore(void);
void camera_real_coords_update_animation(void);
void camera_cursor_focus_update(void);
void camera_scene_focus_update(void);

/* screen offset */
extern VECTOR g_camera_screen_offset;
extern u16 g_camera_screen_offset_countdown;

VECTOR* camera_screen_offset_get(void);
void camera_screen_offset_set(const VECTOR* coords);
void camera_screen_offset_save(void);
void camera_screen_offset_restore(void);
void camera_screen_offset_start_move_relative(const VECTOR* values, s32 frame_count);
void camera_screen_offset_start_move_uniform(const s32* destination_value, s32 frame_count);
void camera_screen_offset_update_animation(void);

/* rotation */
/* Values of g_camera_rotation_action. */
typedef enum camera_rotation_action {
    CAMERA_ROTATION_NONE = 0,
    CAMERA_ROTATION_LEFT = 1, /* the yaw falls to its destination */
    CAMERA_ROTATION_RIGHT = 2 /* the yaw rises to its destination */
} camera_rotation_action_e;

extern camera_rotation_action_e g_camera_rotation_action;
extern u16 g_camera_rotation_countdown;

SVECTOR* camera_rotation_get(void);
void camera_rotation_set(const SVECTOR* rotation);
void camera_rotation_save(void);
void camera_rotation_restore(void);
void camera_rotation_update(void);
void camera_rotation_update_animation(void);
void camera_unit_visibility_rotate(const struct unit_runtime* unit);

/* tilt */
/* g_camera_tilt_action and g_camera_zoom_action: the move in progress towards
 * g_camera_tilt_target or g_camera_zoom_target. INCREASE equals BATTLE_MAP_TILT_HIGH and DECREASE
 * equals BATTLE_MAP_ZOOM_OUT, and retail compares a target with the action loaded for that value. */
typedef enum camera_step_action {
    CAMERA_STEP_NONE = 0,
    CAMERA_STEP_INCREASE = 2, /* tilt up; zoom in */
    CAMERA_STEP_DECREASE = 4  /* tilt down; zoom out */
} camera_step_action_e;

extern s32 g_camera_tilt_target;
extern camera_step_action_e g_camera_tilt_action;

void camera_tilt_update(void);

/* zoom */
extern s32 g_camera_zoom_target;
extern VECTOR g_camera_zoom;
extern camera_step_action_e g_camera_zoom_action;
extern u16 g_camera_zoom_countdown;

VECTOR* camera_zoom_get(void);
void camera_zoom_set(const VECTOR* zoom);
void camera_zoom_save(void);
void camera_zoom_restore(void);
void camera_zoom_update(void);
void camera_zoom_update_animation(void);

/* script camera */
extern SVECTOR g_camera_script_rotation;
extern u16 g_camera_script_variable_indices[EVENT_CAMERA_COMPONENT_COUNT];
extern u16 g_camera_shake_z_offset;
extern u16 g_camera_position_locked;
extern s32 g_camera_wrap_yaw_pending;

/* The CameraSpeedCurve operand, unpacked with the EVENT_CAMERA_SPEED_CURVE_* masks. */
extern s32 g_camera_speed_curve;

void camera_thread(void);
void camera_idle_wait(void);
void camera_tilt_zoom_init(void);
void camera_zoom_flag_clear(void);
void camera_yaw_normalize(void);
void camera_yaw_band_store(s32 value);
s32 camera_yaw_wrap(void);
void camera_state_to_script_variables_store(void);

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
    BATTLE_MAP_TILT_LOW = 1, /* pitch 0x12e */
    BATTLE_MAP_TILT_HIGH = 2 /* pitch 0x1c0 */
};

/* g_camera_zoom_target values. */
enum {
    BATTLE_MAP_ZOOM_IN = 1, /* scale ONE */
    BATTLE_MAP_ZOOM_OUT = 4 /* scale 0xc00 */
};

#endif
