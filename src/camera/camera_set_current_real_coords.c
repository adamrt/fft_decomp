#include "fft/battle_camera.h"

void camera_set_current_real_coords(const VECTOR* coords) {
    copyVector(&g_camera_current_real_coords, coords);
}
