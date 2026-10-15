#include "fft/battle_camera.h"

void camera_set_zoom(const VECTOR* zoom) {
    copyVector(&g_camera_zoom, zoom);
}
