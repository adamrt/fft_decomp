#include "fft/battle_camera.h"

/* Saves the camera angles for camera_restore_rotation (effect script
 * opcodes 0c and 0d). */
void camera_save_rotation(void) {
    copyVector(&g_camera_rotation_backup, &g_camera_render_state);
}
