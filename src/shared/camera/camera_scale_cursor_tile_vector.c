#include "fft/battle_action.h"
#include "fft/battle_camera.h"

/* Doubles the cursor tile vector computed by
 * camera_calculate_cursor_tile_vector_normal and passes that routine's
 * result through; callers hand it to camera_step_focus_toward_cursor_tile. */
s32 camera_scale_cursor_tile_vector(void) {
    s32 result = camera_calculate_cursor_tile_vector_normal();

    g_camera_step_vector.vx *= 2;
    g_camera_step_vector.vy *= 2;
    g_camera_step_vector.vz *= 2;
    return result;
}
