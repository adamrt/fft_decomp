#ifndef CAMERA_H
#define CAMERA_H

/* Camera routines and script camera state shared by the BATTLE and WORLD event
 * systems. Each scene links its own copy against its own camera state. */

#include "fft/gfx.h"
#include "psx/types.h"

/* script camera */
extern u16 g_camera_shake_z_offset;
extern s16 g_camera_diagonal_yaws[4];

void camera_clear_zoom_variable_low_bit(void);
void camera_fusion_thread(void);
void camera_init_tilt_and_zoom(void);
void camera_normalize_yaw_angle(void);
s16 camera_pick_nearest_unblocked_yaw(s32 runtime_id_a, s32 runtime_id_b);
s32 camera_step_track(camera_track_t* track, s32 component);
void camera_store_yaw_band(s32 value);
void camera_thread(void);
void camera_wait_until_idle(void);
s32 camera_wrap_yaw_angle(void);

#endif
