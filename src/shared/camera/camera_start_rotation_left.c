#include "fft/battle_camera.h"
#include "psx/types.h"

/* Start a left turn to target_angle from the slowest speed. */
void camera_start_rotation_left(s32 target_angle) {
    g_camera_rotation_action = CAMERA_ROTATION_LEFT;
    g_camera_rotation_increment = 1;
    camera_rotate_left(target_angle);
}
