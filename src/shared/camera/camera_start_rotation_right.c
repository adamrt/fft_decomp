#include "fft/battle_camera.h"
#include "psx/types.h"

/* Start a right turn to target_angle from the slowest speed. */
void camera_start_rotation_right(s32 target_angle) {
    g_camera_rotation_action = CAMERA_ROTATION_RIGHT;
    g_camera_rotation_increment = 1;
    camera_rotate_right(target_angle);
}
