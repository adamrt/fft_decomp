#include "fft/battle.h"
#include "fft/battle_camera.h"

/* No retail code calls it. */
void camera_set_screen_offset(const VECTOR* coords) {
    copyVector(&g_camera_current_screen_offset, coords);
}
