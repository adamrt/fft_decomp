#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_handle_tilt_request(s32 mode) {
    switch (mode) {
    case 0:
        if (g_battle_camera_render_state.vx >= 0x12F) {
            g_battle_map_tilt_target = 1;
            g_battle_camera_tilt_action = 4;
            g_battle_camera_tilt_increment = 1;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        } else if (g_battle_camera_render_state.vx < 0x12E) {
            g_battle_map_tilt_target = 1;
            g_battle_camera_tilt_action = 2;
            g_battle_camera_tilt_increment = 1;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        }
        break;
    case 1:
        if (g_battle_camera_render_state.vx >= 0x1C1) {
            g_battle_map_tilt_target = 2;
            g_battle_camera_tilt_action = 4;
            g_battle_camera_tilt_increment = mode;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        } else if (g_battle_camera_render_state.vx < 0x1C0) {
            g_battle_camera_tilt_action = 2;
            g_battle_map_tilt_target = 2;
            g_battle_camera_tilt_increment = mode;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        }
        break;
    }
}
