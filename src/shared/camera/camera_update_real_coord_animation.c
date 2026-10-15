#include "fft/battle_camera.h"

/* Advances the camera-position move by one frame while its countdown runs. Only
 * the uncalled camera_set_real_coord_delta_* routines start the countdown,
 * so nothing moves in retail. */
void camera_update_real_coord_animation(void) {
    if (g_camera_real_coord_countdown != 0) {
        g_camera_real_coord_countdown--;
        g_camera_current_real_coords.vx += g_camera_real_coord_delta.vx;
        g_camera_current_real_coords.vy += g_camera_real_coord_delta.vy;
        g_camera_current_real_coords.vz += g_camera_real_coord_delta.vz;
    }
}
