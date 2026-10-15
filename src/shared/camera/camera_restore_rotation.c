#include "fft/battle_camera.h"

void camera_restore_rotation(void) {
    copyVector(&g_camera_render_state, &g_camera_rotation_backup);
}
