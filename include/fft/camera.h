#ifndef CAMERA_H
#define CAMERA_H

/* Camera routines and script camera state shared by the BATTLE and WORLD event
 * systems. Each scene links its own copy against its own camera state. */

#include "psx/types.h"

/* script camera */
extern u16 g_camera_shake_z_offset;
extern s16 g_camera_diagonal_yaws[4];

void camera_clear_zoom_variable_low_bit(void);
void camera_fusion_thread(void);
void camera_init_tilt_and_zoom(void);
void camera_normalize_yaw_angle(void);
void camera_store_yaw_band(s32 value);
void camera_wait_until_idle(void);
s32 camera_wrap_yaw_angle(void);

/* camera state */
extern u16 g_camera_position_locked;
/* Camera consumers unpack two 2-bit fields and a high nibble. */
extern s32 g_camera_speed_curve;
extern s32 g_camera_cross_product_overflow; /* set by the Q12 cross product on overflow */
extern s32 g_camera_wrap_yaw_pending;

/* fixed-point math */
s32 mul_div_s64(s32 a, s32 b, s32 c);
s32 fixed_cross_product_q12(s32 a, s32 b, s32 c, s32 d);

/* per-module bodies: each module links its own copy under the shared name. */
void camera_store_state_to_script_variables(void);

/* shared with the per-program event interpreter and dialog thread */
void camera_thread(void);

#endif
