#include "fft/battle.h"

void battle_camera_update_map_pan(void) {
    s32* coord;
    s32 limit;

    if (g_battle_state_game_flow_running != 0) {
        /* The target passes only the mode; the definition's second parameter is a matching device. */
        switch (((s32 (*)(s32))battle_camera_get_input_direction)(0)) {
        case 0x800:
            coord = &g_battle_camera_current_real_coords.vz;
            *coord += 0x4000;
            break;
        case 0:
            coord = &g_battle_camera_current_real_coords.vz;
            *coord -= 0x4000;
            break;
        case 0x400:
            coord = &g_battle_camera_current_real_coords.vx;
            *coord -= 0x4000;
            break;
        case 0xC00:
            coord = &g_battle_camera_current_real_coords.vx;
            *coord += 0x4000;
            break;
        case 0xA00: {
            s32* y = &g_battle_camera_current_real_coords.vz;
            *y += 0x4000;
            g_battle_camera_current_real_coords.vx += 0x4000;
            break;
        }
        case 0x600: {
            s32* y = &g_battle_camera_current_real_coords.vz;
            *y += 0x4000;
            g_battle_camera_current_real_coords.vx -= 0x4000;
            break;
        }
        case 0xE00: {
            s32* y = &g_battle_camera_current_real_coords.vz;
            *y -= 0x4000;
            g_battle_camera_current_real_coords.vx += 0x4000;
            break;
        }
        case 0x200: {
            s32* y = &g_battle_camera_current_real_coords.vz;
            *y -= 0x4000;
            g_battle_camera_current_real_coords.vx -= 0x4000;
            break;
        }
        }
        {
            s32* x = &g_battle_camera_current_real_coords.vx;
            limit = (g_map_max_x * 0x1C000) + 0x70000;
            if (limit < *x)
                *x = limit;
            if (*x < -0x70000)
                *x = -0x70000;
        }
        limit = (g_map_max_y * 0x1C000) + 0x70000;
        if (limit < g_battle_camera_current_real_coords.vz)
            g_battle_camera_current_real_coords.vz = limit;
        if (g_battle_camera_current_real_coords.vz < -0x70000)
            g_battle_camera_current_real_coords.vz = -0x70000;
    }
}
