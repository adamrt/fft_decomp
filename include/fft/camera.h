#ifndef CAMERA_H
#define CAMERA_H

/* Event library camera for BATTLE and WORLD: the script camera, its fusion tracks and the fixed-point math they
 * use. */

#include "psx/types.h"

/* script camera */
extern u16 g_camera_shake_z_offset;
extern u16 g_camera_position_locked;
/* The CameraSpeedCurve operand, unpacked with the EVENT_CAMERA_SPEED_CURVE_* masks. */
extern s32 g_camera_speed_curve;
extern s32 g_camera_wrap_yaw_pending;

void camera_thread(void);
void camera_wait_until_idle(void);
void camera_init_tilt_and_zoom(void);
void camera_clear_zoom_variable_low_bit(void);
void camera_normalize_yaw_angle(void);
void camera_store_yaw_band(s32 value);
s32 camera_wrap_yaw_angle(void);
void camera_store_state_to_script_variables(void);

/* fusion tracks */
/* A camera operand or key value that keeps the component's current value. */
enum {
    CAMERA_FUSION_HOLD_SENTINEL = 0x2710,
};

/* Provisional camera interpolation key: frame time and target value. */
typedef struct camera_key {
    s32 time;  /* 0x00; cumulative frame of this key */
    s32 value; /* 0x04 */
    u8 _unused_08[8];
} camera_key_t;
typedef char camera_key_size_must_be_16[sizeof(camera_key_t) == 16 ? 1 : -1];

/* Provisional per-component camera fusion track (0xa4 bytes), seeded by
 * camera_fusion_thread and advanced by its camera_step_track. */
typedef struct camera_track {
    camera_key_t keys[8]; /* 0x00 */
    s32 key_count;        /* 0x80 */
    s32 segment;          /* 0x84; first key of the active segment */
    s32 frame;            /* 0x88; frames since the track started */
    s32 segment_frame;    /* 0x8c; frames into the active segment */
    s32 prev_time;        /* 0x90; Q12 */
    s32 prev_value;       /* 0x94; Q12 */
    s32 start_value;      /* 0x98; Q12 */
    s32 segment_duration; /* 0x9c */
    s32 finished;         /* 0xa0 */
} camera_track_t;
typedef char camera_track_size_must_be_164[sizeof(camera_track_t) == 164 ? 1 : -1];

void camera_fusion_thread(void);

/* fixed-point math */
extern s32 g_camera_cross_product_overflow; /* set by the Q12 cross product on overflow */

s32 mul_div_s64(s32 a, s32 b, s32 c);
s32 fixed_cross_product_q12(s32 a, s32 b, s32 c, s32 d);

#endif
