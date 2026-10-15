#include "fft/battle_camera.h"

/* Saves the camera zoom for camera_restore_zoom (effect script opcodes 0f
 * and 24). */
void camera_save_zoom(void) {
    copyVector(&g_camera_zoom_backup, &g_camera_zoom);
}
