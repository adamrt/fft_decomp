#include "fft/battle.h"
#include "psx/types.h"

/* The retail routine preserves the ambient a1 value on the state-1 path. */
void battle_camera_toggle_tilt(s32 unused_0, s32 sound_arg) {
    s32 target;

    target = g_battle_map_tilt_target;
    if (target == 1) {
        g_battle_camera_tilt_action = 2;
        g_battle_map_tilt_target = 2;
    } else {
        sound_arg = 2;
        if (target == 2) {
            g_battle_map_tilt_target = 1;
            g_battle_camera_tilt_action = 4;
        } else if (target == 3) {
            g_battle_camera_tilt_action = 4;
            g_battle_map_tilt_target = 4;
        } else if (target == 4) {
            g_battle_map_tilt_target = 1;
            g_battle_camera_tilt_action = 2;
        }
    }
    g_battle_camera_tilt_increment = 1;
    /* The target passes a second argument the one-parameter callee ignores. */
    ((void (*)(s32, s32))main_sound_play_sfx)(0x31, sound_arg);
}
