#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_update_zoom(void) {
    s32 increment;
    s32* zoom_ptr;

    if (g_battle_camera_zoom_action == 2 && g_battle_map_zoom_target == 1) {
        s32 zoom;
        s32 below;
        s32 pan_x;
        s32 pan_y;

        zoom_ptr = &g_battle_camera_zoom.vx;
        zoom = *zoom_ptr;
        increment = g_battle_camera_zoom_increment;
        zoom = zoom + increment;
        *zoom_ptr = zoom;
        below = zoom < ONE;
        pan_x = g_battle_camera_zoom.vy;
        pan_y = g_battle_camera_zoom.vz;
        pan_x = pan_x + increment;
        pan_y = pan_y + increment;
        g_battle_camera_zoom.vy = pan_x;
        g_battle_camera_zoom.vz = pan_y;
        if (!below) {
            g_battle_camera_zoom_action = 0;
            g_battle_camera_zoom.vz = ONE;
            g_battle_camera_zoom.vy = ONE;
            *zoom_ptr = ONE;
        }
    } else if (g_battle_camera_zoom_action == 4 && g_battle_map_zoom_target == g_battle_camera_zoom_action) {
        s32 zoom;
        s32 below;

        zoom_ptr = &g_battle_camera_zoom.vx;
        zoom = *zoom_ptr;
        increment = g_battle_camera_zoom_increment;
        zoom = zoom - increment;
        *zoom_ptr = zoom;
        below = zoom < 0xC01;
        g_battle_camera_zoom.vy -= increment;
        g_battle_camera_zoom.vz -= increment;
        if (below) {
            g_battle_camera_zoom_action = 0;
            g_battle_camera_zoom.vz = 0xC00;
            g_battle_camera_zoom.vy = 0xC00;
            *zoom_ptr = 0xC00;
        }
    } else {
        return;
    }
    if (increment < 0x300) {
        g_battle_camera_zoom_increment = g_animation_speed * 4 + increment;
    }
}
