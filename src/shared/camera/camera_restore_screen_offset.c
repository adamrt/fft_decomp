#include "fft/battle.h"
#include "fft/battle_camera.h"

/* No retail code calls it. */
void camera_restore_screen_offset(void) {
    copyVector(&g_camera_current_screen_offset, &g_camera_saved_screen_offset);
}
