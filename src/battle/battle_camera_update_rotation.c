#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_camera_update_rotation(void) {
    s32 angle;

    if (g_battle_camera_rotation_action == 1) {
        u16* yaw = (u16*)&g_battle_camera_render_state.vy;
        *yaw -= g_battle_camera_rotation_increment;
        if ((s16)*yaw <= g_battle_destination_camera_yaw) {
            angle = (u16)g_battle_destination_camera_yaw & 0xFFF;
            *yaw = angle;
            if (g_controller_input_previous & PSX_PAD_L1) {
                if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
                    battle_camera_rotate_left((angle & 0xFE0) - 0x400);
                }
            } else {
                g_battle_camera_rotation_action = 0;
                main_sound_stop_sfx(0xC);
                main_sound_stop_sfx(0xB);
                main_sound_play_sfx_find_channel(0xD);
            }
        }
    } else if (g_battle_camera_rotation_action == 2) {
        u16* yaw = (u16*)&g_battle_camera_render_state.vy;
        *yaw += g_battle_camera_rotation_increment;
        if ((s16)*yaw >= g_battle_destination_camera_yaw) {
            angle = (u16)g_battle_destination_camera_yaw & 0xFFF;
            *yaw = angle;
            if (g_controller_input_previous & PSX_PAD_R1) {
                if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
                    battle_camera_rotate_left((angle & 0xFE0) + 0x400);
                }
            } else {
                g_battle_camera_rotation_action = 0;
                main_sound_stop_sfx(0xC);
                main_sound_stop_sfx(0xB);
                main_sound_play_sfx_find_channel(0xD);
            }
        }
    } else {
        return;
    }
    if (g_battle_camera_rotation_increment < 0x30) {
        g_battle_camera_rotation_increment = (g_animation_speed * 2) + g_battle_camera_rotation_increment;
    }
}
