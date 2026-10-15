#include "fft/battle.h"
#include "fft/battle_camera.h"

/* No retail code calls it. */
VECTOR* camera_get_screen_offset(void) {
    return &g_camera_current_screen_offset;
}
