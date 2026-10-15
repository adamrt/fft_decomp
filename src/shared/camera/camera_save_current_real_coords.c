#include "fft/battle_camera.h"

/* Saves the camera position for camera_restore_current_real_coords (effect
 * script opcodes 09 and 0a). */
void camera_save_current_real_coords(void) {
    copyVector(&g_camera_saved_real_coords, &g_camera_current_real_coords);
}
