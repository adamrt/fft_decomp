#include "fft/battle_camera.h"

/* Advances the camera-angle move by one frame while its countdown runs. Only the
 * uncalled camera_set_rotation_delta_* routines start the countdown, so
 * nothing moves in retail. */
void camera_update_rotation_animation(void) {
    if (g_camera_rotation_countdown != 0) {
        g_camera_rotation_countdown--;
        g_camera_render_state.vx += g_camera_rotation_delta.vx;
        g_camera_render_state.vy += g_camera_rotation_delta.vy;
        g_camera_render_state.vz += g_camera_rotation_delta.vz;
    }
}
