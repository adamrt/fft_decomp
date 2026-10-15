#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "psx/types.h"

/* 1 while the camera is moving: a pan vector is left, or it is rotating,
 * tilting or zooming. */
s32 camera_is_active(void) {
    if (g_camera_step_vector.vx == 0 && g_camera_step_vector.vy == 0 && g_camera_step_vector.vz == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
        && g_camera_zoom_action == CAMERA_STEP_NONE) {
        return 0;
    }
    return 1;
}
