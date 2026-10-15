#include "fft/battle_camera.h"

SVECTOR* camera_get_rotation(void) {
    return &g_camera_render_state;
}
