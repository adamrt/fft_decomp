#include "fft/battle_camera.h"

/* Doubles the cursor tile vector computed by
 * battle_camera_calculate_cursor_tile_vector_normal and passes that routine's
 * result through; callers hand it to battle_camera_step_focus_toward_cursor_tile. */
s32 battle_camera_scale_cursor_tile_vector(void) {
    s32 result = battle_camera_calculate_cursor_tile_vector_normal();

    g_battle_current_vector.vx *= 2;
    g_battle_current_vector.vy *= 2;
    g_battle_current_vector.vz *= 2;
    return result;
}
