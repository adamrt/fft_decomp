#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_request_rotation(s32 angle) {
    if (g_battle_camera_rotation_action == 0) {
        if (angle < 0) {
            battle_camera_call_rotate_left((g_battle_camera_render_state.vy & 0xFE00) + angle);
            main_sound_play_sfx_find_channel(0xB);
        } else {
            battle_camera_call_rotate_right((g_battle_camera_render_state.vy & 0xFE00) + angle);
            main_sound_play_sfx_find_channel(0xC);
        }
    }
}
