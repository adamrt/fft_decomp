#include "fft/battle_camera.h"

void camera_restore_current_real_coords(void) {
    copyVector(&g_camera_current_real_coords, &g_camera_saved_real_coords);
}
