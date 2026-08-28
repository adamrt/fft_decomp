#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/main_runtime.h"

void battle_camera_update_tilt(void) {
    s16 tilt;

    if (g_battle_camera_tilt_action == 2 && g_battle_map_tilt_target == g_battle_camera_tilt_action) {
        u16* state = (u16*)&g_battle_camera_render_state.vx;
        tilt = *state + *(u16*)&g_battle_camera_tilt_increment;
        *state = tilt;
        if (tilt >= 0x1C0) {
            g_battle_camera_tilt_action = 0;
            *state = 0x1C0;
        }
    } else if (g_battle_camera_tilt_action == 4 && g_battle_map_tilt_target == 1) {
        u16* state = (u16*)&g_battle_camera_render_state.vx;
        tilt = *state - *(u16*)&g_battle_camera_tilt_increment;
        *state = tilt;
        if (tilt < 0x12F) {
            g_battle_camera_tilt_action = 0;
            *state = 0x12E;
        }
    } else if (g_battle_camera_tilt_action == 4 && g_battle_map_tilt_target == 2) {
        u16* state = (u16*)&g_battle_camera_render_state.vx;
        tilt = *state - *(u16*)&g_battle_camera_tilt_increment;
        *state = tilt;
        if (tilt < 0x1C1) {
            g_battle_camera_tilt_action = 0;
            *state = 0x1C0;
        }
    } else if (g_battle_camera_tilt_action == 2 && g_battle_map_tilt_target == 1) {
        u16* state = (u16*)&g_battle_camera_render_state.vx;
        tilt = *state + *(u16*)&g_battle_camera_tilt_increment;
        *state = tilt;
        if (tilt >= 0x12E) {
            g_battle_camera_tilt_action = 0;
            *state = 0x12E;
        }
    } else {
        return;
    }
    if (g_battle_camera_tilt_increment < 0x30) {
        g_battle_camera_tilt_increment += g_animation_speed * 2;
    }
}
