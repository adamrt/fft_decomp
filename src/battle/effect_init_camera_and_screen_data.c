#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

void effect_init_camera_and_screen_data(void) {
    s32 pad[2];
    g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
    g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
    g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
    g_battle_unknown_801b8b1c = 0;
    g_battle_unknown_801b8b18 = 0;
    effect_copy_camera_angles_and_world_coords();
    (void)pad;
}
