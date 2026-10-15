#include "fft/battle_camera.h"

void camera_restore_zoom(void) {
    copyVector(&g_camera_zoom, &g_camera_zoom_backup);
}
