#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "psx/types.h"

/* Set the destination of a left turn (falling yaw) and record the yaw it
 * leaves. A destination on a map axis (a multiple of 0x400) is moved 0x200
 * back, so the camera always comes to rest on a diagonal. */
void camera_rotate_left(s32 target_angle) {
    g_battle_previous_camera_yaw = g_camera_render_state.vy & 0xfe00;
    g_camera_yaw_target = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_camera_yaw_target = target_angle + 0x200;
    }
}
