#include "fft/battle_camera.h"

/* End free map panning: the camera follows the cursor tile again. */
void camera_disable_map_pan(void) {
    g_camera_map_pan_enabled = 0;
}
