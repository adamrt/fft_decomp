#include "fft/battle.h"
#include "fft/battle_camera.h"

/* Saves the screen offset for camera_restore_screen_offset. No
 * retail code calls either. */
void camera_save_screen_offset(void) {
    copyVector(&g_camera_saved_screen_offset, &g_camera_current_screen_offset);
}
