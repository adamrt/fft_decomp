#include "fft/battle.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_camera_handle_rotation_input(void) {
    if (g_battle_camera_rotation_action == 0) {
        if (g_controller_input_pressed & PSX_PAD_L1) {
            battle_camera_call_rotate_left((g_battle_camera_render_state.vy & 0xFE00) - 0x400);
            main_sound_play_sfx_find_channel(0xB);
        }
        if (g_controller_input_pressed & PSX_PAD_R1) {
            battle_camera_call_rotate_right((g_battle_camera_render_state.vy & 0xFE00) + 0x400);
            main_sound_play_sfx_find_channel(0xC);
        }
    }
}
