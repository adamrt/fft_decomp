#include "fft/battle_camera.h"

void camera_set_rotation(const SVECTOR* rotation) {
    copyVector(&g_camera_render_state, rotation);
}
