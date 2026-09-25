#include "fft/battle.h"
#include "psx/types.h"

s32 battle_camera_is_active(void) {
    /* Pin: unpinned, GCC builds the result in $v1 and adds a trailing
     * `move v0,v1`; early-return and split-if forms do the same. */
    register s32 result asm("$2");

    result = 1;
    if (g_battle_current_vector.vx == 0 && g_battle_current_vector.vy == 0 && g_battle_current_vector.vz == 0
        && g_battle_camera_rotation_action == 0 && g_battle_camera_tilt_action == 0) {
        result = g_battle_camera_zoom_action != 0;
    }

    return result;
}
